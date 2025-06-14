#include "fsexperimentalshadowintegration.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>

// Static member initialization
bool FsExperimentalShadowIntegration::initialized = false;
FSSHADOWQUALITY FsExperimentalShadowIntegration::currentQuality = FSSHADOWQUALITY_MEDIUM;
bool FsExperimentalShadowIntegration::adaptiveQuality = true;
double FsExperimentalShadowIntegration::lastFrameTime = 0.0;
int FsExperimentalShadowIntegration::frameCount = 0;
double FsExperimentalShadowIntegration::targetFrameTime = 16.67; // 60 FPS target

// Performance callback
FsExperimentalShadowPerfCallback fsExperimentalShadowPerfCallback = nullptr;

void FsExperimentalShadowIntegration::Initialize(const FsFlightConfig &config)
{
    if (initialized) {
        return;
    }
    
    // Check if experimental shadows are supported
    if (!IsExperimentalShadowsAvailable()) {
        return;
    }
    
    // Auto-detect optimal quality based on hardware
    currentQuality = DetectOptimalQuality();
    
    // Initialize the experimental shadow renderer
    FsInitializeExperimentalShadows(currentQuality);
    
    if (fsExperimentalShadowRenderer) {
        // Configure based on flight config
        fsExperimentalShadowRenderer->SetSoftShadowsEnabled(true);
        fsExperimentalShadowRenderer->SetInstancingEnabled(true);
        fsExperimentalShadowRenderer->SetMultithreadingEnabled(true);
        fsExperimentalShadowRenderer->SetShadowBias(0.003);
        fsExperimentalShadowRenderer->SetShadowRadius(1.5);
        
        // Set light direction from config
        fsExperimentalShadowRenderer->SetLightDirection(config.lightSourceDirection);
        
        initialized = true;
    }
}

void FsExperimentalShadowIntegration::Cleanup()
{
    FsCleanupExperimentalShadows();
    initialized = false;
    frameCount = 0;
    lastFrameTime = 0.0;
}

void FsExperimentalShadowIntegration::RenderShadows(const FsFlightConfig &config, 
                                                   const YsVec3 &viewPos, 
                                                   const YsVec3 &viewDir,
                                                   const YsVec3 &lightDir,
                                                   const FsWorld *world,
                                                   const YsMatrix4x4 &viewMatrix,
                                                   const YsMatrix4x4 &projMatrix)
{
    if (!initialized || !fsExperimentalShadowRenderer) {
        return;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Update light direction if it changed
    if ((lightDir - config.lightSourceDirection).GetSquareLength() > 0.01) {
        fsExperimentalShadowRenderer->SetLightDirection(config.lightSourceDirection);
    }
    
    // Render shadows using experimental renderer
    FsRenderExperimentalShadows(viewPos, viewDir, lightDir, world, viewMatrix, projMatrix);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    double frameTime = duration.count() / 1000.0; // Convert to milliseconds
    
    UpdatePerformanceStats(frameTime);
    
    // Adaptive quality adjustment
    if (adaptiveQuality && frameCount > 60) { // Wait for 60 frames before adjusting
        if (ShouldReduceQuality()) {
            if (currentQuality > FSSHADOWQUALITY_LOW) {
                SetShadowQuality(static_cast<FSSHADOWQUALITY>(currentQuality - 1));
            }
        } else if (ShouldIncreaseQuality()) {
            if (currentQuality < FSSHADOWQUALITY_ULTRA) {
                SetShadowQuality(static_cast<FSSHADOWQUALITY>(currentQuality + 1));
            }
        }
    }
    
    // Call performance callback if set
    if (fsExperimentalShadowPerfCallback && fsExperimentalShadowRenderer) {
        fsExperimentalShadowPerfCallback(frameTime, 
                                       fsExperimentalShadowRenderer->GetRenderedObjectCount(),
                                       fsExperimentalShadowRenderer->GetBatchCount());
    }
}

void FsExperimentalShadowIntegration::SetShadowQuality(FSSHADOWQUALITY quality)
{
    if (quality != currentQuality && fsExperimentalShadowRenderer) {
        currentQuality = quality;
        fsExperimentalShadowRenderer->SetShadowQuality(quality);
        
        // Reset frame count to prevent immediate quality changes
        frameCount = 0;
    }
}

void FsExperimentalShadowIntegration::SetAdaptiveQuality(bool enabled)
{
    adaptiveQuality = enabled;
}

void FsExperimentalShadowIntegration::SetTargetFrameRate(double fps)
{
    if (fps > 0.0) {
        targetFrameTime = 1000.0 / fps; // Convert to milliseconds
    }
}

void FsExperimentalShadowIntegration::UpdatePerformanceStats(double frameTime)
{
    frameCount++;
    
    if (frameCount == 1) {
        lastFrameTime = frameTime;
    } else {
        // Use exponential moving average
        lastFrameTime = (lastFrameTime * 0.9) + (frameTime * 0.1);
    }
}

double FsExperimentalShadowIntegration::GetAverageFrameTime()
{
    return lastFrameTime;
}

bool FsExperimentalShadowIntegration::ShouldReduceQuality()
{
    // Reduce quality if we're consistently over target frame time
    return lastFrameTime > (targetFrameTime * 1.3) && frameCount > 60;
}

bool FsExperimentalShadowIntegration::ShouldIncreaseQuality()
{
    // Increase quality if we have headroom (significantly under target)
    return lastFrameTime < (targetFrameTime * 0.7) && frameCount > 120;
}

FSSHADOWQUALITY FsExperimentalShadowIntegration::DetectOptimalQuality()
{
    // Check OpenGL capabilities
    int maxTextureSize = FsExperimentalShadowRenderer::GetMaxTextureSize();
    int maxFramebufferAttachments = FsExperimentalShadowRenderer::GetMaxFramebufferColorAttachments();
    
    // Basic heuristics for quality detection
    if (maxTextureSize >= 4096 && maxFramebufferAttachments >= 8) {
        return FSSHADOWQUALITY_HIGH;
    } else if (maxTextureSize >= 2048 && maxFramebufferAttachments >= 4) {
        return FSSHADOWQUALITY_MEDIUM;
    } else if (maxTextureSize >= 1024) {
        return FSSHADOWQUALITY_LOW;
    }
    
    return FSSHADOWQUALITY_LOW;
}

void FsExperimentalShadowIntegration::ConfigureForHardware()
{
    if (!fsExperimentalShadowRenderer) {
        return;
    }
    
    // Get hardware capabilities
    int maxTextureSize = FsExperimentalShadowRenderer::GetMaxTextureSize();
    bool hasInstancing = FsExperimentalShadowRenderer::IsOpenGLExtensionSupported("GL_ARB_draw_instanced");
    
    // Configure renderer based on capabilities
    fsExperimentalShadowRenderer->SetInstancingEnabled(hasInstancing);
    
    // Adjust settings based on texture size support
    if (maxTextureSize < 2048) {
        fsExperimentalShadowRenderer->SetSoftShadowsEnabled(false);
        fsExperimentalShadowRenderer->SetCascadedShadowsEnabled(false);
    } else if (maxTextureSize >= 4096) {
        fsExperimentalShadowRenderer->SetSoftShadowsEnabled(true);
        fsExperimentalShadowRenderer->SetCascadedShadowsEnabled(true);
    }
    
    // Enable multithreading based on CPU cores
    int numCores = std::thread::hardware_concurrency();
    fsExperimentalShadowRenderer->SetMultithreadingEnabled(numCores > 2);
}

void FsExperimentalShadowIntegration::ShowPerformanceStats()
{
    if (!fsExperimentalShadowRenderer) {
        return;
    }
    
    // This would ideally output to the game's console or debug output
    // For now, we'll use a simple approach
    double avgTime = fsExperimentalShadowRenderer->GetAverageRenderTime();
    int objectCount = fsExperimentalShadowRenderer->GetRenderedObjectCount();
    int batchCount = fsExperimentalShadowRenderer->GetBatchCount();
    
    // In a real implementation, this would use the game's logging system
    printf("Experimental Shadows - Avg: %.2fms, Objects: %d, Batches: %d, Quality: %s\n",
           avgTime, objectCount, batchCount, GetQualityString(currentQuality));
}

void FsExperimentalShadowIntegration::EnableDebugOutput(bool enabled)
{
    if (fsExperimentalShadowRenderer) {
        fsExperimentalShadowRenderer->EnableDebugOutput(enabled);
    }
}

bool FsExperimentalShadowIntegration::IsExperimentalShadowsAvailable()
{
    return FsExperimentalShadowRenderer::CheckOpenGLCapabilities();
}

const char* FsExperimentalShadowIntegration::GetQualityString(FSSHADOWQUALITY quality)
{
    switch (quality) {
        case FSSHADOWQUALITY_LOW:    return "LOW";
        case FSSHADOWQUALITY_MEDIUM: return "MEDIUM";
        case FSSHADOWQUALITY_HIGH:   return "HIGH";
        case FSSHADOWQUALITY_ULTRA:  return "ULTRA";
        default:                     return "UNKNOWN";
    }
}

const char* FsExperimentalShadowIntegration::GetCurrentStatusString()
{
    if (!initialized) {
        return "NOT INITIALIZED";
    }
    
    if (!fsExperimentalShadowRenderer) {
        return "UNAVAILABLE";
    }
    
    static char statusBuffer[256];
    snprintf(statusBuffer, sizeof(statusBuffer), 
             "ACTIVE - Quality: %s, Adaptive: %s, Avg: %.2fms",
             GetQualityString(currentQuality),
             adaptiveQuality ? "ON" : "OFF",
             GetAverageFrameTime());
    
    return statusBuffer;
}