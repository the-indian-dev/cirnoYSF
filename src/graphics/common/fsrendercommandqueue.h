#ifndef FSRENDERCOMMANDQUEUE_IS_INCLUDED
#define FSRENDERCOMMANDQUEUE_IS_INCLUDED

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <ysgl.h>
#include <ysclass.h>

// Forward declarations
class YsGLBufferManager;
class YsTextureManager;

enum FsRenderCommandType
{
    FS_RENDER_CMD_DRAW_VBO,
    FS_RENDER_CMD_DRAW_IMMEDIATE,
    FS_RENDER_CMD_SET_TEXTURE,
    FS_RENDER_CMD_SET_SHADER_PARAMS,
    FS_RENDER_CMD_SET_TRANSFORM,
    FS_RENDER_CMD_SET_BLEND_MODE,
    FS_RENDER_CMD_DEPTH_TEST,
    FS_RENDER_CMD_CULL_FACE,
    FS_RENDER_CMD_VIEWPORT
};

enum FsBlendMode
{
    FS_BLEND_NONE,
    FS_BLEND_ALPHA,
    FS_BLEND_ADDITIVE,
    FS_BLEND_MULTIPLY
};

struct FsRenderCommand
{
    FsRenderCommandType type;
    int priority;
    double distance; // For depth sorting
    
    union
    {
        struct
        {
            YsGLBufferManager::Handle vboHandle;
            GLenum primitiveType;
            int vertexCount;
            GLuint textureId;
            YsMatrix4x4 transform;
        } drawVbo;
        
        struct
        {
            GLuint textureId;
            int textureUnit;
        } setTexture;
        
        struct
        {
            YsMatrix4x4 matrix;
        } setTransform;
        
        struct
        {
            FsBlendMode mode;
        } setBlend;
        
        struct
        {
            YSBOOL enable;
        } depthTest;
        
        struct
        {
            YSBOOL enable;
            GLenum mode;
        } cullFace;
        
        struct
        {
            int x, y, width, height;
        } viewport;
    } data;
    
    FsRenderCommand() : type(FS_RENDER_CMD_DRAW_VBO), priority(0), distance(0.0) {}
};

class FsRenderCommandQueue
{
private:
    std::vector<FsRenderCommand> commands;
    std::vector<FsRenderCommand> transparentCommands;
    std::mutex commandMutex;
    std::atomic<bool> sorted;
    
    // State tracking for optimization
    GLuint currentTexture;
    YsMatrix4x4 currentTransform;
    FsBlendMode currentBlend;
    YSBOOL depthTestEnabled;
    YSBOOL cullFaceEnabled;
    
    void SortCommandsByState();
    void SortTransparentCommandsByDistance();
    
public:
    FsRenderCommandQueue();
    ~FsRenderCommandQueue();
    
    // Thread-safe command adding
    void AddDrawVboCommand(
        YsGLBufferManager::Handle vboHandle,
        GLenum primitiveType,
        int vertexCount,
        GLuint textureId,
        const YsMatrix4x4& transform,
        double distance = 0.0,
        int priority = 0,
        YSBOOL transparent = YSFALSE
    );
    
    void AddSetTextureCommand(GLuint textureId, int textureUnit = 0);
    void AddSetTransformCommand(const YsMatrix4x4& matrix);
    void AddSetBlendModeCommand(FsBlendMode mode);
    void AddDepthTestCommand(YSBOOL enable);
    void AddCullFaceCommand(YSBOOL enable, GLenum mode = GL_BACK);
    void AddViewportCommand(int x, int y, int width, int height);
    
    // Batch operations
    void BeginBatch();
    void EndBatch();
    
    // Execution
    void ExecuteCommands();
    void ExecuteTransparentCommands();
    
    // Utility
    void Clear();
    void Reserve(size_t count);
    size_t GetCommandCount() const;
    size_t GetTransparentCommandCount() const;
    
    // Statistics
    struct Stats
    {
        int textureChanges;
        int transformChanges;
        int blendChanges;
        int drawCalls;
        int verticesDrawn;
        
        Stats() : textureChanges(0), transformChanges(0), blendChanges(0), drawCalls(0), verticesDrawn(0) {}
        void Reset() { *this = Stats(); }
    };
    
    Stats GetAndResetStats();
    
private:
    Stats currentStats;
    bool batchMode;
    
    void ExecuteCommand(const FsRenderCommand& cmd);
    void OptimizeStateChange(const FsRenderCommand& cmd);
};

// Thread-safe command builders for common objects
class FsRenderCommandBuilder
{
public:
    static void BuildAirplaneCommands(
        FsRenderCommandQueue& queue,
        const class FsAirplane* airplane,
        const YsVec3& viewPoint,
        const YsMatrix4x4& viewMatrix,
        const YsMatrix4x4& projMatrix,
        YSBOOL drawTransparent
    );
    
    static void BuildGroundCommands(
        FsRenderCommandQueue& queue,
        const class FsGround* ground,
        const YsVec3& viewPoint,
        const YsMatrix4x4& viewMatrix,
        const YsMatrix4x4& projMatrix,
        YSBOOL drawTransparent
    );
    
    static void BuildTerrainCommands(
        FsRenderCommandQueue& queue,
        const class FsField* field,
        const YsVec3& viewPoint,
        const YsMatrix4x4& viewMatrix,
        const YsMatrix4x4& projMatrix
    );
    
    static void BuildParticleCommands(
        FsRenderCommandQueue& queue,
        const class YsGLParticleManager& particleManager,
        const YsVec3& viewPoint,
        const YsMatrix4x4& viewMatrix,
        const YsMatrix4x4& projMatrix
    );
};

// Global command queue manager
class FsRenderCommandManager
{
private:
    static FsRenderCommandManager* instance;
    
    FsRenderCommandQueue opaqueQueue;
    FsRenderCommandQueue transparentQueue;
    FsRenderCommandQueue hudQueue;
    FsRenderCommandQueue shadowQueue;
    
    std::mutex managerMutex;
    
public:
    static FsRenderCommandManager& GetInstance();
    
    FsRenderCommandQueue& GetOpaqueQueue() { return opaqueQueue; }
    FsRenderCommandQueue& GetTransparentQueue() { return transparentQueue; }
    FsRenderCommandQueue& GetHudQueue() { return hudQueue; }
    FsRenderCommandQueue& GetShadowQueue() { return shadowQueue; }
    
    void ClearAllQueues();
    void ExecuteAllQueues();
    
    struct FrameStats
    {
        FsRenderCommandQueue::Stats opaque;
        FsRenderCommandQueue::Stats transparent;
        FsRenderCommandQueue::Stats hud;
        FsRenderCommandQueue::Stats shadow;
        double buildTime;
        double executeTime;
    };
    
    FrameStats GetFrameStats();
};

#endif