#include "fssimulationcalcthread.h"
#include "fssimulation.h"
#include "fs.h"
#include "fsconfig.h"
#include "../common/fsdef.h"
#include "../platform/common/fswindow.h"
#include "../common/fsairsound.h"
#include "fsexistence.h"

// FsSimulation method implementations for calculation thread

void FsSimulation::InitCalculationThread()
{
    if (calcThread == nullptr)
    {
        calcThread = new FsSimulationCalculationThread(this);
        // Lower thread priority to avoid impacting UI responsiveness
        calcThread->SetFrameRateLimit(0);
        calcThread->Start();
        
        // Give the thread time to initialize properly
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void FsSimulation::CleanupCalculationThread()
{
    if (calcThread != nullptr)
    {
        delete calcThread;
        calcThread = nullptr;
    }
}

// Implementation for the threaded simulation step
void FsSimulation::SimulateOneStepThreaded(
    const double &passedTime,
    YSBOOL demoMode, YSBOOL record, YSBOOL showTimer, YSBOOL networkStandby, FSUSERCONTROL userControl,
    YSBOOL showTimeMarker)
{
    if (NULL == firstPlayer.GetObject(this) && NULL != GetPlayerObject())
    {
        firstPlayer.SetObject(GetPlayerObject());
    }

    if (airTrafficSequence->GetNextUpdateTime() < currentTime)
    {
        airTrafficSequence->RefreshAirTrafficSlot(this);
    }
    airTrafficSequence->RefreshRunwayUsage(this);

    if (pause != YSTRUE)
    {
        double deltaTime = YsSmaller(passedTime, 3.0);

        // These tasks still run in the thread pool as before
        std::vector<std::function<void()>> taskArray;
        taskArray.push_back(std::bind(&FsSimulation::SimCacheFieldElevation, this));
        taskArray.push_back(std::bind(&FsSimulation::SimCacheRectRegion, this));
        taskArray.push_back(std::bind(&FsSimulation::SimComputeAirToObjCollision, this));
        threadPool.Run(taskArray.size(), taskArray.data());

        SimProcessCollisionAndTerrain(passedTime);

        const double stepTime = 0.025;
        
        // Initialize calculation thread if needed
        if (calcThread == nullptr)
        {
            InitCalculationThread();
        }

        // Main loop for simulation steps
        while (deltaTime > YsTolerance)
        {
            double dt;
            if (cfgPtr != nullptr && cfgPtr->accurateTime == YSTRUE)
            {
                dt = YsSmaller(deltaTime, stepTime);
            }
            else
            {
                dt = deltaTime;
            }

            // Move simulation one step forward
            if (calcThread != nullptr && calcThread->IsActive())
            {
                // Queue calculation if the thread isn't busy
                if (calcThread->calculationDone.load()) {
                    calcThread->QueueCalculation(dt, demoMode, record, showTimer, networkStandby, userControl, showTimeMarker);
                }
                // DO NOT wait for the calculation to complete - let it run asynchronously
                // This prevents main thread blocking and allows input to be processed
            }
            else
            {
                // Fallback if thread creation failed
                SimMove(dt);
                bulletHolder.PlayRecord(currentTime, dt);
                explosionHolder.PlayRecord(currentTime, dt);
                SimPlayTimedEvent(currentTime);
                currentTime += dt;
                
                if (currentTime >= nextControlTime)
                {
                    SimControlByComputer(currentTime - lastControlledTime);
                    lastControlledTime = currentTime;
                    nextControlTime = currentTime + 0.025;
                }
                
                if (demoMode != YSTRUE && networkStandby != YSTRUE && userControl != FSUSC_DISABLE)
                {
                    SimControlByUser(dt, userControl);
                }
                
                if (record == YSTRUE)
                {
                    if (currentTime > nextAirRecordTime)
                    {
                        SimRecordAir(currentTime, YSFALSE);
                        nextAirRecordTime = currentTime + 0.05;
                    }
                    if (currentTime > nextGndRecordTime)
                    {
                        SimRecordGnd(currentTime, YSFALSE);
                        nextGndRecordTime = currentTime + 1.0;
                    }
                }
            }
            
            deltaTime -= dt;
            
            // Always break after processing one step in threaded mode
            // This ensures that user input is processed every frame
            if (calcThread != nullptr && calcThread->IsActive()) {
                break;
            }
        }
    }

    // Always check state directly from current state
    // Don't rely on the results of the async calculation thread
    // since it could be behind by a frame
    FsAirplane *playerPlane = GetPlayerAirplane();
    if (playerPlane != NULL && playerPlane->IsAlive() == YSTRUE)
    {
        if (playerPlane->Prop().CheckHasJustTouchDown() == YSTRUE)
        {
            FsSoundSetOneTime(FSSND_ONETIME_TOUCHDWN);
        }

        if (ppGear != pGear)
        {
            ppGear = pGear;
        }
        if (pGear != Gear)
        {
            pGear = Gear;
        }
        Gear = playerPlane->Prop().GetLandingGear();
        if (ppGear <= pGear && pGear > Gear)
        {
            if (hideNextGearSound != YSTRUE)
            {
                FsSoundSetOneTime(FSSND_ONETIME_GEARUP);
            }
            else
            {
                hideNextGearSound = YSFALSE;
            }
        }
        if (ppGear >= pGear && pGear < Gear)
        {
            FsSoundSetOneTime(FSSND_ONETIME_GEARDOWN);
        }

        if (ppFlap != pFlap)
        {
            ppFlap = pFlap;
        }
        if (pFlap != Flap)
        {
            pFlap = Flap;
        }
        Flap = playerPlane->Prop().GetFlap();
    }

    // Need to set redraw flag so the scene gets redrawn next frame
    SetNeedRedraw(YSTRUE);
}
