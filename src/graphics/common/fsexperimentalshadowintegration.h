#ifndef FSEXPERIMENTALSHADOWINTEGRATION_IS_INCLUDED
#define FSEXPERIMENTALSHADOWINTEGRATION_IS_INCLUDED
/* { */

#include "fsexperimentalshadow.h"
#include "../../config/fsconfig.h"

// Integration helper class for experimental shadows
class FsExperimentalShadowIntegration
{
private:
    static bool initialized;
    static FSSHADOWQUALITY currentQuality;
    static bool adaptiveQuality;
    static double lastFrameTime;
    static int frameCount;
    static double targetFrameTime; // Target frame time in milliseconds (e.g., 16.67ms for 60fps)
    
public:
    // Initialization and cleanup
    static void Initialize(const FsFlightConfig &config);
    static void Cleanup();
    
    // Main integration point - call this from SimDrawShadowMap
    static void RenderShadows(const FsFlightConfig &config, 
                             const YsVec3 &viewPos, 
                             const YsVec3 &viewDir,
                             const YsVec3 &lightDir,
                             const FsWorld *world,
                             const YsMatrix4x4 &viewMatrix,
                             const YsMatrix4x4 &projMatrix);
    
    // Configuration methods
    static void SetShadowQuality(FSSHADOWQUALITY quality);
    static void SetAdaptiveQuality(bool enabled);
    static void SetTargetFrameRate(double fps);
    
    // Performance monitoring
    static void UpdatePerformanceStats(double frameTime);
    static double GetAverageFrameTime();
    static bool ShouldReduceQuality();
    static bool ShouldIncreaseQuality();
    
    // Auto-configuration based on hardware
    static FSSHADOWQUALITY DetectOptimalQuality();
    static void ConfigureForHardware();
    
    // Debug and profiling
    static void ShowPerformanceStats();
    static void EnableDebugOutput(bool enabled);
    
    // Utility methods
    static bool IsExperimentalShadowsAvailable();
    static const char* GetQualityString(FSSHADOWQUALITY quality);
    static const char* GetCurrentStatusString();
};

// Convenience macros for integration
#define FS_EXPERIMENTAL_SHADOW_INIT(config) \
    FsExperimentalShadowIntegration::Initialize(config)

#define FS_EXPERIMENTAL_SHADOW_RENDER(config, viewPos, viewDir, lightDir, world, viewMat, projMat) \
    FsExperimentalShadowIntegration::RenderShadows(config, viewPos, viewDir, lightDir, world, viewMat, projMat)

#define FS_EXPERIMENTAL_SHADOW_CLEANUP() \
    FsExperimentalShadowIntegration::Cleanup()

// Performance monitoring callback
typedef void (*FsExperimentalShadowPerfCallback)(double frameTime, int objectCount, int batchCount);
extern FsExperimentalShadowPerfCallback fsExperimentalShadowPerfCallback;

/* } */
#endif