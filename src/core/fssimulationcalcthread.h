#ifndef FSSIMULATIONCALCTHREAD_IS_INCLUDED
#define FSSIMULATIONCALCTHREAD_IS_INCLUDED
/* { */

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "fs.h"
#include "../common/fsdef.h"

// Forward declarations
class FsSimulation;

class FsSimulationCalculationThread
{
public:
    // Simulation parameters structure - passed between threads
    struct SimulationParams
    {
        double deltaTime;
        YSBOOL demoMode;
        YSBOOL record;
        YSBOOL showTimer;
        YSBOOL networkStandby;
        FSUSERCONTROL userControl;  // Using proper enum type
        YSBOOL showTimeMarker;

        SimulationParams() : 
            deltaTime(0.0),
            demoMode(YSFALSE),
            record(YSFALSE),
            showTimer(YSFALSE),
            networkStandby(YSFALSE),
            userControl(FSUSC_DISABLE),
            showTimeMarker(YSFALSE) {}
    };
    
private:
    FsSimulation *sim;
    std::atomic<bool> terminate;
    std::atomic<bool> paused;
    std::atomic<bool> dataReady;
public:
    std::atomic<bool> calculationDone;
private:    
    int frameRateLimit;  // 0 = no limit, otherwise max frames per second
    unsigned long lastFrameTime; // Last frame time in milliseconds
    
    std::thread calcThread;
    std::mutex paramsMutex;
    std::mutex simMutex;
    std::condition_variable dataReadyCV;
    std::condition_variable calcDoneCV;
    
    SimulationParams params;
    
    void ThreadFunction();

public:
    FsSimulationCalculationThread(FsSimulation *sim);
    ~FsSimulationCalculationThread();
    
    // Set frame rate limit (0 = no limit)
    void SetFrameRateLimit(int fps);

    // Start/stop the calculation thread
    void Start();
    void Stop();

    // Queue a new calculation with the given parameters
    void QueueCalculation(
        const double &deltaTime,
        YSBOOL demoMode,
        YSBOOL record,
        YSBOOL showTimer,
        YSBOOL networkStandby,
        FSUSERCONTROL userControl,
        YSBOOL showTimeMarker);
    
    // Wait for calculation to complete
    // Returns true if calculation was completed, false if timed out
    bool WaitForCalculation(int timeoutMs = 100);
    
    // Is the calculation thread active?
    bool IsActive() const;
    
    // Is calculation in progress?
    bool IsCalculating() const;
    
    // Pause/Resume calculation thread
    void Pause();
    void Resume();

    // Get mutex for simulation object - must be locked when modifying sim from main thread
    std::mutex& GetSimulationMutex();
};

/* } */
#endif
