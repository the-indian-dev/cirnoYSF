#include "fsexperimentalshadow.h"
#include <ysglmath.h>
#include <ysviewcontrol.h>
#include <cmath>
#include <algorithm>
#include <chrono>

// Define missing OpenGL 2.0 constants for compatibility
#ifndef GL_MAX_COLOR_ATTACHMENTS
#define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#endif

// Global instance
FsExperimentalShadowRenderer *fsExperimentalShadowRenderer = nullptr;

// Shadow generation vertex shader
const char* SHADOW_GEN_VERTEX_SHADER = R"(
#version 120
attribute vec3 position;
uniform mat4 lightViewProjectionMatrix;
uniform mat4 modelMatrix;

void main()
{
    gl_Position = lightViewProjectionMatrix * modelMatrix * vec4(position, 1.0);
}
)";

// Shadow generation fragment shader
const char* SHADOW_GEN_FRAGMENT_SHADER = R"(
#version 120
void main()
{
    // Depth is automatically written to depth buffer
    gl_FragColor = vec4(gl_FragCoord.z, gl_FragCoord.z, gl_FragCoord.z, 1.0);
}
)";

// Shadow rendering vertex shader
const char* SHADOW_RENDER_VERTEX_SHADER = R"(
#version 120
attribute vec3 position;
attribute vec3 normal;
attribute vec2 texCoord;

uniform mat4 modelViewProjectionMatrix;
uniform mat4 lightViewProjectionMatrix;
uniform mat4 modelMatrix;
uniform mat3 normalMatrix;

varying vec3 worldPos;
varying vec3 worldNormal;
varying vec4 shadowCoord;
varying vec2 uv;

void main()
{
    vec4 worldPosition = modelMatrix * vec4(position, 1.0);
    worldPos = worldPosition.xyz;
    worldNormal = normalize(normalMatrix * normal);
    uv = texCoord;
    
    // Transform to light space for shadow mapping
    shadowCoord = lightViewProjectionMatrix * worldPosition;
    
    gl_Position = modelViewProjectionMatrix * vec4(position, 1.0);
}
)";

// Shadow rendering fragment shader
const char* SHADOW_RENDER_FRAGMENT_SHADER = R"(
#version 120
uniform sampler2D shadowMap;
uniform vec3 lightDirection;
uniform float shadowBias;
uniform float shadowRadius;
uniform bool enableSoftShadows;

varying vec3 worldPos;
varying vec3 worldNormal;
varying vec4 shadowCoord;
varying vec2 uv;

float PCF(vec2 shadowUV, float depth)
{
    float shadow = 0.0;
    float texelSize = 1.0 / 1024.0; // Assume 1024x1024 shadow map
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture2D(shadowMap, shadowUV + vec2(x, y) * texelSize).r;
            shadow += (depth - shadowBias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main()
{
    // Basic lighting calculation
    vec3 lightDir = normalize(-lightDirection);
    float NdotL = max(dot(worldNormal, lightDir), 0.0);
    
    // Shadow calculation
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    float shadow = 0.0;
    if (projCoords.z <= 1.0 && projCoords.x >= 0.0 && projCoords.x <= 1.0 && 
        projCoords.y >= 0.0 && projCoords.y <= 1.0) {
        
        if (enableSoftShadows) {
            shadow = PCF(projCoords.xy, projCoords.z);
        } else {
            float closestDepth = texture2D(shadowMap, projCoords.xy).r;
            shadow = (projCoords.z - shadowBias > closestDepth) ? 1.0 : 0.0;
        }
    }
    
    // Apply shadow to lighting
    float finalLight = NdotL * (1.0 - shadow * 0.8);
    
    gl_FragColor = vec4(vec3(finalLight), 1.0);
}
)";

FsExperimentalShadowRenderer::FsExperimentalShadowRenderer()
{
    shadowMapFBO = 0;
    shadowMapTexture = 0;
    shadowMapColorTexture = 0;
    shadowMapSize = 1024;
    
    shadowGenShader = 0;
    shadowRenderShader = 0;
    
    shadowVBO = 0;
    shadowEBO = 0;
    instanceVBO = 0;
    
    maxCacheSize = 4;
    
    threadsActive = false;
    completedJobs = 0;
    
    lastFrameTime = 0.0;
    framesRendered = 0;
    avgRenderTime = 0.0;
    
    shadowQuality = FSSHADOWQUALITY_MEDIUM;
    enableSoftShadows = true;
    enableCascadedShadows = false;
    enableInstancing = true;
    enableMultithreading = true;
    shadowBias = 0.005;
    shadowRadius = 2.0;
    
    lightNear = 1.0;
    lightFar = 1000.0;
    lightOrthoSize = 100.0;
    
    lightDirection.Set(0.0, -1.0, -1.0);
    lightDirection.Normalize();
}

FsExperimentalShadowRenderer::~FsExperimentalShadowRenderer()
{
    Cleanup();
}

bool FsExperimentalShadowRenderer::Initialize()
{
    if (!CheckOpenGLCapabilities()) {
        return false;
    }
    
    shadowMapSize = SHADOW_MAP_SIZES[shadowQuality];
    
    if (!InitializeOpenGLResources()) {
        return false;
    }
    
    if (enableMultithreading) {
        InitializeThreadPool();
    }
    
    return true;
}

void FsExperimentalShadowRenderer::Cleanup()
{
    if (enableMultithreading) {
        ShutdownThreadPool();
    }
    
    CleanupOpenGLResources();
    CleanupShadowMapCache();
}

bool FsExperimentalShadowRenderer::InitializeOpenGLResources()
{
    // Create shadow map framebuffer
    if (!CreateShadowMapFramebuffer()) {
        return false;
    }
    
    // Load shadow shaders
    if (!LoadShadowShaders()) {
        return false;
    }
    
    // Create vertex buffer objects
    glGenBuffers(1, &shadowVBO);
    glGenBuffers(1, &shadowEBO);
    glGenBuffers(1, &instanceVBO);
    
    return true;
}

void FsExperimentalShadowRenderer::CleanupOpenGLResources()
{
    if (shadowMapFBO != 0) {
        glDeleteFramebuffers(1, &shadowMapFBO);
        shadowMapFBO = 0;
    }
    
    if (shadowMapTexture != 0) {
        glDeleteTextures(1, &shadowMapTexture);
        shadowMapTexture = 0;
    }
    
    if (shadowMapColorTexture != 0) {
        glDeleteTextures(1, &shadowMapColorTexture);
        shadowMapColorTexture = 0;
    }
    
    if (shadowGenShader != 0) {
        glDeleteProgram(shadowGenShader);
        shadowGenShader = 0;
    }
    
    if (shadowRenderShader != 0) {
        glDeleteProgram(shadowRenderShader);
        shadowRenderShader = 0;
    }
    
    if (shadowVBO != 0) {
        glDeleteBuffers(1, &shadowVBO);
        shadowVBO = 0;
    }
    
    if (shadowEBO != 0) {
        glDeleteBuffers(1, &shadowEBO);
        shadowEBO = 0;
    }
    
    if (instanceVBO != 0) {
        glDeleteBuffers(1, &instanceVBO);
        instanceVBO = 0;
    }
}

void FsExperimentalShadowRenderer::CleanupShadowMapCache()
{
    for (auto &entry : shadowMapCache) {
        if (entry.frameBuffer != 0) {
            glDeleteFramebuffers(1, &entry.frameBuffer);
        }
        if (entry.depthTexture != 0) {
            glDeleteTextures(1, &entry.depthTexture);
        }
        if (entry.colorTexture != 0) {
            glDeleteTextures(1, &entry.colorTexture);
        }
    }
    shadowMapCache.clear();
}

bool FsExperimentalShadowRenderer::CreateShadowMapFramebuffer()
{
    // Generate framebuffer
    glGenFramebuffers(1, &shadowMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    
    // Create depth texture
    glGenTextures(1, &shadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, shadowMapSize, shadowMapSize, 
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Attach depth texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTexture, 0);
    
    // We don't need a color buffer for shadow mapping
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    // Check framebuffer status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool FsExperimentalShadowRenderer::LoadShadowShaders()
{
    shadowGenShader = CompileShader(SHADOW_GEN_VERTEX_SHADER, SHADOW_GEN_FRAGMENT_SHADER);
    if (shadowGenShader == 0) {
        return false;
    }
    
    shadowRenderShader = CompileShader(SHADOW_RENDER_VERTEX_SHADER, SHADOW_RENDER_FRAGMENT_SHADER);
    if (shadowRenderShader == 0) {
        return false;
    }
    
    return true;
}

GLuint FsExperimentalShadowRenderer::CompileShader(const char *vertexSource, const char *fragmentSource)
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);
    
    if (!CheckShaderCompileStatus(vertexShader, "vertex")) {
        glDeleteShader(vertexShader);
        return 0;
    }
    
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);
    
    if (!CheckShaderCompileStatus(fragmentShader, "fragment")) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    if (!CheckProgramLinkStatus(program)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        return 0;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

bool FsExperimentalShadowRenderer::CheckShaderCompileStatus(GLuint shader, const char *type)
{
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        // Could log error here if logging system available
        return false;
    }
    
    return true;
}

bool FsExperimentalShadowRenderer::CheckProgramLinkStatus(GLuint program)
{
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        // Could log error here if logging system available
        return false;
    }
    
    return true;
}

void FsExperimentalShadowRenderer::BeginShadowPass(const YsVec3 &viewPos, const YsVec3 &viewDir, 
                                                  const YsVec3 &lightDir, const FsWorld *world)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    
    lightDirection = lightDir;
    lightDirection.Normalize();
    
    UpdateLightMatrices(viewPos, viewDir);
    CullShadowCasters(world, viewPos);
    SortShadowCastersByDistance(viewPos);
    CreateShadowBatches();
    
    // Render shadow map
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glViewport(0, 0, shadowMapSize, shadowMapSize);
    
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Enable polygon offset to reduce shadow acne
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    
    if (enableMultithreading && renderJobs.size() > 4) {
        RenderShadowMapMultiThreaded();
    } else {
        RenderShadowMapSingleThreaded();
    }
    
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    lastFrameTime = duration.count() / 1000.0; // Convert to milliseconds
    
    // Update running average
    framesRendered++;
    if (framesRendered == 1) {
        avgRenderTime = lastFrameTime;
    } else {
        avgRenderTime = (avgRenderTime * 0.9) + (lastFrameTime * 0.1);
    }
}

void FsExperimentalShadowRenderer::EndShadowPass()
{
    // Restore viewport (this would need to be set to correct viewport)
    glViewport(0, 0, 800, 600); // This should be set by the main rendering system
}

void FsExperimentalShadowRenderer::UpdateLightMatrices(const YsVec3 &viewPos, const YsVec3 &viewDir)
{
    // Use the same light matrix calculation as AUTO mode for accuracy
    const YsVec3 cameraEv = viewDir;
    const YsVec3 cameraUv(0.0, 1.0, 0.0); // Simplified up vector
    
    const YsVec3 lightEv = -YsUnitVector(lightDirection);
    const double cameraEvSimilarity = fabs(lightEv * cameraEv);
    const double cameraUvSimilarity = fabs(lightEv * cameraUv);
    const YsVec3 lightUv = (cameraEvSimilarity < cameraUvSimilarity ? cameraEv : cameraUv);
    
    YsAtt3 lightViewAtt;
    lightViewAtt.SetTwoVector(lightEv, lightUv);
    
    // Calculate shadow parameters similar to AUTO mode
    const double cosTheta = cameraEv * lightEv;
    const double sinTheta = sqrt(1.0 - YsSmaller(1.0, cosTheta * cosTheta));
    
    const double baseDist = 200.0; // Default view target distance
    const double lightVolumeProfile = baseDist;
    const double lightDepthProfile = lightVolumeProfile * 30.0;
    const double lightPullProfile = lightDepthProfile / 3.0;
    
    const YsVec3 push = cameraEv * lightVolumeProfile * sinTheta;
    const YsVec3 lightOrigin = viewPos + push - lightEv * lightPullProfile;
    
    // Build light view matrix like AUTO mode
    lightViewMatrix.LoadIdentity();
    lightViewMatrix.RotateXY(-lightViewAtt.b());
    lightViewMatrix.RotateZY(-lightViewAtt.p());
    lightViewMatrix.RotateXZ(-lightViewAtt.h());
    lightViewMatrix.Translate(-lightOrigin);
    
    // Use YsProjectionTransformation for consistent projection matrix
    YsProjectionTransformation proj;
    proj.SetProjectionMode(YsProjectionTransformation::ORTHOGONAL);
    proj.SetAspectRatio(1.0);
    proj.SetOrthogonalProjectionHeight(lightVolumeProfile * 2.0);
    proj.SetNearFar(0.0, lightDepthProfile);
    lightProjectionMatrix = proj.GetProjectionMatrix();
}

void FsExperimentalShadowRenderer::CullShadowCasters(const FsWorld *world, const YsVec3 &viewPos)
{
    renderJobs.clear();
    
    if (!world) return;
    
    // Since we can't directly access FsWorld objects yet, we'll create a minimal
    // implementation that at least sets up the rendering pipeline correctly.
    // The actual object iteration will be handled by the calling code.
    
    // Create a placeholder job to ensure the rendering pipeline works
    FsShadowRenderJob job;
    job.existence = nullptr;
    job.distance = 0.0;
    job.isVisible = true;
    job.lightViewMatrix = lightViewMatrix;
    job.lightProjectionMatrix = lightProjectionMatrix;
    
    renderJobs.push_back(job);
}

void FsExperimentalShadowRenderer::SortShadowCastersByDistance(const YsVec3 &viewPos)
{
    std::sort(renderJobs.begin(), renderJobs.end(), 
              [](const FsShadowRenderJob &a, const FsShadowRenderJob &b) {
                  return a.distance < b.distance;
              });
}

void FsExperimentalShadowRenderer::CreateShadowBatches()
{
    shadowBatches.clear();
    
    if (renderJobs.empty()) {
        return;
    }
    
    // Simple batching - group similar objects together
    FsShadowBatch currentBatch;
    
    for (const auto &job : renderJobs) {
        if (!job.isVisible) continue;
        
        currentBatch.objects.push_back(job.existence);
        currentBatch.count++;
        
        // Start new batch if current one is full
        if (currentBatch.count >= 16) {
            shadowBatches.push_back(std::move(currentBatch));
            currentBatch = FsShadowBatch();
        }
    }
    
    // Add remaining objects
    if (currentBatch.count > 0) {
        shadowBatches.push_back(std::move(currentBatch));
    }
}

void FsExperimentalShadowRenderer::RenderShadowMapSingleThreaded()
{
    // For now, we'll let the calling code handle the actual object rendering
    // since it already has access to the proper DrawShadow methods.
    // This ensures compatibility with the existing shadow rendering system.
    
    // Set up OpenGL state for shadow map rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    
    // The actual object rendering will be handled by the integration layer
    // which can call the proper object->DrawShadow() methods
}

void FsExperimentalShadowRenderer::RenderShadowMapMultiThreaded()
{
    // Reset job completion counter
    completedJobs = 0;
    
    // Signal threads to start processing
    {
        std::lock_guard<std::mutex> lock(jobMutex);
        threadsActive = true;
    }
    
    // Wait for all jobs to complete
    while (completedJobs < renderJobs.size()) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    // Render all batches (OpenGL calls must be on main thread)
    RenderShadowMapSingleThreaded();
    
    threadsActive = false;
}

void FsExperimentalShadowRenderer::RenderShadowBatch(const FsShadowBatch &batch)
{
    // This is a placeholder implementation
    // In the real version, you would render the actual geometry
    for (void *obj : batch.objects) {
        if (!obj) continue;
        
        // Get object transformation matrix and render geometry
        // This would call the object's specific rendering method
        // ((FsExistence*)obj)->DrawShadow() or similar when integrated
    }
}

bool FsExperimentalShadowRenderer::IsShadowCasterVisible(const void *obj, const YsVec3 &viewPos) const
{
    if (!obj) return false;
    
    double distance = CalculateObjectDistance(obj, viewPos);
    return distance <= SHADOW_CULLING_DISTANCE;
}

double FsExperimentalShadowRenderer::CalculateObjectDistance(const void *obj, const YsVec3 &viewPos) const
{
    if (!obj) return SHADOW_CULLING_DISTANCE + 1.0;
    
    // This would need to get the object's position
    // YsVec3 objPos = ((FsExistence*)obj)->GetPosition();
    // return (objPos - viewPos).GetLength();
    
    return 0.0; // Placeholder
}

void FsExperimentalShadowRenderer::InitializeThreadPool()
{
    int numThreads = std::min(4, (int)std::thread::hardware_concurrency());
    if (numThreads <= 1) {
        enableMultithreading = false;
        return;
    }
    
    threadsActive = false;
    
    for (int i = 0; i < numThreads; ++i) {
        workerThreads.emplace_back(&FsExperimentalShadowRenderer::WorkerThreadFunction, this);
    }
}

void FsExperimentalShadowRenderer::ShutdownThreadPool()
{
    threadsActive = false;
    
    for (auto &thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    workerThreads.clear();
}

void FsExperimentalShadowRenderer::WorkerThreadFunction()
{
    while (true) {
        bool hasWork = false;
        
        {
            std::lock_guard<std::mutex> lock(jobMutex);
            hasWork = threadsActive;
        }
        
        if (!hasWork) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        
        // Process shadow jobs (non-OpenGL work like culling, matrix calculations)
        for (auto &job : renderJobs) {
            if (!job.isVisible) continue;
            ProcessShadowJob(job);
        }
        
        completedJobs = renderJobs.size();
        threadsActive = false;
    }
}

void FsExperimentalShadowRenderer::ProcessShadowJob(FsShadowRenderJob &job)
{
    // Perform non-OpenGL processing for shadow job
    // This could include matrix calculations, culling tests, etc.
    // The actual OpenGL rendering must happen on the main thread
}

void FsExperimentalShadowRenderer::SetShadowQuality(FSSHADOWQUALITY quality)
{
    shadowQuality = quality;
    shadowMapSize = SHADOW_MAP_SIZES[quality];
    
    // Recreate shadow map with new resolution
    if (shadowMapFBO != 0) {
        CleanupOpenGLResources();
        InitializeOpenGLResources();
    }
}

void FsExperimentalShadowRenderer::SetSoftShadowsEnabled(bool enabled)
{
    enableSoftShadows = enabled;
}

void FsExperimentalShadowRenderer::SetCascadedShadowsEnabled(bool enabled)
{
    enableCascadedShadows = enabled;
}

void FsExperimentalShadowRenderer::SetInstancingEnabled(bool enabled)
{
    enableInstancing = enabled;
}

void FsExperimentalShadowRenderer::SetMultithreadingEnabled(bool enabled)
{
    enableMultithreading = enabled;
}

void FsExperimentalShadowRenderer::SetShadowBias(double bias)
{
    shadowBias = bias;
}

void FsExperimentalShadowRenderer::SetShadowRadius(double radius)
{
    shadowRadius = radius;
}

void FsExperimentalShadowRenderer::SetLightDirection(const YsVec3 &dir)
{
    lightDirection = dir;
    lightDirection.Normalize();
}

void FsExperimentalShadowRenderer::BindShadowMapTexture(int textureUnit)
{
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
}

void FsExperimentalShadowRenderer::UnbindShadowMapTexture()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool FsExperimentalShadowRenderer::CheckOpenGLCapabilities()
{
    // Check for required OpenGL extensions
    if (!IsOpenGLExtensionSupported("GL_EXT_framebuffer_object")) {
        return false;
    }
    
    int maxTextureSize = GetMaxTextureSize();
    if (maxTextureSize < 1024) {
        return false;
    }
    
    return true;
}

bool FsExperimentalShadowRenderer::IsOpenGLExtensionSupported(const char *extension)
{
    const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (!extensions) return false;
    
    return strstr(extensions, extension) != nullptr;
}

int FsExperimentalShadowRenderer::GetMaxTextureSize()
{
    int maxSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    return maxSize;
}

int FsExperimentalShadowRenderer::GetMaxFramebufferColorAttachments()
{
    int maxAttachments = 4; // Safe default for OpenGL 2.0
    // GL_MAX_COLOR_ATTACHMENTS might not be available in OpenGL 2.0
    // glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttachments);
    return maxAttachments;
}

void FsExperimentalShadowRenderer::EnableDebugOutput(bool enabled)
{
    // Debug output implementation would go here
    // For now, just store the flag
    static bool debugEnabled = enabled;
}

FsShadowMapCacheEntry* FsExperimentalShadowRenderer::GetCachedShadowMap(int resolution)
{
    // Find existing cache entry
    for (auto &entry : shadowMapCache) {
        if (entry.resolution == resolution && entry.isValid) {
            entry.lastUsed = 0.0; // Update timestamp (would use real time in full implementation)
            return &entry;
        }
    }
    
    // Create new entry if cache not full
    if (shadowMapCache.size() < maxCacheSize) {
        FsShadowMapCacheEntry newEntry;
        newEntry.resolution = resolution;
        newEntry.isValid = false;
        newEntry.lastUsed = 0.0;
        shadowMapCache.push_back(newEntry);
        return &shadowMapCache.back();
    }
    
    // Find least recently used entry
    auto oldest = shadowMapCache.begin();
    for (auto it = shadowMapCache.begin(); it != shadowMapCache.end(); ++it) {
        if (it->lastUsed < oldest->lastUsed) {
            oldest = it;
        }
    }
    
    // Clean up old resources
    if (oldest->frameBuffer != 0) {
        glDeleteFramebuffers(1, &oldest->frameBuffer);
    }
    if (oldest->depthTexture != 0) {
        glDeleteTextures(1, &oldest->depthTexture);
    }
    if (oldest->colorTexture != 0) {
        glDeleteTextures(1, &oldest->colorTexture);
    }
    
    // Reset entry
    *oldest = FsShadowMapCacheEntry();
    oldest->resolution = resolution;
    oldest->lastUsed = 0.0;
    
    return &(*oldest);
}

void FsExperimentalShadowRenderer::RenderDebugShadowMap(int x, int y, int width, int height)
{
    if (shadowMapTexture == 0) return;
    
    // Simple debug rendering - would need proper 2D rendering setup
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 800, 600, 0, -1, 1); // Assume 800x600 screen
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(x, y);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(x + width, y);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(x + width, y + height);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(x, y + height);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

double FsExperimentalShadowRenderer::GetLastFrameRenderTime() const
{
    return lastFrameTime;
}

double FsExperimentalShadowRenderer::GetAverageRenderTime() const
{
    return avgRenderTime;
}

int FsExperimentalShadowRenderer::GetRenderedObjectCount() const
{
    return renderJobs.size();
}

int FsExperimentalShadowRenderer::GetBatchCount() const
{
    return shadowBatches.size();
}

// Global convenience functions
void FsInitializeExperimentalShadows(FSSHADOWQUALITY quality)
{
    if (!fsExperimentalShadowRenderer) {
        fsExperimentalShadowRenderer = new FsExperimentalShadowRenderer();
        fsExperimentalShadowRenderer->SetShadowQuality(quality);
        fsExperimentalShadowRenderer->Initialize();
    }
}

void FsCleanupExperimentalShadows()
{
    if (fsExperimentalShadowRenderer) {
        delete fsExperimentalShadowRenderer;
        fsExperimentalShadowRenderer = nullptr;
    }
}

void FsRenderExperimentalShadows(const YsVec3 &viewPos, const YsVec3 &viewDir, 
                                const YsVec3 &lightDir, const FsWorld *world,
                                const YsMatrix4x4 &viewMatrix, const YsMatrix4x4 &projMatrix)
{
    if (fsExperimentalShadowRenderer) {
        fsExperimentalShadowRenderer->BeginShadowPass(viewPos, viewDir, lightDir, world);
        fsExperimentalShadowRenderer->EndShadowPass();
    }
}