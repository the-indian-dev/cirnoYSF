#include "fsmultithreadrenderer.h"
#include "fsrendercommandqueue.h"
#include <chrono>
#include <algorithm>
#include <thread>
#include <cmath>
#include <ysgl.h>
#include <ysglslsharedrenderer.h>
#include <ysglbuffermanager.h>

FsRenderingSystem* FsRenderingSystem::instance = nullptr;

void FsRenderContext::CalculateFrustumPlanes()
{
    // Frustum culling is already implemented in the game
    // This function is kept for compatibility but does nothing
}

YSBOOL FsRenderContext::IsInFrustum(const YsVec3& center, double radius) const
{
    // Frustum culling is already implemented in the game
    // Always return true since culling is handled elsewhere
    return YSTRUE;
}

FsMultiThreadRenderer::FsMultiThreadRenderer()
    : threadPool(nullptr), numThreads(0), objectsCulled(0), objectsLodReduced(0),
      cullingEnabled(YSTRUE), lodEnabled(YSTRUE), frustumCullingEnabled(YSTRUE),
      occlusionCullingEnabled(YSFALSE), instancingEnabled(YSFALSE),
      commandBufferingEnabled(YSTRUE), lodNearDistance(100.0), lodMediumDistance(500.0),
      lodFarDistance(2000.0), instanceBatchSize(100), commandBufferSize(10000),
      initialized(YSFALSE), commandManager(nullptr)
{
}

FsMultiThreadRenderer::~FsMultiThreadRenderer()
{
    Shutdown();
}

void FsMultiThreadRenderer::Initialize(int requestedThreads)
{
    if (initialized)
        return;
    
    if (requestedThreads <= 0)
    {
        numThreads = std::max(1, (int)std::thread::hardware_concurrency() - 1);
    }
    else
    {
        numThreads = requestedThreads;
    }
    
    // Initialize thread data
    threadData.resize(numThreads);
    for (int i = 0; i < numThreads; i++)
    {
        threadData[i].threadId = i;
        threadData[i].commandQueue.Reserve(commandBufferSize / numThreads);
    }
    
    // Get reference to existing thread pool from simulation
    // threadPool will be set by the simulation system
    
    commandManager = &FsRenderCommandManager::GetInstance();
    
    // Reserve memory for object lists
    allObjects.reserve(10000);
    opaqueObjects.reserve(8000);
    transparentObjects.reserve(2000);
    
    initialized = YSTRUE;
}

void FsMultiThreadRenderer::Shutdown()
{
    if (!initialized)
        return;
    
    threadData.clear();
    allObjects.clear();
    opaqueObjects.clear();
    transparentObjects.clear();
    
    initialized = YSFALSE;
}

void FsMultiThreadRenderer::BeginFrame(const FsRenderContext& context)
{
    if (!initialized)
        return;
    
    ResetStats();
    
    // Clear previous frame data
    allObjects.clear();
    opaqueObjects.clear();
    transparentObjects.clear();
    objectsCulled = 0;
    objectsLodReduced = 0;
    
    // Clear command queues
    for (auto& data : threadData)
    {
        data.commandQueue.Clear();
        data.finished = YSFALSE;
        data.context = context;
    }
    
    commandManager->ClearAllQueues();
}

void FsMultiThreadRenderer::RenderSimulation(const FsSimulation* sim, const FsRenderContext& context)
{
    if (!initialized || !sim)
        return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Step 1: Gather all renderable objects
    GatherRenderableObjects(sim, context);
    frameStats.totalObjects = (int)allObjects.size();
    
    // Step 2: LOD calculation only (culling is already handled by the game)
    if (lodEnabled)
    {
        CalculateLOD(context);
    }
    
    frameStats.culledObjects = objectsCulled.load();
    frameStats.lodReductions = objectsLodReduced.load();
    frameStats.drawnObjects = frameStats.totalObjects - frameStats.culledObjects;
    
    // Step 3: Separate opaque and transparent objects
    for (const auto& obj : allObjects)
    {
        if (obj.visible)
        {
            if (obj.transparent)
                transparentObjects.push_back(obj);
            else
                opaqueObjects.push_back(obj);
        }
    }
    
    // Step 4: Distribute objects to threads and build commands
    DistributeObjectsToThreads();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    frameStats.commandBuildTime = std::chrono::duration<double>(endTime - startTime).count() * 1000.0;
    
    // Step 5: Merge command queues
    MergeCommandQueues();
}

void FsMultiThreadRenderer::EndFrame()
{
    if (!initialized)
        return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Execute all commands
    commandManager->ExecuteAllQueues();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    frameStats.commandExecuteTime = std::chrono::duration<double>(endTime - startTime).count() * 1000.0;
}

void FsMultiThreadRenderer::GatherRenderableObjects(const FsSimulation* sim, const FsRenderContext& context)
{
    allObjects.clear();
    
    // Gather airplanes
    for (auto airPtr = sim->GetAirplaneList(); nullptr != airPtr; airPtr = airPtr->GetNext())
    {
        if (airPtr->IsAlive())
        {
            FsRenderableObject obj;
            obj.type = FsRenderableObject::AIRPLANE;
            obj.objectPtr = (void*)airPtr;
            obj.position = airPtr->GetPosition();
            obj.distance = (obj.position - context.viewPoint).GetLength();
            obj.visible = YSTRUE;
            obj.transparent = YSFALSE;
            obj.lodLevel = 0;
            allObjects.push_back(obj);
        }
    }
    
    // Gather ground vehicles
    for (auto gndPtr = sim->GetGroundList(); nullptr != gndPtr; gndPtr = gndPtr->GetNext())
    {
        if (gndPtr->IsAlive())
        {
            FsRenderableObject obj;
            obj.type = FsRenderableObject::GROUND;
            obj.objectPtr = (void*)gndPtr;
            obj.position = gndPtr->GetPosition();
            obj.distance = (obj.position - context.viewPoint).GetLength();
            obj.visible = YSTRUE;
            obj.transparent = YSFALSE;
            obj.lodLevel = 0;
            allObjects.push_back(obj);
        }
    }
    
    // Add terrain objects (simplified - would need more detailed terrain subdivision)
    FsRenderableObject terrainObj;
    terrainObj.type = FsRenderableObject::TERRAIN;
    terrainObj.objectPtr = (void*)&sim->GetField();
    terrainObj.position = context.viewPoint;
    terrainObj.distance = 0.0;
    terrainObj.visible = YSTRUE;
    terrainObj.transparent = YSFALSE;
    terrainObj.lodLevel = 0;
    allObjects.push_back(terrainObj);
}

void FsMultiThreadRenderer::CullObjects(const FsRenderContext& context)
{
    // Culling is already handled by the game's existing frustum culling system
    // This function is kept for compatibility but does nothing
    frameStats.cullTime = 0.0;
}

void FsMultiThreadRenderer::CalculateLOD(const FsRenderContext& context)
{
    if (!lodEnabled)
        return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (auto& obj : allObjects)
    {
        if (obj.visible)
        {
            int newLodLevel = 0;
            
            switch (obj.type)
            {
            case FsRenderableObject::AIRPLANE:
                newLodLevel = CalculateAirplaneLOD((const FsAirplane*)obj.objectPtr, obj.distance);
                break;
            case FsRenderableObject::GROUND:
                newLodLevel = CalculateGroundLOD((const FsGround*)obj.objectPtr, obj.distance);
                break;
            case FsRenderableObject::TERRAIN:
                newLodLevel = CalculateTerrainLOD((const FsField*)obj.objectPtr, obj.position, obj.distance);
                break;
            default:
                newLodLevel = 0;
                break;
            }
            
            if (newLodLevel > 0)
            {
                objectsLodReduced++;
            }
            
            obj.lodLevel = newLodLevel;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    frameStats.lodTime = std::chrono::duration<double>(endTime - startTime).count() * 1000.0;
}

void FsMultiThreadRenderer::DistributeObjectsToThreads()
{
    if (numThreads <= 1)
    {
        // Single threaded - process all objects in thread 0
        threadData[0].objects = allObjects;
        BuildCommandsForThread(0);
        return;
    }
    
    // Distribute objects evenly across threads
    int objectsPerThread = (int)allObjects.size() / numThreads;
    int remainder = (int)allObjects.size() % numThreads;
    
    int currentIndex = 0;
    for (int i = 0; i < numThreads; i++)
    {
        threadData[i].objects.clear();
        
        int count = objectsPerThread + (i < remainder ? 1 : 0);
        for (int j = 0; j < count && currentIndex < allObjects.size(); j++)
        {
            threadData[i].objects.push_back(allObjects[currentIndex++]);
        }
    }
    
    // Process objects in parallel
    if (threadPool)
    {
        std::vector<std::function<void()>> tasks;
        for (int i = 0; i < numThreads; i++)
        {
            tasks.push_back([this, i]() { BuildCommandsForThread(i); });
        }
        threadPool->Run(tasks.size(), tasks.data());
    }
    else
    {
        // Fallback to sequential processing
        for (int i = 0; i < numThreads; i++)
        {
            BuildCommandsForThread(i);
        }
    }
}

void FsMultiThreadRenderer::BuildCommandsForThread(int threadId)
{
    if (threadId >= threadData.size())
        return;
    
    auto& data = threadData[threadId];
    data.commandQueue.BeginBatch();
    
    for (const auto& obj : data.objects)
    {
        if (!obj.visible)
            continue;
        
        switch (obj.type)
        {
        case FsRenderableObject::AIRPLANE:
            // Skip airplane drawing for now due to header issues
            // Will be implemented after fixing forward declarations
            break;
            
        case FsRenderableObject::GROUND:
            // Skip ground drawing for now due to header issues
            // Will be implemented after fixing forward declarations
            break;
            
        case FsRenderableObject::TERRAIN:
            // Skip terrain drawing for now due to header issues
            // Will be implemented after fixing forward declarations
            break;
        }
    }
    
    data.commandQueue.EndBatch();
    data.finished = YSTRUE;
}

void FsMultiThreadRenderer::MergeCommandQueues()
{
    // Wait for all threads to finish
    bool allFinished = false;
    while (!allFinished)
    {
        allFinished = true;
        for (const auto& data : threadData)
        {
            if (!data.finished)
            {
                allFinished = false;
                break;
            }
        }
        if (!allFinished)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    // Merge commands from all threads into global queues
    for (const auto& data : threadData)
    {
        // This is a simplified merge - in practice, you'd copy commands
        // from thread-local queues to the global command manager
        // For now, we'll execute thread commands directly
        const_cast<FsRenderCommandQueue&>(data.commandQueue).ExecuteCommands();
    }
}

// LOD calculation methods
int FsMultiThreadRenderer::CalculateAirplaneLOD(const FsAirplane* airplane, double distance) const
{
    if (distance < lodNearDistance)
        return 0; // Full detail
    else if (distance < lodMediumDistance)
        return 1; // Medium detail
    else if (distance < lodFarDistance)
        return 2; // Low detail
    else
        return 3; // Very low detail or cull
}

int FsMultiThreadRenderer::CalculateGroundLOD(const FsGround* ground, double distance) const
{
    if (distance < lodNearDistance)
        return 0;
    else if (distance < lodMediumDistance)
        return 1;
    else if (distance < lodFarDistance)
        return 2;
    else
        return 3;
}

int FsMultiThreadRenderer::CalculateTerrainLOD(const FsField* field, const YsVec3& pos, double distance) const
{
    // Terrain LOD based on distance from camera
    if (distance < lodNearDistance * 2.0)
        return 0;
    else if (distance < lodMediumDistance * 2.0)
        return 1;
    else
        return 2;
}

// Culling methods - disabled since culling is already handled by the game
YSBOOL FsMultiThreadRenderer::CullAirplane(const FsAirplane* airplane, const FsRenderContext& context) const
{
    // Culling is already handled by the game
    return YSFALSE;
}

YSBOOL FsMultiThreadRenderer::CullGround(const FsGround* ground, const FsRenderContext& context) const
{
    // Culling is already handled by the game
    return YSFALSE;
}

YSBOOL FsMultiThreadRenderer::CullTerrain(const FsField* field, const YsVec3& pos, const FsRenderContext& context) const
{
    // Culling is already handled by the game
    return YSFALSE;
}
</edits>

// Configuration methods
void FsMultiThreadRenderer::SetLODDistances(double near, double medium, double far)
{
    lodNearDistance = near;
    lodMediumDistance = medium;
    lodFarDistance = far;
}

void FsMultiThreadRenderer::SetCullingEnabled(YSBOOL enabled)
{
    cullingEnabled = enabled;
}

void FsMultiThreadRenderer::SetLODEnabled(YSBOOL enabled)
{
    lodEnabled = enabled;
}

void FsMultiThreadRenderer::SetFrustumCullingEnabled(YSBOOL enabled)
{
    frustumCullingEnabled = enabled;
}

void FsMultiThreadRenderer::SetOcclusionCullingEnabled(YSBOOL enabled)
{
    occlusionCullingEnabled = enabled;
}

void FsMultiThreadRenderer::EnableInstancing(YSBOOL enable)
{
    instancingEnabled = enable;
}

void FsMultiThreadRenderer::SetInstanceBatchSize(int size)
{
    instanceBatchSize = size;
}

void FsMultiThreadRenderer::EnableCommandBuffering(YSBOOL enable)
{
    commandBufferingEnabled = enable;
}

void FsMultiThreadRenderer::SetCommandBufferSize(int size)
{
    commandBufferSize = size;
}

FsRenderCommandQueue& FsMultiThreadRenderer::GetOpaqueQueue()
{
    return commandManager->GetOpaqueQueue();
}

FsRenderCommandQueue& FsMultiThreadRenderer::GetTransparentQueue()
{
    return commandManager->GetTransparentQueue();
}

FsRenderCommandQueue& FsMultiThreadRenderer::GetHudQueue()
{
    return commandManager->GetHudQueue();
}

void FsMultiThreadRenderer::ResetStats()
{
    frameStats = PerformanceStats();
}

// FsRenderingSystem implementation
FsRenderingSystem& FsRenderingSystem::GetInstance()
{
    if (!instance)
    {
        instance = new FsRenderingSystem();
    }
    return *instance;
}

void FsRenderingSystem::Initialize(int numThreads)
{
    auto& system = GetInstance();
    if (system.renderer)
    {
        system.renderer->Initialize(numThreads);
    }
}

void FsRenderingSystem::Shutdown()
{
    if (instance && instance->renderer)
    {
        instance->renderer->Shutdown();
        delete instance->renderer;
        instance->renderer = nullptr;
    }
}

FsRenderingSystem::FsRenderingSystem()
{
    renderer = new FsMultiThreadRenderer();
}

FsRenderingSystem::~FsRenderingSystem()
{
    delete renderer;
}

// Utility functions implementation
namespace FsRenderUtils
{
    void ConvertAirplaneDrawToCommands(
        FsRenderCommandQueue& queue,
        const FsAirplane* airplane,
        const FsRenderContext& context,
        int visualMode)
    {
        if (!airplane)
            return;
        
        // This would convert the airplane's existing draw calls to commands
        // For now, we'll create a placeholder command
        YsMatrix4x4 transform;
        airplane->GetMatrix(transform);
        
        // Add commands for airplane rendering
        queue.AddSetBlendModeCommand(FS_BLEND_ALPHA);
        queue.AddSetTransformCommand(transform);
        
        // Would add actual VBO draw commands here based on airplane's visual data
    }
    
    void ConvertGroundDrawToCommands(
        FsRenderCommandQueue& queue,
        const FsGround* ground,
        const FsRenderContext& context,
        int visualMode)
    {
        if (!ground)
            return;
        
        YsMatrix4x4 transform;
        ground->GetMatrix(transform);
        
        queue.AddSetBlendModeCommand(FS_BLEND_NONE);
        queue.AddSetTransformCommand(transform);
        
        // Would add actual VBO draw commands here
    }
    
    void ConvertTerrainDrawToCommands(
        FsRenderCommandQueue& queue,
        const FsField* field,
        const FsRenderContext& context)
    {
        if (!field)
            return;
        
        YsMatrix4x4 identity;
        identity.LoadIdentity();
        
        queue.AddSetBlendModeCommand(FS_BLEND_NONE);
        queue.AddSetTransformCommand(identity);
        
        // Would add terrain chunk commands here
    }
    
    void BatchSimilarObjects(std::vector<FsRenderableObject>& objects)
    {
        // Sort objects by type and material for better batching
        std::sort(objects.begin(), objects.end(), 
                 [](const FsRenderableObject& a, const FsRenderableObject& b) {
                     if (a.type != b.type)
                         return a.type < b.type;
                     return a.transparent < b.transparent;
                 });
    }
    
    void SortObjectsByDistance(std::vector<FsRenderableObject>& objects, const YsVec3& viewPoint)
    {
        std::sort(objects.begin(), objects.end(),
                 [&viewPoint](const FsRenderableObject& a, const FsRenderableObject& b) {
                     double distA = (a.position - viewPoint).GetSquareLength();
                     double distB = (b.position - viewPoint).GetSquareLength();
                     return distA < distB;
                 });
    }
    
    void SortObjectsByState(std::vector<FsRenderableObject>& objects)
    {
        BatchSimilarObjects(objects);
    }
    
    double CalculateObjectDistance(const FsRenderableObject& obj, const YsVec3& viewPoint)
    {
        return (obj.position - viewPoint).GetLength();
    }
    
    YsVec3 GetObjectBoundingSphere(const FsRenderableObject& obj, double& radius)
    {
        radius = 50.0; // Default radius
        
        switch (obj.type)
        {
        case FsRenderableObject::AIRPLANE:
            radius = 50.0;
            break;
        case FsRenderableObject::GROUND:
            radius = 30.0;
            break;
        case FsRenderableObject::TERRAIN:
            radius = 1000.0;
            break;
        default:
            radius = 25.0;
            break;
        }
        
        return obj.position;
    }
    
    YSBOOL IsObjectVisible(const FsRenderableObject& obj, const FsRenderContext& context)
    {
        double radius;
        YsVec3 center = GetObjectBoundingSphere(obj, radius);
        return context.IsInFrustum(center, radius);
    }
}