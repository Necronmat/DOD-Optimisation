// DODVisualisation.cpp: A program using the TL-Engine

#include <Windows.h>
#include <TL-Engine.h>	// TL-Engine include file and namespace
#include <iostream>
#include <ctime>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <string>
using namespace tle;

//---------------------------------------------------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------------------------------------------------
const int CIRCLE_NUM = 25000;
const int MOVING_NUM = CIRCLE_NUM / 2;
const int STATIONARY_NUM = CIRCLE_NUM - MOVING_NUM;

const int MAX_X = 1000;
const int MAX_Y = 1000;
const int MIN_X = -1000;
const int MIN_Y = -1000;

const int WALL_MAX_X = MAX_X + MAX_X / 10;
const int WALL_MAX_Y = MAX_Y + MAX_Y / 10;
const int WALL_MIN_X = MIN_X + MIN_X / 10;
const int WALL_MIN_Y = MIN_Y + MIN_Y / 10;

const int MAXVEL_X = 50;
const int MAXVEL_Y = 50;
const int MINVEL_X = -50;
const int MINVEL_Y = -50;

const int MAX_RAD = 5;
const int MIN_RAD = 1;

//Options
const bool VISUALIZER = true;
const bool DEATH = false;
const bool WALLS = true;
const bool RANDRADIUS = true;

//---------------------------------------------------------------------------------------------------------------------
// Circle Data
//---------------------------------------------------------------------------------------------------------------------

struct Circle
{
    float rad;
    float x;
    float y;

    Circle()
    {
        if (RANDRADIUS)
        {
            rad = MIN_RAD + (std::rand() % (MAX_RAD - MIN_RAD + 1));
        }
        else
        {
            rad = 1;
        }
        
        x = MIN_X + (std::rand() % (MAX_X - MIN_X + 1));
        y = MIN_Y + (std::rand() % (MAX_Y - MIN_Y + 1));

    };
};

struct CircleVelocity
{
    float x;
    float y;

    CircleVelocity()
    {
        x = MINVEL_X + (std::rand() % (MAXVEL_X - MINVEL_X + 1));
        y = MINVEL_Y + (std::rand() % (MAXVEL_Y - MINVEL_Y + 1));
    };

};

struct CircleCollisionData
{
    std::string name;
    int hp;

    CircleCollisionData() 
    {
        name = 'a' + rand() % 26;
        name.push_back('a' + rand() % 26); name.push_back('a' + rand() % 26); name.push_back('a' + rand() % 26); name.push_back('a' + rand() % 26); name.push_back('a' + rand() % 26); name.push_back('a' + rand() % 26); name.push_back('a' + rand() % 26); name.push_back('a' + rand() % 26); name.push_back('a' + rand() % 26);
        hp = 100;
    };
};

struct CircleColourData
{
    float r;
    float g;
    float b;

    CircleColourData()
    {
        r = (std::rand() % 255) / 255.0f;
        g = (std::rand() % 255) / 255.0f;
        b = (std::rand() % 255) / 255.0f;
    };
};

bool CircleSorter(Circle const& lhs, Circle const& rhs)
{
    return lhs.x < rhs.x;
}

Circle movingCircles[MOVING_NUM];
CircleVelocity movingVelocitys[MOVING_NUM];
CircleCollisionData movingCollisions[MOVING_NUM];
CircleColourData movingColours[MOVING_NUM];

Circle stationaryCircles[STATIONARY_NUM];
CircleCollisionData stationaryCollisions[STATIONARY_NUM];
CircleColourData stationaryColours[STATIONARY_NUM];

auto start = std::chrono::steady_clock::now();

//---------------------------------------------------------------------------------------------------------------------
// Thread Pools
//---------------------------------------------------------------------------------------------------------------------

// A worker thread wakes up when work is signalled to be ready, and signals back when the work is complete.
// Same condition variable is used for signalling in both directions.
// A mutex is used to guard data shared between threads
struct WorkerThread
{
    std::thread             thread;
    std::condition_variable workReady;
    std::mutex              lock;
};

// Data describing work to do by a worker thread - this task is collision detection between some sprites against some blockers
struct CollisionWork
{
    bool complete = true;
    float frametime;
    uint32_t numMoving;
    Circle* moving;
    CircleVelocity* movingVel;
    CircleCollisionData* movingCollision;
    uint32_t numStationary;
    Circle* stationary;
    CircleCollisionData* stationaryCollision;
    std::vector<std::string> output;
};

struct ModelWork
{
    bool complete = true;
    uint32_t numMoving;
    Circle* moving;
    CircleVelocity* movingVel;
    CircleCollisionData* movingCollision;
    IModel** movingModel;
    uint32_t numStationary;
    Circle* stationary;
    CircleCollisionData* stationaryCollision;
    IModel** stationaryModel;

};

static const uint32_t MAX_WORKERS = 31;
std::pair<WorkerThread, CollisionWork> collisionWorkers[MAX_WORKERS];
std::pair<WorkerThread, ModelWork> modelWorkers[MAX_WORKERS];
uint32_t mNumWorkers;  // Actual number of worker threads being used in array above

//Vector for collision message output
std::vector<std::string> collisionsOutput[MAX_WORKERS];

//---------------------------------------------------------------------------------------------------------------------
// Functions
//---------------------------------------------------------------------------------------------------------------------

//Multithreaded method for checking if circles collide with circles with text output
void CheckCircleCollision(float frametime, uint32_t numMoving, Circle* moving, CircleVelocity* movingVel, CircleCollisionData* movingCollision, uint32_t numStationary, Circle* stationary, CircleCollisionData* stationaryCollision, std::vector<std::string>& collisions)
{
    auto movingEnd = moving + numMoving;
    auto stationaryEnd = stationary + numStationary;
    auto stationaryCollisionEnd = stationaryCollision + numStationary;

    while (moving != movingEnd)
    {
        float mvelx = movingVel->x;
        float mvely = movingVel->y;

        moving->x += mvelx * frametime;
        moving->y += mvely * frametime;

        float mx = moving->x;
        float my = moving->y;

        float mrad = moving->rad;

        float mradx = moving->x - mrad;
        float mxrad = moving->x + mrad;

        //Binary search
        auto s = stationary;
        auto e = stationaryEnd;
        auto sC = stationaryCollision;
        auto eC = stationaryCollisionEnd;
        Circle* mid;
        CircleCollisionData* midC;
        bool found = false;
        do
        {
            mid = s + (e - s) / 2;
            midC = sC + (eC - sC) / 2;

            float midradx = mid->x - mid->rad;
            float midxrad = mid->x + mid->rad;

            if (mxrad <= midradx)
            {
                e = mid;
                eC = midC;
            }
            else if (mradx >= midxrad)
            {
                s = mid;
                sC = midC;
            }
            else found = true;
        } while (!found && e - s > 1);

        // If no overlapping x-range found then no collision
        if (found)
        {
            bool collided = false;

            // Search from the stationary found in the strip, in a rightwards direction, until outside strip or end of list
            auto currentstationary = mid;
            auto currentstationaryCollision = midC;
            while (mxrad > currentstationary->x - currentstationary->rad && currentstationary != stationaryEnd)
            {
                float sx = currentstationary->x;
                float sy = currentstationary->y;

                float srad = currentstationary->rad;

                float mx_sx = sx - mx;
                float my_sy = sy - my;

                float dist = sqrt((mx_sx * mx_sx) + (my_sy * my_sy));

                if (dist < mrad + srad)
                {
                    //Move the circle so it is no longer colliding
                    float moveddist;
                    do
                    {
                        moving->x -= mvelx * 1.1f * frametime;
                        moving->y -= mvely * 1.1f * frametime;

                        float movedmx_sx = sx - moving->x;
                        float movedmy_sy = sy - moving->y;

                        moveddist = sqrt((movedmx_sx * movedmx_sx) + (movedmy_sy * movedmy_sy));
                    } while (moveddist < mrad + srad);

                    //Refeclt velocity of the moving circle
                    float normx = sx - mx;
                    float normy = sy - my;
                    float mag = sqrt((normx * normx) + (normy * normy));
                    normx /= mag;
                    normy /= mag;
                    float dot = (mvelx * normx) + (mvely * normy);

                    movingVel->x = mvelx - normx * (dot * 2.0f);
                    movingVel->y = mvely - normy * (dot * 2.0f);

                    movingCollision->hp -= 20;
                    currentstationaryCollision->hp -= 20;

                    auto end = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

                    collisions.push_back("Collision at " + std::to_string(elapsed.count()) + " microseconds: " + movingCollision->name + " - " + std::to_string(movingCollision->hp) + " " + currentstationaryCollision->name + " - " + std::to_string(currentstationaryCollision->hp));

                    collided = true;
                    break;
                }
                ++currentstationary;
                ++currentstationaryCollision;
            }

            // Search from the stationary found in the strip, in a lefttwards direction, until outside strip or end of list
            if (!collided)
            {
                currentstationary = mid;
                currentstationaryCollision = midC;
                while (currentstationary-- != stationary && currentstationaryCollision-- != stationaryCollision && mradx < currentstationary->x + currentstationary->rad)
                {
                    float sx = currentstationary->x;
                    float sy = currentstationary->y;

                    float srad = currentstationary->rad;

                    float mx_sx = sx - mx;
                    float my_sy = sy - my;

                    float dist = sqrt((mx_sx * mx_sx) + (my_sy * my_sy));

                    if (dist < mrad + srad)
                    {
                        //Move the circle so it is no longer colliding
                        float moveddist;
                        do
                        {
                            moving->x -= mvelx * 1.1f * frametime;
                            moving->y -= mvely * 1.1f * frametime;

                            float movedmx_sx = sx - moving->x;
                            float movedmy_sy = sy - moving->y;

                            moveddist = sqrt((movedmx_sx * movedmx_sx) + (movedmy_sy * movedmy_sy));
                        } while (moveddist < mrad + srad);

                        //Refeclt velocity of the moving circle
                        float normx = sx - mx;
                        float normy = sy - my;
                        float mag = sqrt((normx * normx) + (normy * normy));
                        normx /= mag;
                        normy /= mag;
                        float dot = (mvelx * normx) + (mvely * normy);

                        movingVel->x = mvelx - normx * (dot * 2.0f);
                        movingVel->y = mvely - normy * (dot * 2.0f);

                        movingCollision->hp -= 20;
                        currentstationaryCollision->hp -= 20;

                        auto end = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

                        collisions.push_back("Collision at " + std::to_string(elapsed.count()) + " microseconds: " + movingCollision->name + " - " + std::to_string(movingCollision->hp) + " " + currentstationaryCollision->name + " - " + std::to_string(currentstationaryCollision->hp));

                        break;
                    }
                }
            }
        }
        ++moving;
        ++movingVel;
        ++movingCollision;
    }
}

//Multithreaded method for checking if circles collide with circles
void CheckCircleCollision(float frametime, uint32_t numMoving, Circle* moving, CircleVelocity* movingVel, CircleCollisionData* movingCollision, uint32_t numStationary, Circle* stationary, CircleCollisionData* stationaryCollision)
{
    auto movingEnd = moving + numMoving;
    auto stationaryEnd = stationary + numStationary;
    auto stationaryCollisionEnd = stationaryCollision + numStationary;

    while (moving != movingEnd)
    {
        float mvelx = movingVel->x;
        float mvely = movingVel->y;

        moving->x += mvelx * frametime;
        moving->y += mvely * frametime;

        float mx = moving->x;
        float my = moving->y;

        float mrad = moving->rad;

        float mradx = moving->x - mrad;
        float mxrad = moving->x + mrad;

        //Binary search
        auto s = stationary;
        auto e = stationaryEnd;
        auto sC = stationaryCollision;
        auto eC = stationaryCollisionEnd;
        Circle* mid;
        CircleCollisionData* midC;
        bool found = false;
        do
        {
            mid = s + (e - s) / 2;
            midC = sC + (eC - sC) / 2;

            float midradx = mid->x - mid->rad;
            float midxrad = mid->x + mid->rad;

            if (mxrad <= midradx)
            {
                e = mid;
                eC = midC;
            }
            else if (mradx >= midxrad)
            {
                s = mid;
                sC = midC;
            }
            else found = true;
        } while (!found && e - s > 1);

        // If no overlapping x-range found then no collision
        if (found)
        {
            bool collided = false;

            // Search from the stationary found in the strip, in a rightwards direction, until outside strip or end of list
            auto currentstationary = mid;
            auto currentstationaryCollision = midC;
            while (mxrad > currentstationary->x - currentstationary->rad && currentstationary != stationaryEnd)
            {
                float sx = currentstationary->x;
                float sy = currentstationary->y;

                float srad = currentstationary->rad;

                float mx_sx = sx - mx;
                float my_sy = sy - my;

                float dist = sqrt((mx_sx * mx_sx) + (my_sy * my_sy));

                if (dist < mrad + srad)
                {
                    //Move the circle so it is no longer colliding
                    float moveddist;
                    do
                    {
                        moving->x -= mvelx * 1.1f * frametime;
                        moving->y -= mvely * 1.1f * frametime;

                        float movedmx_sx = sx - moving->x;
                        float movedmy_sy = sy - moving->y;

                        moveddist = sqrt((movedmx_sx * movedmx_sx) + (movedmy_sy * movedmy_sy));
                    } while (moveddist < mrad + srad);

                    //Refeclt velocity of the moving circle
                    float normx = sx - mx;
                    float normy = sy - my;
                    float mag = sqrt((normx * normx) + (normy * normy));
                    normx /= mag;
                    normy /= mag;
                    float dot = (mvelx * normx) + (mvely * normy);

                    movingVel->x = mvelx - normx * (dot * 2.0f);
                    movingVel->y = mvely - normy * (dot * 2.0f);

                    movingCollision->hp -= 20;
                    currentstationaryCollision->hp -= 20;
                    collided = true;
                    break;
                }
                ++currentstationary;
                ++currentstationaryCollision;
            }

            // Search from the stationary found in the strip, in a lefttwards direction, until outside strip or end of list
            if (!collided)
            {
                currentstationary = mid;
                currentstationaryCollision = midC;
                while (currentstationary-- != stationary && currentstationaryCollision-- != stationaryCollision && mradx < currentstationary->x + currentstationary->rad)
                {
                    float sx = currentstationary->x;
                    float sy = currentstationary->y;

                    float srad = currentstationary->rad;

                    float mx_sx = sx - mx;
                    float my_sy = sy - my;

                    float dist = sqrt((mx_sx * mx_sx) + (my_sy * my_sy));

                    if (dist < mrad + srad)
                    {
                        //Move the circle so it is no longer colliding
                        float moveddist;
                        do
                        {
                            moving->x -= mvelx * 1.1f * frametime;
                            moving->y -= mvely * 1.1f * frametime;

                            float movedmx_sx = sx - moving->x;
                            float movedmy_sy = sy - moving->y;

                            moveddist = sqrt((movedmx_sx * movedmx_sx) + (movedmy_sy * movedmy_sy));
                        } while (moveddist < mrad + srad);

                        //Refeclt velocity of the moving circle
                        float normx = sx - mx;
                        float normy = sy - my;
                        float mag = sqrt((normx * normx) + (normy * normy));
                        normx /= mag;
                        normy /= mag;
                        float dot = (mvelx * normx) + (mvely * normy);

                        movingVel->x = mvelx - normx * (dot * 2.0f);
                        movingVel->y = mvely - normy * (dot * 2.0f);

                        movingCollision->hp -= 20;
                        currentstationaryCollision->hp -= 20;
                        break;
                    }
                }
            }
        }
        ++moving;
        ++movingVel;
        ++movingCollision;
    }
}

//Multithreaded method for checking if circles collide with walls
void CheckWallCollision(uint32_t numMoving, Circle* moving, CircleVelocity* movingVel)
{
    auto movingEnd = moving + numMoving;

    while (moving != movingEnd)
    {
        float mvelx = movingVel->x;
        float mvely = movingVel->y;

        float mx = moving->x;
        float my = moving->y;

        float mrad = moving->rad;

        if (mx - mrad <= WALL_MIN_X)
        {
            //Move the circle so it is no longer colliding
            moving->x = WALL_MIN_X + mrad + 1.0f;

            //Refeclt velocity of the moving circle         
            movingVel->x = mvelx - 1.0f * (mvelx * 2.0f);
            movingVel->y = mvely - 0.0f * (mvelx * 2.0f);
        }
        else if (mx + mrad >= WALL_MAX_X)
        {
            //Move the circle so it is no longer colliding
            moving->x = WALL_MAX_X - mrad - 1.0f;

            //Refeclt velocity of the moving circle
            movingVel->x = mvelx + 1 * (-mvelx * 2.0f);
            movingVel->y = mvely - 0.0f * (-mvelx * 2.0f);
        }
        else if (my - mrad <= WALL_MIN_Y)
        {
            //Move the circle so it is no longer colliding
            moving->y = WALL_MIN_Y + mrad + 1.0f;

            //Refeclt velocity of the moving circle
            movingVel->x = mvelx - 0.0f * (mvely * 2.0f);
            movingVel->y = mvely - 1.0f * (mvely * 2.0f);
        }
        else if (my + mrad >= WALL_MAX_Y)
        {
            //Move the circle so it is no longer colliding
            moving->y = WALL_MAX_Y - mrad - 1.0f;

            //Refeclt velocity of the moving circle
            movingVel->x = mvelx - 0.0f * (-mvely * 2.0f);
            movingVel->y = mvely + 1.0f * (-mvely * 2.0f);
        }
        ++moving;
        ++movingVel;
    }
}

//Multithreaded method for moving models
void MoveModel(uint32_t numMoving, Circle* moving, IModel** movingModel)
{
    auto movingEnd = moving + numMoving;

    while (moving != movingEnd)
    {
        movingModel[0]->SetX(moving->x);
        movingModel[0]->SetY(moving->y);

        ++moving;
        ++movingModel;
    }
}

void DeathModel(uint32_t numMoving, Circle* moving, IModel** movingModel, CircleVelocity* movingVel, CircleCollisionData* movingCollision, uint32_t numStationary, Circle* stationary, CircleCollisionData* stationaryCollision, IModel** stationaryModel)
{
    auto movingEnd = moving + numMoving;

    while (moving != movingEnd)
    {
        if (movingCollision->hp <= 0)
        {
            moving->x = WALL_MAX_X + 999999999;
            moving->y = WALL_MAX_Y + 999999999;

            movingVel->x = 0;
            movingVel->y = 0;

            movingModel[0]->SetX(WALL_MAX_X + 999999999);
            movingModel[0]->SetY(WALL_MAX_Y + 999999999);
        }

        ++moving;
        ++movingVel;
        ++movingCollision;
        ++movingModel;
    }

    //auto stationaryEnd = stationary + numStationary;
    //while (stationary != stationaryEnd)
    //{
    //    if (stationaryCollision->hp <= 0)
    //    {
    //        stationary->x = WALL_MIN_X - 999999999;
    //        stationary->y = WALL_MIN_Y - 999999999;

    //        stationaryModel[0]->SetX(WALL_MIN_X - 999999999);
    //        stationaryModel[0]->SetY(WALL_MIN_Y - 999999999);
    //    }

    //    ++stationary;
    //    ++stationaryCollision;
    //    ++stationaryModel;
    //}
}

//*********************************************************
// Worker threads run this method
// The worker waits for work to be available, processes the work, then signals it is complete.
// It then returns to waiting. This thread is created at start-up time and destroyed at shutdown,
// because creating threads at runtime is too slow for this kind of game usage
void CollisionThread(uint32_t thread)
{
    auto& worker = collisionWorkers[thread].first;
    auto& work = collisionWorkers[thread].second;
    while (true)
    {
        {
            std::unique_lock<std::mutex> l(worker.lock);
            worker.workReady.wait(l, [&]() { return !work.complete; }); // Wait until a workReady signal arrives, then verify it by testing      
            // that work.complete is false. The test is required because there is
            // the possibility of "spurious wakeups": a false signal. Also in 
            // some situations other threads may have eaten the work already.
        }
        // We have some work so do it...
        if (VISUALIZER)
        {
            CheckCircleCollision(work.frametime, work.numMoving, work.moving, work.movingVel, work.movingCollision, work.numStationary, work.stationary, work.stationaryCollision);
        }
        else
        {
            CheckCircleCollision(work.frametime, work.numMoving, work.moving, work.movingVel, work.movingCollision, work.numStationary, work.stationary, work.stationaryCollision, work.output);
        }
        if (WALLS)
        {
            CheckWallCollision(work.numMoving, work.moving, work.movingVel);
        }
        {
            // Flag the work is complete
            // We also guard every normal access to shared variable "work.complete" with the same mutex
            std::unique_lock<std::mutex> l(worker.lock);
            work.complete = true;
        }
        // Send a signal back to the main thread to say the work is complete, loop back and wait for more work
        worker.workReady.notify_one();
    }
}

void ModelThread(uint32_t thread)
{
    auto& worker = modelWorkers[thread].first;
    auto& work = modelWorkers[thread].second;
    while (true)
    {
        {
            std::unique_lock<std::mutex> l(worker.lock);
            worker.workReady.wait(l, [&]() { return !work.complete; }); // Wait until a workReady signal arrives, then verify it by testing      
            // that work.complete is false. The test is required because there is
            // the possibility of "spurious wakeups": a false signal. Also in 
            // some situations other threads may have eaten the work already.
        }
        // We have some work so do it...
        if (DEATH)
        {
            DeathModel(work.numMoving, work.moving, work.movingModel, work.movingVel, work.movingCollision, work.numStationary, work.stationary, work.stationaryCollision, work.stationaryModel);
        }
        MoveModel(work.numMoving, work.moving, work.movingModel);

        {
            // Flag the work is complete
            // We also guard every normal access to shared variable "work.complete" with the same mutex
            std::unique_lock<std::mutex> l(worker.lock);
            work.complete = true;
        }
        // Send a signal back to the main thread to say the work is complete, loop back and wait for more work
        worker.workReady.notify_one();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Main game setup and loop
//---------------------------------------------------------------------------------------------------------------------

void main()
{
    // Create a 3D engine (using TLX engine here) and open a window for it
    I3DEngine* myEngine = New3DEngine(kTLX);
    myEngine->StartWindowed();

    // Add default folder for meshes and other media
    myEngine->AddMediaFolder("C:\\ProgramData\\TL-Engine\\Media");

    /**** Set up your scene here ****/
    ICamera* camera = myEngine->CreateCamera(kManual);
    camera->SetNearClip(0.001f);
    camera->SetFarClip(10000);

    camera->SetX(0);
    camera->SetY(0);
    camera->SetZ(-250);

    // Start worker threads
    mNumWorkers = std::thread::hardware_concurrency(); // Gives a hint about level of thread concurrency supported by system (0 means no hint given)
    if (mNumWorkers == 0)  mNumWorkers = 8;
    --mNumWorkers; // Decrease by one because this main thread is already running
    for (uint32_t i = 0; i < mNumWorkers; ++i)
    {
        // Start each worker thread running the CollisionThread method. Note the way to construct std::thread to run a member function
        collisionWorkers[i].first.thread = std::thread(&CollisionThread, i);
        modelWorkers[i].first.thread = std::thread(&ModelThread, i);
    }

    std::sort(stationaryCircles, stationaryCircles + STATIONARY_NUM, &CircleSorter);

    IMesh* ballMesh = myEngine->LoadMesh("PoolBall.x");
    IModel* movingModels[MOVING_NUM];
    IModel* stationaryModels[STATIONARY_NUM];
    if (VISUALIZER)
    {
        for (int i = 0; i < MOVING_NUM; ++i)
        {
            movingModels[i] = ballMesh->CreateModel(movingCircles[i].x, movingCircles[i].y, 0);
            movingModels[i]->Scale(movingCircles[i].rad * 0.05f);
            movingModels[i]->SetSkin("RedBall.jpg");
        }

        for (int i = 0; i < STATIONARY_NUM; ++i)
        {
            stationaryModels[i] = ballMesh->CreateModel(stationaryCircles[i].x, stationaryCircles[i].y, 0);
            stationaryModels[i]->Scale(stationaryCircles[i].rad * 0.05f);
            stationaryModels[i]->SetSkin("BlackBall.jpg");
        }
    }
    else
    {
        start = std::chrono::steady_clock::now();
    }

    myEngine->Timer();
    // The main game loop, repeat until engine is stopped
    int tickNum = 0;
    long totalTicktime = 0;
    while (myEngine->IsRunning() && !myEngine->KeyHeld(Key_Escape))
    {
        auto tickStart = std::chrono::steady_clock::now();
        float frameTime = myEngine->Timer();

        if (VISUALIZER)
        {
            // Draw the scene
            myEngine->DrawScene();

            if (myEngine->KeyHeld(Key_Q))  camera->MoveLocalZ(frameTime * std::abs(camera->GetZ()));
            if (myEngine->KeyHeld(Key_E))  camera->MoveLocalZ(-frameTime * std::abs(camera->GetZ()));
            if (myEngine->KeyHeld(Key_D))  camera->MoveLocalX(frameTime * std::abs(camera->GetZ()));
            if (myEngine->KeyHeld(Key_A))  camera->MoveLocalX(-frameTime * std::abs(camera->GetZ()));
            if (myEngine->KeyHeld(Key_W))  camera->MoveLocalY(frameTime * std::abs(camera->GetZ()));
            if (myEngine->KeyHeld(Key_S))  camera->MoveLocalY(-frameTime * std::abs(camera->GetZ()));
        }

        /**** Update your scene each frame here ****/

        auto movingStart = movingCircles;
        auto movingVelStart = movingVelocitys;
        auto movingCollisionStart = movingCollisions;
        for (uint32_t j = 0; j < mNumWorkers; ++j)
        {
            // Prepare a section of work (basically the parameters to the collision detection function)
            auto& work = collisionWorkers[j].second;
            work.frametime = frameTime;
            work.numMoving = MOVING_NUM / (mNumWorkers + 1);// Add one because this main thread will also do some work
            work.moving = movingStart;
            work.movingVel = movingVelStart;
            work.movingCollision = movingCollisionStart;
            work.numStationary = STATIONARY_NUM;
            work.stationary = stationaryCircles;
            work.stationaryCollision = stationaryCollisions;
            work.output = collisionsOutput[j];

            // Flag the work as not yet complete
            auto& workerThread = collisionWorkers[j].first;
            {
                // Guard every access to shared variable "work.complete" with a mutex (see BlockSpritesThread comments)
                std::unique_lock<std::mutex> l(workerThread.lock);
                work.complete = false;
            }

            // Notify the worker thread via a condition variable - this will wake the worker thread up
            workerThread.workReady.notify_one();

            movingStart += work.numMoving;
            movingVelStart += work.numMoving;
            movingCollisionStart += work.numMoving;
        }

        // This main thread will also do one section of the work
        uint32_t numRemaining = MOVING_NUM / (mNumWorkers + 1);
        if (VISUALIZER)
        {
            CheckCircleCollision(frameTime, numRemaining, movingCircles, movingVelocitys, movingCollisions, STATIONARY_NUM, stationaryCircles, stationaryCollisions);
        }
        else
        {
            CheckCircleCollision(frameTime, numRemaining, movingCircles, movingVelocitys, movingCollisions, STATIONARY_NUM, stationaryCircles, stationaryCollisions, collisionsOutput[mNumWorkers]);
        }
        if (WALLS)
        {
            CheckWallCollision(numRemaining, movingCircles, movingVelocitys);
        }

        // Wait for all the workers to finish
        for (uint32_t j = 0; j < mNumWorkers; ++j)
        {
            auto& workerThread = collisionWorkers[j].first;
            auto& work = collisionWorkers[j].second;

            // Wait for a signal via a condition variable indicating that the worker has finished the work
            // See comments in BlockSpritesThread regarding the mutex and the wait method
            std::unique_lock<std::mutex> l(workerThread.lock);
            workerThread.workReady.wait(l, [&]() { return work.complete; });
            collisionsOutput[j] = work.output;
        }

        if (VISUALIZER)
        {
            movingStart = movingCircles;
            movingVelStart = movingVelocitys;
            movingCollisionStart = movingCollisions;
            auto movingModelStart = movingModels;

            auto stationaryStart = stationaryCircles;
            auto stationaryCollisionStart = stationaryCollisions;
            auto stationaryModelStart = stationaryModels;
            for (uint32_t j = 0; j < mNumWorkers; ++j)
            {
                // Prepare a section of work (basically the parameters to the collision detection function)
                auto& work = modelWorkers[j].second;
                work.numMoving = MOVING_NUM / (mNumWorkers + 1);// Add one because this main thread will also do some work
                work.moving = movingStart;
                work.movingVel = movingVelStart;
                work.movingCollision = movingCollisionStart;
                work.movingModel = movingModelStart;
                work.numStationary = STATIONARY_NUM / (mNumWorkers + 1);// Add one because this main thread will also do some work;
                work.stationary = stationaryStart;
                work.stationaryCollision = stationaryCollisionStart;
                work.stationaryModel = stationaryModelStart;

                // Flag the work as not yet complete
                auto& workerThread = modelWorkers[j].first;
                {
                    // Guard every access to shared variable "work.complete" with a mutex (see BlockSpritesThread comments)
                    std::unique_lock<std::mutex> l(workerThread.lock);
                    work.complete = false;
                }

                // Notify the worker thread via a condition variable - this will wake the worker thread up
                workerThread.workReady.notify_one();

                movingStart += work.numMoving;
                movingVelStart += work.numMoving;
                movingCollisionStart += work.numMoving;
                movingModelStart += work.numMoving;

                stationaryStart += work.numStationary;
                stationaryCollisionStart += work.numStationary;
                stationaryModelStart += work.numStationary;
            }

            // This main thread will also do one section of the work
            uint32_t movingNumRemaining = MOVING_NUM / (mNumWorkers + 1);
            uint32_t stationaryNumRemaining = STATIONARY_NUM / (mNumWorkers + 1);
            if (DEATH)
            {
                DeathModel(movingNumRemaining, movingCircles, movingModels, movingVelocitys, movingCollisions, stationaryNumRemaining, stationaryCircles, stationaryCollisions, stationaryModels);
            }
            MoveModel(movingNumRemaining, movingCircles, movingModels);

            // Wait for all the workers to finish
            for (uint32_t j = 0; j < mNumWorkers; ++j)
            {
                auto& workerThread = modelWorkers[j].first;
                auto& work = modelWorkers[j].second;

                // Wait for a signal via a condition variable indicating that the worker has finished the work
                // See comments in BlockSpritesThread regarding the mutex and the wait method
                std::unique_lock<std::mutex> l(workerThread.lock);
                workerThread.workReady.wait(l, [&]() { return work.complete; });
            }
        }
        else
        {
            for (uint32_t i = 0; i < mNumWorkers + 1; ++i)
            {
                for (auto j : collisionsOutput[i])
                {
                    std::cout << j << std::endl;
                }

                collisionsOutput[i].clear();
            }

            if (DEATH)
            {
                for (int i = 0; i < MOVING_NUM; ++i)
                {
                    movingCircles[i].x = WALL_MAX_X + 999999999;
                    movingCircles[i].y = WALL_MAX_Y + 999999999;

                    movingVelocitys[i].x = 0;
                    movingVelocitys[i].y = 0;
                }

                //for (int i = 0; i < STATIONARY_NUM; ++i)
                //{
                //    stationaryCircles[i].x = WALL_MIN_X - 999999999;
                //    stationaryCircles[i].y = WALL_MIN_Y - 999999999;
                //}
            }

            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - tickStart);

            totalTicktime += elapsed.count();
            ++tickNum;
        }
    }

    if (!VISUALIZER)
    {
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Time taken: " << elapsed.count() << " microseconds" << std::endl;

        std::cout << "Average tick time: " << totalTicktime / tickNum << " microseconds" << std::endl;
    }

    // Delete the 3D engine now we are finished with it

    //*********************************************************
    // Running threads must be joined to the main thread or detached before their destruction. If not
    // the entire program immediately terminates (in fact the program is quiting here anyway, but if 
    // std::terminate is called we probably won't get proper destruction). The worker threads never
    // naturally exit so we can't use join. So in this case detach them prior to their destruction.
    for (uint32_t i = 0; i < mNumWorkers; ++i)
    {
        collisionWorkers[i].first.thread.detach();
        modelWorkers[i].first.thread.detach();
    }

    myEngine->Delete();
}