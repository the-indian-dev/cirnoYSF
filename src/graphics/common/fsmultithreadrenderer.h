#ifndef FSMULTITHREADRENDERER_IS_INCLUDED
#define FSMULTITHREADRENDERER_IS_INCLUDED

#include <vector>
#include <memory>
#include <future>
#include <functional>
#include <atomic>
#include "fsrendercommandqueue.h"
#include <ysclass.h>
#include <ysthreadpool.h>

// Forward declarations
class FsSimulation;
class FsAirplane;
class FsGround;
class FsField;
class YsGLParticleManager;
class FsProjection;

struct FsRenderableObject
{
    enum Type
    {
        AIRPLANE,
        GROUND,
        TERRAIN,
        PARTICLE,
        EXPLOSION,
        CLOUD,
        WEAPON,
        EFFECT
    };
    
    Type type;
    void* objectPtr;
    YsVec3 position;
    double distance;
    YSBOOL visible;
    YSBOOL transparent;
    int lodLevel;
    
    FsRenderableObject() : type(AIRPLANE), objectPtr(nullptr), distance(0.0), 
                          visible(YSFALSE), transparent(YSFALSE), lodLevel(0) {}
};

struct FsRenderContext
{
    YsVec3 viewPoint;
    YsMatrix4x4 viewMatrix;
    YsMatrix4x4 projMatrix;
    YsMatrix4x4 viewProjMatrix;
    double nearZ, farZ;
    double tanFov;
    int screenWidth, screenHeight;
    
    // Frustum culling is handled by the game's existing system
    // These are kept for compatibility but not used
    YsVec3 frustumPlanes[6];
    YsVec3 frustumNormals[6];
    
    // Environment settings
    int environment; // FSDAYLIGHT, FSNIGHT, FSSUNSET
    YSBOOL fogEnabled;
    YsColor fogColor;
    double fogVisibility;
    
    FsRenderContext() : nearZ(1.0), farZ(10000.0), tanFov(1.0), 
                       screenWidth(800), screenHeight(600),
                       environment(0), fogEnabled(YSFALSE), fogVisibility(5000.0) 
    {
        viewMatrix.LoadIdentity();
        projMatrix.LoadIdentity();
        viewProjMatrix.LoadIdentity();
    }
    
    void CalculateFrustumPlanes(); // Compatibility only - does nothing
    YSBOOL IsInFrustum(const YsVec3& center, double radius) const; // Always returns true
};

class FsMultiThreadRenderer
{
private:
    struct ThreadData
    {
        int threadId;
        std::vector<FsRenderableObject> objects;
        FsRenderCommandQueue commandQueue;
        FsRenderContext context;
        YSBOOL finished;
        
        ThreadData() : threadId(0), finished(YSFALSE) {}
    };
    
    std::vector<ThreadData> threadData;
    YsThreadPool* threadPool;
    int numThreads;
    
    // Object lists for threading
    std::vector<FsRenderableObject> allObjects;
    std::vector<FsRenderableObject> opaqueObjects;
    std::vector<FsRenderableObject> transparentObjects;
    
    // LOD only (culling handled by game)
    std::atomic<int> objectsCulled;
    std::atomic<int> objectsLodReduced;
    
    // Performance tracking
    struct PerformanceStats
    {
        double cullTime;
        double lodTime;
        double commandBuildTime;
        double commandExecuteTime;
        int totalObjects;
        int culledObjects;
        int drawnObjects;
        int lodReductions;
        
        PerformanceStats() : cullTime(0.0), lodTime(0.0), commandBuildTime(0.0), 
                           commandExecuteTime(0.0), totalObjects(0), culledObjects(0),
                           drawnObjects(0), lodReductions(0) {}
    };
    
    PerformanceStats frameStats;
    
    // Methods
    void GatherRenderableObjects(const FsSimulation* sim, const FsRenderContext& context);
    void CullObjects(const FsRenderContext& context); // Compatibility only - does nothing
    void CalculateLOD(const FsRenderContext& context);
    void DistributeObjectsToThreads();
    void BuildCommandsForThread(int threadId);
    void MergeCommandQueues();
    
    // Threading functions
    static void ThreadBuildCommands(FsMultiThreadRenderer* renderer, int threadId);
    static void ThreadCullAndLOD(FsMultiThreadRenderer* renderer, int threadId, 
                                const FsRenderContext& context); // LOD only
    
    // LOD calculation helpers
    int CalculateAirplaneLOD(const FsAirplane* airplane, double distance) const;
    int CalculateGroundLOD(const FsGround* ground, double distance) const;
    int CalculateTerrainLOD(const FsField* field, const YsVec3& pos, double distance) const;
    
    // Culling helpers - disabled (always return false since culling is handled by game)
    YSBOOL CullAirplane(const FsAirplane* airplane, const FsRenderContext& context) const;
    YSBOOL CullGround(const FsGround* ground, const FsRenderContext& context) const;
    YSBOOL CullTerrain(const FsField* field, const YsVec3& pos, const FsRenderContext& context) const;
    
public:
    FsMultiThreadRenderer();
    ~FsMultiThreadRenderer();
    
    void Initialize(int numThreads = 0); // 0 = auto-detect
    void Shutdown();
    
    // Main rendering interface
    void BeginFrame(const FsRenderContext& context);
    void RenderSimulation(const FsSimulation* sim, const FsRenderContext& context);
    void EndFrame();
    
    // Command queue access
    FsRenderCommandQueue& GetOpaqueQueue();
    FsRenderCommandQueue& GetTransparentQueue();
    FsRenderCommandQueue& GetHudQueue();
    
    // Performance monitoring
    const PerformanceStats& GetFrameStats() const { return frameStats; }
    void ResetStats();
    
    // Configuration
    void SetLODDistances(double near, double medium, double far);
    void SetCullingEnabled(YSBOOL enabled); // Ignored - culling handled by game
    void SetLODEnabled(YSBOOL enabled);
    void SetFrustumCullingEnabled(YSBOOL enabled); // Ignored - culling handled by game
    void SetOcclusionCullingEnabled(YSBOOL enabled); // Ignored - culling handled by game
    
    // Advanced features
    void EnableInstancing(YSBOOL enable);
    void SetInstanceBatchSize(int size);
    void EnableCommandBuffering(YSBOOL enable);
    void SetCommandBufferSize(int size);
    
private:
    // Configuration
    YSBOOL cullingEnabled; // Ignored - culling handled by game
    YSBOOL lodEnabled;
    YSBOOL frustumCullingEnabled; // Ignored - culling handled by game
    YSBOOL occlusionCullingEnabled; // Ignored - culling handled by game
    YSBOOL instancingEnabled;
    YSBOOL commandBufferingEnabled;
    
    double lodNearDistance;
    double lodMediumDistance;
    double lodFarDistance;
    int instanceBatchSize;
    int commandBufferSize;
    
    // State
    YSBOOL initialized;
    FsRenderCommandManager* commandManager;
};

// Singleton access
class FsRenderingSystem
{
private:
    static FsRenderingSystem* instance;
    FsMultiThreadRenderer* renderer;
    
public:
    static FsRenderingSystem& GetInstance();
    static void Initialize(int numThreads = 0);
    static void Shutdown();
    
    FsMultiThreadRenderer* GetRenderer() { return renderer; }
    
private:
    FsRenderingSystem();
    ~FsRenderingSystem();
};

// Utility functions for integration
namespace FsRenderUtils
{
    // Convert existing draw calls to command queue
    void ConvertAirplaneDrawToCommands(
        FsRenderCommandQueue& queue,
        const FsAirplane* airplane,
        const FsRenderContext& context,
        int visualMode
    );
    
    void ConvertGroundDrawToCommands(
        FsRenderCommandQueue& queue,
        const FsGround* ground,
        const FsRenderContext& context,
        int visualMode
    );
    
    void ConvertTerrainDrawToCommands(
        FsRenderCommandQueue& queue,
        const FsField* field,
        const FsRenderContext& context
    );
    
    // Batch operations
    void BatchSimilarObjects(std::vector<FsRenderableObject>& objects);
    void SortObjectsByDistance(std::vector<FsRenderableObject>& objects, const YsVec3& viewPoint);
    void SortObjectsByState(std::vector<FsRenderableObject>& objects);
    
    // Performance helpers
    double CalculateObjectDistance(const FsRenderableObject& obj, const YsVec3& viewPoint);
    YsVec3 GetObjectBoundingSphere(const FsRenderableObject& obj, double& radius);
    YSBOOL IsObjectVisible(const FsRenderableObject& obj, const FsRenderContext& context);
}

#endif