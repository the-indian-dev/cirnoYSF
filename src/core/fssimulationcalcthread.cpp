#include "fssimulationcalcthread.h"
#include "fssimulation.h"
#include "../common/fsdef.h"

FsSimulationCalculationThread::FsSimulationCalculationThread(FsSimulation *sim) :
    sim(sim),
    terminate(false),
    paused(false),
    dataReady(false),
    calculationDone(true),
    frameRateLimit(0),  // 0 means no limit, otherwise limits frames per second
    lastFrameTime(0)    // Time of last frame in milliseconds
{
}

FsSimulationCalculationThread::~FsSimulationCalculationThread()
{
    // Make sure the thread is stopped and joined
    if (calcThread.joinable())
    {
        terminate = true;
        
        // Wake up the thread if it's waiting
        {
            std::lock_guard<std::mutex> lock(paramsMutex);
            dataReady = true;
            dataReadyCV.notify_one();
        }
        
        calcThread.join();
    }
}

void FsSimulationCalculationThread::Start()
{
    if (!calcThread.joinable()) {
        terminate = false;
        paused = false;
        calcThread = std::thread(&FsSimulationCalculationThread::ThreadFunction, this);
    }
}

void FsSimulationCalculationThread::Stop()
{
    if (calcThread.joinable()) {
        terminate = true;
        
        // Wake up the thread if it's waiting
        {
            std::lock_guard<std::mutex> lock(paramsMutex);
            dataReady = true;
            dataReadyCV.notify_one();
        }
        
        calcThread.join();
    }
}

void FsSimulationCalculationThread::QueueCalculation(
    const double &deltaTime,
    YSBOOL demoMode,
    YSBOOL record,
    YSBOOL showTimer,
    YSBOOL networkStandby,
    FSUSERCONTROL userControl,
    YSBOOL showTimeMarker)
{
    if (!IsActive()) {
        return;
    }

    // Always update parameters - even if the thread is busy
    // This ensures we're always working with the latest state
    {
        std::lock_guard<std::mutex> lock(paramsMutex);
        params.deltaTime = deltaTime;
        params.demoMode = demoMode;
        params.record = record;
        params.showTimer = showTimer;
        params.networkStandby = networkStandby;
        params.userControl = userControl;
        params.showTimeMarker = showTimeMarker;
        
        // Mark ready for processing
        dataReady = true;
        
        // Only reset calculationDone if we know the thread can pick up the work
        if (calculationDone.load()) {
            calculationDone = false;
        }
    }
    
    // Wake up the calculation thread
    dataReadyCV.notify_all();
}

bool FsSimulationCalculationThread::WaitForCalculation(int timeoutMs)
{
    if (!IsActive()) {
        return true;
    }

    // Never wait - always return immediately
    // This is critical for ensuring the main thread is never blocked
    return true;
}

bool FsSimulationCalculationThread::IsActive() const
{
    return calcThread.joinable() && !terminate;
}

bool FsSimulationCalculationThread::IsCalculating() const
{
    return !calculationDone;
}

void FsSimulationCalculationThread::Pause()
{
    paused = true;
}

void FsSimulationCalculationThread::Resume()
{
    paused = false;
}

void FsSimulationCalculationThread::SetFrameRateLimit(int fps)
{
    frameRateLimit = fps;
}

std::mutex& FsSimulationCalculationThread::GetSimulationMutex()
{
    return simMutex;
}

void FsSimulationCalculationThread::ThreadFunction()
{
    while (!terminate) {
        // Sleep a tiny amount to prevent CPU hogging
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        
        // Wait for new calculation data
        std::unique_lock<std::mutex> lock(paramsMutex);
        if (!dataReady && !terminate) {
            // Use wait_for with short timeout to prevent permanent blocking
            dataReadyCV.wait_for(lock, std::chrono::milliseconds(1));
            
            // Re-check condition after wait
            if (!dataReady && !terminate) {
                lock.unlock();
                continue;
            }
        }
        
        if (terminate) {
            break;
        }
        
        // Reset data ready flag
        dataReady = false;
        
        // Copy params for this calculation
        SimulationParams currentParams = params;
        lock.unlock();
        
        // Skip calculation if paused
        if (paused) {
            calculationDone = true;
            calcDoneCV.notify_all();
            continue;
        }
        
        // Grab the sim mutex for minimal time - primarily for physics
        {
            // Never block if we can't get the lock immediately
            if (simMutex.try_lock()) {
                // Critical section - keep as short as possible 
                try {
                    // Avoid using user flight controls - just physics
                    sim->SimMove(currentParams.deltaTime);
                    
                    // Update sim time - this is key for keeping in sync
                    sim->currentTime += currentParams.deltaTime;
                    
                    // Only handle AI on appropriate schedules - never user input
                    if (sim->currentTime >= sim->nextControlTime) {
                        sim->SimControlByComputer(sim->currentTime - sim->lastControlledTime);
                        sim->lastControlledTime = sim->currentTime;
                        sim->nextControlTime = sim->currentTime + 0.025;
                    }
                    
                    // Weapons and explosions physics
                    sim->bulletHolder.PlayRecord(sim->CurrentTime(), currentParams.deltaTime);
                    sim->explosionHolder.PlayRecord(sim->CurrentTime(), currentParams.deltaTime);
                    
                    // Record flight data if needed
                    if (currentParams.record == YSTRUE) {
                        if (sim->currentTime > sim->nextAirRecordTime) {
                            sim->SimRecordAir(sim->currentTime, YSFALSE);
                            sim->nextAirRecordTime = sim->currentTime + 0.05;
                        }
                        if (sim->currentTime > sim->nextGndRecordTime) {
                            sim->SimRecordGnd(sim->currentTime, YSFALSE);
                            sim->nextGndRecordTime = sim->currentTime + 1.0;
                        }
                    }
                } catch (...) {
                    // Safety to prevent crashes
                }
                
                // Release lock immediately
                simMutex.unlock();
            }
        }
        
        // Mark as done regardless of whether we succeeded
        calculationDone = true;
        calcDoneCV.notify_all();
        
        // Always let the main thread have priority
        std::this_thread::yield();
    }
}