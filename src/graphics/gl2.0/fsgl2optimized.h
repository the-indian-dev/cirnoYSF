#ifndef FSGL2OPTIMIZED_IS_INCLUDED
#define FSGL2OPTIMIZED_IS_INCLUDED

#include <ysgl.h>
#include <ysglslsharedrenderer.h>
#include <ysglbuffermanager.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include "../common/fsrendercommandqueue.h"

// Forward declarations
class FsSimulation;
class FsProjection;
class FsAirplane;
class FsGround;
class FsField;
class YsGLParticleManager;
class FsExplosionHolder;

// Optimized texture manager for reducing texture switches
class FsOptimizedTextureManager
{
private:
    struct TextureInfo
    {
        GLuint textureId;
        int width, height;
        GLenum format;
        int lastUsedFrame;
        int bindCount;
        
        TextureInfo() : textureId(0), width(0), height(0), format(GL_RGBA), lastUsedFrame(0), bindCount(0) {}
    };
    
    std::unordered_map<std::string, TextureInfo> textureCache;
    std::vector<GLuint> textureAtlas;
    GLuint currentBoundTexture;
    int currentFrame;
    
public:
    FsOptimizedTextureManager();
    ~FsOptimizedTextureManager();
    
    GLuint LoadTexture(const char* filename, YSBOOL generateMipmaps = YSFALSE);
    GLuint CreateTextureAtlas(const std::vector<std::string>& filenames);
    void BindTexture(GLuint textureId);
    void OptimizeTextures(); // Remove unused textures
    void BeginFrame();
    int GetTextureBindCount() const;
    void ResetStats();
    
    static FsOptimizedTextureManager& GetInstance();
    
private:
    static FsOptimizedTextureManager* instance;
};

// VBO batch manager for reducing draw calls
class FsVBOBatchManager
{
private:
    struct BatchData
    {
        YsGLBufferManager::Handle vboHandle;
        GLenum primitiveType;
        GLuint textureId;
        YsMatrix4x4 transform;
        int vertexCount;
        float distance;
        YSBOOL transparent;
    };
    
    std::vector<BatchData> opaqueBatches;
    std::vector<BatchData> transparentBatches;
    std::vector<BatchData> currentBatch;
    
    // Instancing support
    struct InstanceData
    {
        YsMatrix4x4 transforms[100]; // Max 100 instances per batch
        int count;
        YsGLBufferManager::Handle instanceVBO;
    };
    
    std::unordered_map<GLuint, InstanceData> instanceBatches;
    
    // Performance tracking
    std::atomic<int> drawCallsSaved;
    std::atomic<int> totalBatches;
    
public:
    FsVBOBatchManager();
    ~FsVBOBatchManager();
    
    void BeginFrame();
    void AddToBatch(YsGLBufferManager::Handle vbo, GLenum primitive, GLuint texture, 
                   const YsMatrix4x4& transform, int vertexCount, float distance, YSBOOL transparent);
    void AddInstance(GLuint objectId, const YsMatrix4x4& transform);
    void FlushBatches();
    void FlushTransparentBatches();
    
    // Statistics
    int GetDrawCallsSaved() const { return drawCallsSaved.load(); }
    int GetTotalBatches() const { return totalBatches.load(); }
    void ResetStats();
    
    static FsVBOBatchManager& GetInstance();
    
private:
    void SortBatches();
    void ExecuteBatch(const BatchData& batch);
    void ExecuteInstanceBatch(GLuint objectId, const InstanceData& instances);
    
    static FsVBOBatchManager* instance;
};

// Optimized shader manager
class FsOptimizedShaderManager
{
private:
    struct ShaderProgram
    {
        GLuint programId;
        GLint uniformLocations[32];
        int lastUsedFrame;
        
        ShaderProgram() : programId(0), lastUsedFrame(0)
        {
            for (int i = 0; i < 32; i++) uniformLocations[i] = -1;
        }
    };
    
    std::unordered_map<std::string, ShaderProgram> shaderCache;
    GLuint currentShader;
    int currentFrame;
    
public:
    FsOptimizedShaderManager();
    ~FsOptimizedShaderManager();
    
    GLuint LoadShader(const char* vertexSource, const char* fragmentSource, const char* name);
    void UseShader(const char* name);
    void SetUniform(const char* shaderName, const char* uniformName, const float* value, int count);
    void SetUniformMatrix(const char* shaderName, const char* uniformName, const YsMatrix4x4& matrix);
    
    void BeginFrame();
    static FsOptimizedShaderManager& GetInstance();
    
private:
    GLuint CompileShader(const char* source, GLenum type);
    GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader);
    
    static FsOptimizedShaderManager* instance;
};

// Main optimized rendering interface
class FsOptimizedGL2Renderer
{
private:
    FsOptimizedTextureManager* textureManager;
    FsVBOBatchManager* batchManager;
    FsOptimizedShaderManager* shaderManager;
    FsRenderCommandManager* commandManager;
    
    // Performance tracking
    struct FrameStats
    {
        int drawCalls;
        int textureChanges;
        int shaderChanges;
        int verticesDrawn;
        int trianglesDrawn;
        double frameTime;
        double cpuTime;
        double gpuTime;
        
        FrameStats() : drawCalls(0), textureChanges(0), shaderChanges(0), 
                      verticesDrawn(0), trianglesDrawn(0), frameTime(0.0), 
                      cpuTime(0.0), gpuTime(0.0) {}
    };
    
    FrameStats currentFrameStats;
    FrameStats averageStats;
    int frameCount;
    
    // State tracking
    YsMatrix4x4 currentModelView;
    YsMatrix4x4 currentProjection;
    GLuint currentTexture;
    GLuint currentShader;
    GLenum currentBlendMode;
    YSBOOL depthTestEnabled;
    YSBOOL cullFaceEnabled;
    
    // Multi-threading support
    YSBOOL useMultiThreading;
    int numRenderThreads;
    
public:
    FsOptimizedGL2Renderer();
    ~FsOptimizedGL2Renderer();
    
    void Initialize();
    void Shutdown();
    
    // Frame management
    void BeginFrame();
    void EndFrame();
    
    // Optimized rendering functions (simplified to avoid forward declaration issues)
    void OptimizedDrawAirplane(const void* airplane, const YsVec3& viewPoint, 
                              const YsMatrix4x4& viewMatrix, const void* proj, int visualMode);
    void OptimizedDrawGround(const void* ground, const YsVec3& viewPoint,
                           const YsMatrix4x4& viewMatrix, const void* proj, int visualMode);
    void OptimizedDrawField(const void* field, const YsVec3& viewPoint,
                          const YsMatrix4x4& viewMatrix, const void* proj);
    
    // Batch operations
    void FlushOpaqueBatches();
    void FlushTransparentBatches();
    
    // Configuration
    void SetMultiThreadingEnabled(YSBOOL enabled);
    void SetRenderThreadCount(int count);
    void SetLODEnabled(YSBOOL enabled);
    void SetFrustumCullingEnabled(YSBOOL enabled);
    void SetTextureCachingEnabled(YSBOOL enabled);
    void SetBatchingEnabled(YSBOOL enabled);
    
    // Statistics and debugging
    const FrameStats& GetFrameStats() const { return currentFrameStats; }
    const FrameStats& GetAverageStats() const { return averageStats; }
    void ResetStats();
    void PrintPerformanceReport() const;
    
    // Utility functions
    void PreloadTextures(const std::vector<std::string>& filenames);
    void PrecompileShaders();
    void WarmupPipeline();
    
    static FsOptimizedGL2Renderer& GetInstance();
    
private:
    void UpdateAverageStats();
    void OptimizeRenderState();
    void CheckGLError(const char* operation) const;
    
    // LOD helpers
    int CalculateLODLevel(float distance) const;
    YSBOOL ShouldCullObject(const YsVec3& position, float radius, const YsMatrix4x4& viewMatrix, const void* proj) const;
    
    static FsOptimizedGL2Renderer* instance;
};

// Integration macros for easy adoption
#define FS_USE_OPTIMIZED_RENDERING 1

// Optimization macros are disabled for now to avoid compilation issues
// The optimized functions are called directly from the modified simulation code

// Shader sources for optimized rendering
namespace FsOptimizedShaders
{
    extern const char* vertexShaderSource;
    extern const char* fragmentShaderSource;
    extern const char* instancedVertexShaderSource;
    extern const char* terrainVertexShaderSource;
    extern const char* terrainFragmentShaderSource;
    extern const char* particleVertexShaderSource;
    extern const char* particleFragmentShaderSource;
}

// Performance monitoring
class FsRenderProfiler
{
private:
    struct ProfileData
    {
        double startTime;
        double endTime;
        const char* name;
    };
    
    std::vector<ProfileData> frameProfile;
    YSBOOL enabled;
    
public:
    FsRenderProfiler();
    
    void BeginProfile(const char* name);
    void EndProfile(const char* name);
    void BeginFrame();
    void EndFrame();
    void SetEnabled(YSBOOL enable) { enabled = enable; }
    void PrintReport() const;
    
    static FsRenderProfiler& GetInstance();
    
private:
    static FsRenderProfiler* instance;
};

#define FS_PROFILE_SCOPE(name) \
    FsRenderProfiler::GetInstance().BeginProfile(name); \
    struct ProfilerRAII { const char* n; ProfilerRAII(const char* name) : n(name) {} \
    ~ProfilerRAII() { FsRenderProfiler::GetInstance().EndProfile(n); } } _prof(name);

#endif