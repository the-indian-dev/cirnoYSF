#ifndef FSEXPERIMENTALSHADOW_IS_INCLUDED
#define FSEXPERIMENTALSHADOW_IS_INCLUDED
/* { */

#include <ysclass.h>
#include <ysgl.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>

// Forward declarations - use void* for flexibility
class FsWorld;

// Shadow quality levels
enum FSSHADOWQUALITY
{
	FSSHADOWQUALITY_LOW = 0,
	FSSHADOWQUALITY_MEDIUM = 1,
	FSSHADOWQUALITY_HIGH = 2,
	FSSHADOWQUALITY_ULTRA = 3
};

// Shadow map resolution based on quality
const int SHADOW_MAP_SIZES[] = {512, 1024, 2048, 4096};

// Maximum number of shadow casters to process per frame
const int MAX_SHADOW_CASTERS_PER_FRAME = 32;

// Shadow culling distance (meters)
const double SHADOW_CULLING_DISTANCE = 5000.0;

// Shadow rendering job for multithreading
struct FsShadowRenderJob
{
	void *existence;  // Generic pointer to avoid include dependencies
	YsMatrix4x4 lightViewMatrix;
	YsMatrix4x4 lightProjectionMatrix;
	double distance;
	bool isVisible;
	
	FsShadowRenderJob() : existence(nullptr), distance(0.0), isVisible(false) {}
};

// Shadow map cache entry
struct FsShadowMapCacheEntry
{
	GLuint frameBuffer;
	GLuint depthTexture;
	GLuint colorTexture;  // Optional for debugging
	int resolution;
	bool isValid;
	double lastUsed;
	
	FsShadowMapCacheEntry() : frameBuffer(0), depthTexture(0), colorTexture(0), 
	                         resolution(0), isValid(false), lastUsed(0.0) {}
};

// Batch rendering data for reducing draw calls
struct FsShadowBatch
{
	std::vector<void*> objects;  // Generic pointers to avoid include dependencies
	YsMatrix4x4 *transforms;
	int count;
	bool useInstancing;
	
	FsShadowBatch() : transforms(nullptr), count(0), useInstancing(false) {}
	~FsShadowBatch() { delete[] transforms; }
};

// Main experimental shadow renderer class
class FsExperimentalShadowRenderer
{
private:
	// OpenGL resources
	GLuint shadowMapFBO;
	GLuint shadowMapTexture;
	GLuint shadowMapColorTexture;
	int shadowMapSize;
	
	// Shadow shader programs
	GLuint shadowGenShader;
	GLuint shadowRenderShader;
	
	// Vertex buffer objects for batching
	GLuint shadowVBO;
	GLuint shadowEBO;
	GLuint instanceVBO;
	
	// Shadow map cache for multiple light sources
	std::vector<FsShadowMapCacheEntry> shadowMapCache;
	int maxCacheSize;
	
	// Threading
	std::vector<std::thread> workerThreads;
	std::vector<FsShadowRenderJob> renderJobs;
	std::mutex jobMutex;
	std::atomic<bool> threadsActive;
	std::atomic<int> completedJobs;
	
	// Batch rendering
	std::vector<FsShadowBatch> shadowBatches;
	
	// Performance tracking
	double lastFrameTime;
	int framesRendered;
	double avgRenderTime;
	
	// Settings
	FSSHADOWQUALITY shadowQuality;
	bool enableSoftShadows;
	bool enableCascadedShadows;
	bool enableInstancing;
	bool enableMultithreading;
	double shadowBias;
	double shadowRadius;
	
	// Light parameters
	YsVec3 lightDirection;
	YsMatrix4x4 lightViewMatrix;
	YsMatrix4x4 lightProjectionMatrix;
	double lightNear, lightFar;
	double lightOrthoSize;
	
	// Frustum culling
	YsVec3 frustumCorners[8];
	YsPlane frustumPlanes[6];
	
	// Internal methods
	bool InitializeOpenGLResources();
	void CleanupOpenGLResources();
	bool CreateShadowMapFramebuffer();
	bool LoadShadowShaders();
	void UpdateLightMatrices(const YsVec3 &viewPos, const YsVec3 &viewDir);
	void CullShadowCasters(const FsWorld *world, const YsVec3 &viewPos);
	void SortShadowCastersByDistance(const YsVec3 &viewPos);
	void CreateShadowBatches();
	void RenderShadowMapSingleThreaded();
	void RenderShadowMapMultiThreaded();
	void RenderShadowBatch(const FsShadowBatch &batch);
	bool IsShadowCasterVisible(const void *obj, const YsVec3 &viewPos) const;
	double CalculateObjectDistance(const void *obj, const YsVec3 &viewPos) const;
	
	// Threading methods
	void InitializeThreadPool();
	void ShutdownThreadPool();
	void WorkerThreadFunction();
	void ProcessShadowJob(FsShadowRenderJob &job);
	
	// Shader compilation helpers
	GLuint CompileShader(const char *vertexSource, const char *fragmentSource);
	bool CheckShaderCompileStatus(GLuint shader, const char *type);
	bool CheckProgramLinkStatus(GLuint program);
	
	// Cache management
	FsShadowMapCacheEntry* GetCachedShadowMap(int resolution);
	void CleanupShadowMapCache();
	
public:
	FsExperimentalShadowRenderer();
	~FsExperimentalShadowRenderer();
	
	// Initialization and cleanup
	bool Initialize();
	void Cleanup();
	
	// Main rendering interface
	void BeginShadowPass(const YsVec3 &viewPos, const YsVec3 &viewDir, 
	                    const YsVec3 &lightDir, const FsWorld *world);
	void EndShadowPass();
	void RenderShadows(const YsMatrix4x4 &viewMatrix, const YsMatrix4x4 &projMatrix);
	
	// Configuration
	void SetShadowQuality(FSSHADOWQUALITY quality);
	void SetSoftShadowsEnabled(bool enabled);
	void SetCascadedShadowsEnabled(bool enabled);
	void SetInstancingEnabled(bool enabled);
	void SetMultithreadingEnabled(bool enabled);
	void SetShadowBias(double bias);
	void SetShadowRadius(double radius);
	void SetLightDirection(const YsVec3 &dir);
	
	// Performance monitoring
	double GetLastFrameRenderTime() const;
	double GetAverageRenderTime() const;
	int GetRenderedObjectCount() const;
	int GetBatchCount() const;
	
	// Debug interface
	void EnableDebugOutput(bool enabled);
	void RenderDebugShadowMap(int x, int y, int width, int height);
	
	// OpenGL state management
	void BindShadowMapTexture(int textureUnit = 0);
	void UnbindShadowMapTexture();
	
	// Utility methods
	static bool IsOpenGLExtensionSupported(const char *extension);
	static int GetMaxTextureSize();
	static int GetMaxFramebufferColorAttachments();
	static bool CheckOpenGLCapabilities();
};

// Global instance
extern FsExperimentalShadowRenderer *fsExperimentalShadowRenderer;

// Convenience functions
void FsInitializeExperimentalShadows(FSSHADOWQUALITY quality = FSSHADOWQUALITY_MEDIUM);
void FsCleanupExperimentalShadows();
void FsRenderExperimentalShadows(const YsVec3 &viewPos, const YsVec3 &viewDir, 
                                const YsVec3 &lightDir, const FsWorld *world,
                                const YsMatrix4x4 &viewMatrix, const YsMatrix4x4 &projMatrix);

/* } */
#endif