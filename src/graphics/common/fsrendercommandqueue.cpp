#include "fsrendercommandqueue.h"
#include <algorithm>
#include <chrono>
#include <ysglbuffermanager.h>
#include <ysglslsharedrenderer.h>

FsRenderCommandManager* FsRenderCommandManager::instance = nullptr;

FsRenderCommandQueue::FsRenderCommandQueue()
    : sorted(false), currentTexture(0), currentBlend(FS_BLEND_NONE), 
      depthTestEnabled(YSTRUE), cullFaceEnabled(YSTRUE), batchMode(false)
{
    commands.reserve(10000);
    transparentCommands.reserve(1000);
    currentTransform.LoadIdentity();
}

FsRenderCommandQueue::~FsRenderCommandQueue()
{
}

void FsRenderCommandQueue::AddDrawVboCommand(
    YsGLBufferManager::Handle vboHandle,
    GLenum primitiveType,
    int vertexCount,
    GLuint textureId,
    const YsMatrix4x4& transform,
    double distance,
    int priority,
    YSBOOL transparent)
{
    FsRenderCommand cmd;
    cmd.type = FS_RENDER_CMD_DRAW_VBO;
    cmd.priority = priority;
    cmd.distance = distance;
    cmd.data.drawVbo.vboHandle = vboHandle;
    cmd.data.drawVbo.primitiveType = primitiveType;
    cmd.data.drawVbo.vertexCount = vertexCount;
    cmd.data.drawVbo.textureId = textureId;
    cmd.data.drawVbo.transform = transform;
    
    std::lock_guard<std::mutex> lock(commandMutex);
    if (transparent == YSTRUE)
    {
        transparentCommands.push_back(cmd);
    }
    else
    {
        commands.push_back(cmd);
    }
    sorted = false;
}

void FsRenderCommandQueue::AddSetTextureCommand(GLuint textureId, int textureUnit)
{
    FsRenderCommand cmd;
    cmd.type = FS_RENDER_CMD_SET_TEXTURE;
    cmd.priority = 1000; // High priority for state changes
    cmd.data.setTexture.textureId = textureId;
    cmd.data.setTexture.textureUnit = textureUnit;
    
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
    sorted = false;
}

void FsRenderCommandQueue::AddSetTransformCommand(const YsMatrix4x4& matrix)
{
    FsRenderCommand cmd;
    cmd.type = FS_RENDER_CMD_SET_TRANSFORM;
    cmd.priority = 1000;
    cmd.data.setTransform.matrix = matrix;
    
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
    sorted = false;
}

void FsRenderCommandQueue::AddSetBlendModeCommand(FsBlendMode mode)
{
    FsRenderCommand cmd;
    cmd.type = FS_RENDER_CMD_SET_BLEND_MODE;
    cmd.priority = 1000;
    cmd.data.setBlend.mode = mode;
    
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
    sorted = false;
}

void FsRenderCommandQueue::AddDepthTestCommand(YSBOOL enable)
{
    FsRenderCommand cmd;
    cmd.type = FS_RENDER_CMD_DEPTH_TEST;
    cmd.priority = 1000;
    cmd.data.depthTest.enable = enable;
    
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
    sorted = false;
}

void FsRenderCommandQueue::AddCullFaceCommand(YSBOOL enable, GLenum mode)
{
    FsRenderCommand cmd;
    cmd.type = FS_RENDER_CMD_CULL_FACE;
    cmd.priority = 1000;
    cmd.data.cullFace.enable = enable;
    cmd.data.cullFace.mode = mode;
    
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
    sorted = false;
}

void FsRenderCommandQueue::AddViewportCommand(int x, int y, int width, int height)
{
    FsRenderCommand cmd;
    cmd.type = FS_RENDER_CMD_VIEWPORT;
    cmd.priority = 2000; // Highest priority
    cmd.data.viewport.x = x;
    cmd.data.viewport.y = y;
    cmd.data.viewport.width = width;
    cmd.data.viewport.height = height;
    
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
    sorted = false;
}

void FsRenderCommandQueue::BeginBatch()
{
    batchMode = true;
}

void FsRenderCommandQueue::EndBatch()
{
    batchMode = false;
    if (!sorted)
    {
        SortCommandsByState();
        SortTransparentCommandsByDistance();
        sorted = true;
    }
}

void FsRenderCommandQueue::SortCommandsByState()
{
    // Sort by priority first, then by texture ID to minimize state changes
    std::sort(commands.begin(), commands.end(), [](const FsRenderCommand& a, const FsRenderCommand& b) {
        if (a.priority != b.priority)
            return a.priority > b.priority;
        
        if (a.type == FS_RENDER_CMD_DRAW_VBO && b.type == FS_RENDER_CMD_DRAW_VBO)
        {
            return a.data.drawVbo.textureId < b.data.drawVbo.textureId;
        }
        
        return a.type < b.type;
    });
}

void FsRenderCommandQueue::SortTransparentCommandsByDistance()
{
    // Sort transparent objects back to front
    std::sort(transparentCommands.begin(), transparentCommands.end(), 
              [](const FsRenderCommand& a, const FsRenderCommand& b) {
                  return a.distance > b.distance;
              });
}

void FsRenderCommandQueue::ExecuteCommands()
{
    if (!sorted)
    {
        SortCommandsByState();
        sorted = true;
    }
    
    currentStats.Reset();
    
    for (const auto& cmd : commands)
    {
        ExecuteCommand(cmd);
    }
}

void FsRenderCommandQueue::ExecuteTransparentCommands()
{
    if (!sorted)
    {
        SortTransparentCommandsByDistance();
    }
    
    for (const auto& cmd : transparentCommands)
    {
        ExecuteCommand(cmd);
    }
}

void FsRenderCommandQueue::ExecuteCommand(const FsRenderCommand& cmd)
{
    switch (cmd.type)
    {
    case FS_RENDER_CMD_DRAW_VBO:
        {
            OptimizeStateChange(cmd);
            
            auto& bufMan = YsGLBufferManager::GetSharedBufferManager();
            auto unitPtr = bufMan.GetBufferUnit(cmd.data.drawVbo.vboHandle);
            
            if (unitPtr && unitPtr->GetState() != YsGLBufferManager::Unit::EMPTY)
            {
                // Set transform matrix
                YsGLSLUse3DRenderer(YsGLSLSharedFlat3DRenderer());
                YsGLSLSet3DRendererModelViewfv(YsGLSLSharedFlat3DRenderer(), cmd.data.drawVbo.transform.GetArray());
                
                // Draw the VBO
                auto actualBuffer = unitPtr->GetActualBuffer();
                if (actualBuffer != nullptr)
                {
                    actualBuffer->DrawPrimitiveVtxCol(
                        *YsGLSLSharedFlat3DRenderer(), 
                        cmd.data.drawVbo.primitiveType);
                }
                
                currentStats.drawCalls++;
                currentStats.verticesDrawn += cmd.data.drawVbo.vertexCount;
            }
        }
        break;
        
    case FS_RENDER_CMD_SET_TEXTURE:
        if (currentTexture != cmd.data.setTexture.textureId)
        {
            glActiveTexture(GL_TEXTURE0 + cmd.data.setTexture.textureUnit);
            glBindTexture(GL_TEXTURE_2D, cmd.data.setTexture.textureId);
            currentTexture = cmd.data.setTexture.textureId;
            currentStats.textureChanges++;
        }
        break;
        
    case FS_RENDER_CMD_SET_TRANSFORM:
        if (currentTransform != cmd.data.setTransform.matrix)
        {
            currentTransform = cmd.data.setTransform.matrix;
            currentStats.transformChanges++;
        }
        break;
        
    case FS_RENDER_CMD_SET_BLEND_MODE:
        if (currentBlend != cmd.data.setBlend.mode)
        {
            switch (cmd.data.setBlend.mode)
            {
            case FS_BLEND_NONE:
                glDisable(GL_BLEND);
                break;
            case FS_BLEND_ALPHA:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case FS_BLEND_ADDITIVE:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
            case FS_BLEND_MULTIPLY:
                glEnable(GL_BLEND);
                glBlendFunc(GL_DST_COLOR, GL_ZERO);
                break;
            }
            currentBlend = cmd.data.setBlend.mode;
            currentStats.blendChanges++;
        }
        break;
        
    case FS_RENDER_CMD_DEPTH_TEST:
        if (depthTestEnabled != cmd.data.depthTest.enable)
        {
            if (cmd.data.depthTest.enable == YSTRUE)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            depthTestEnabled = cmd.data.depthTest.enable;
        }
        break;
        
    case FS_RENDER_CMD_CULL_FACE:
        if (cullFaceEnabled != cmd.data.cullFace.enable)
        {
            if (cmd.data.cullFace.enable == YSTRUE)
            {
                glEnable(GL_CULL_FACE);
                glCullFace(cmd.data.cullFace.mode);
            }
            else
            {
                glDisable(GL_CULL_FACE);
            }
            cullFaceEnabled = cmd.data.cullFace.enable;
        }
        break;
        
    case FS_RENDER_CMD_VIEWPORT:
        glViewport(cmd.data.viewport.x, cmd.data.viewport.y, 
                  cmd.data.viewport.width, cmd.data.viewport.height);
        break;
    }
}

void FsRenderCommandQueue::OptimizeStateChange(const FsRenderCommand& cmd)
{
    if (cmd.type == FS_RENDER_CMD_DRAW_VBO)
    {
        // Automatically set texture if different
        if (currentTexture != cmd.data.drawVbo.textureId)
        {
            glBindTexture(GL_TEXTURE_2D, cmd.data.drawVbo.textureId);
            currentTexture = cmd.data.drawVbo.textureId;
            currentStats.textureChanges++;
        }
    }
}

void FsRenderCommandQueue::Clear()
{
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.clear();
    transparentCommands.clear();
    sorted = false;
}

void FsRenderCommandQueue::Reserve(size_t count)
{
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.reserve(count);
}

size_t FsRenderCommandQueue::GetCommandCount() const
{
    return commands.size();
}

size_t FsRenderCommandQueue::GetTransparentCommandCount() const
{
    return transparentCommands.size();
}

FsRenderCommandQueue::Stats FsRenderCommandQueue::GetAndResetStats()
{
    Stats result = currentStats;
    currentStats.Reset();
    return result;
}

// FsRenderCommandManager implementation
FsRenderCommandManager& FsRenderCommandManager::GetInstance()
{
    if (!instance)
    {
        instance = new FsRenderCommandManager();
    }
    return *instance;
}

void FsRenderCommandManager::ClearAllQueues()
{
    std::lock_guard<std::mutex> lock(managerMutex);
    opaqueQueue.Clear();
    transparentQueue.Clear();
    hudQueue.Clear();
    shadowQueue.Clear();
}

void FsRenderCommandManager::ExecuteAllQueues()
{
    std::lock_guard<std::mutex> lock(managerMutex);
    
    // Execute in proper order
    shadowQueue.ExecuteCommands();
    opaqueQueue.ExecuteCommands();
    transparentQueue.ExecuteTransparentCommands();
    hudQueue.ExecuteCommands();
}

FsRenderCommandManager::FrameStats FsRenderCommandManager::GetFrameStats()
{
    FrameStats stats;
    stats.opaque = opaqueQueue.GetAndResetStats();
    stats.transparent = transparentQueue.GetAndResetStats();
    stats.hud = hudQueue.GetAndResetStats();
    stats.shadow = shadowQueue.GetAndResetStats();
    return stats;
}