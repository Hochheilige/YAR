#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <queue>
#include <functional>

// ======================================= //
//            Render Structures            //
// ======================================= //

/*
    Need to make all this things more abstract in case of using different 
    render api, or just add defines to separate parts of this structs

    Plans to add:
        - Render Pipeline that should include:
            - Blend state
            - Rasterization state
            - Depth|Stencil state
            - Input Assembly 
            - Vertex Input
            - Viewport|Scissor
            - Multisampling (?)
        - Shader Reflection
            - Need to add spirv dependency to make it
            - want to use it 
        - Buffers and Textures
            - If we speak about uniforms there probably should be
                a thing like DescriptorSet
            - Another thing - Vertex and Index buffers
        - Command Queue 
            - as far as OpenGL render immediately 
*/

// Interesting thing got it from The-Forge
#define MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)                                                                        \
	static inline ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b) { return (ENUM_TYPE)((TYPE)(a) | (TYPE)(b)); } \
	static inline ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b) { return (ENUM_TYPE)((TYPE)(a) & (TYPE)(b)); } \
	static inline ENUM_TYPE operator|=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a | b); }                      \
	static inline ENUM_TYPE operator&=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a & b); }

constexpr uint8_t kMaxVertexAttribCount = 16;

struct SwapChain
{
    // this implementation is probably good only for OpenGL
    bool vsync;
    void* window;
    void(*swap_buffers)(void*);
};

enum BufferFlag : uint8_t
{
    kBufferFlagNone          = 0, 
    kBufferFlagDynamic       = 0x00000001,
    kBufferFlagMapRead       = 0x00000002,
    kBufferFlagMapWrite      = 0x00000004,
    kBufferFlagMapReadWrite  = 0x00000008,
    kBufferFlagMapPersistent = 0x00000010,
    kBufferFlagMapCoherent   = 0x00000020,
    kBufferFlagMax           = 6
};
MAKE_ENUM_FLAG(uint8_t, BufferFlag);

struct BufferDesc
{
    uint32_t size;
    BufferFlag flags;
};

struct Buffer
{
    uint32_t id;
    BufferFlag flags;
};

enum ShaderStage : uint8_t
{
    kShaderStageNone = 0,
    kShaderStageVert = 0x00000001,
    kShaderStageFrag = 0x00000002,
    kShaderStageGeom = 0x00000004,
    kShaderStageComp = 0x00000008,
    kShaderStageTese = 0x00000010,
    kShaderStageMax = 5
};
MAKE_ENUM_FLAG(uint8_t, ShaderStage);

struct ShaderStageLoadDesc
{
    std::string file_name;
    std::string entry_point;
    ShaderStage stage;
    // maybe need to add some shader macros support here
};

struct ShaderLoadDesc
{
    std::array<ShaderStageLoadDesc, ShaderStage::kShaderStageMax> stages;
};

struct ShaderStageDesc
{
    void* byte_code;
    uint32_t byte_code_size;
    std::string_view entry_point;
    uint32_t shader; // Only for gl
};

struct ShaderDesc
{
    ShaderStage stages;
    ShaderStageDesc vert;
    ShaderStageDesc frag;
    ShaderStageDesc geom;
    ShaderStageDesc comp;
};

struct Shader
{
    ShaderStage stages;
    uint32_t program;
    // Going to add spirv reflection but later
    void* reflection; 
};

struct VertexAttrib
{
    uint32_t size;
    uint32_t format;
    uint32_t binding;
    uint32_t offset;    
};

struct VertexLayout
{
    uint32_t attrib_count;
    VertexAttrib attribs[kMaxVertexAttribCount];
};

struct PipelineDesc
{
    Shader* shader;
    VertexLayout* vertex_layout;
    /*
    has to contain:
        * Shaders (probably shader reflection as well)
            if Vulkan is a reference to this we have to store shader
            stages count and stages as well
        * Root Signature (?)
        * Topology - Input Assembly
        * Vertex Attributes
        * Viewport state
        * Rasterization state
        * Multisampling state
        * DepthStencil state
        * Color Blend state
        * Layout - to work with resources
*/
};

struct Pipeline
{
    // it probably should be something graphics API specific
    // but for now it will be OpenGL specific
    Shader* shader;
    uint32_t vao;
};

struct CmdQueueDesc
{
    uint8_t dummy;
};

struct CmdBuffer;

struct CmdQueue
{
    std::vector<CmdBuffer*> queue;
};

struct CmdBufferDesc
{
    CmdQueue* current_queue;
};

struct CmdBuffer
{
    using Command = std::function<void()>;
    std::vector<Command> commands;
};

// ======================================= //
//            Load Functions               //
// ======================================= //

#define DECLARE_YAR_LOAD_FUNC(ret, name, ...)  \
using load_##name##_fn = ret(*)(__VA_ARGS__);  \
extern load_##name##_fn load_##name;                  \

DECLARE_YAR_LOAD_FUNC(void, shader, ShaderLoadDesc* desc, ShaderDesc** out);

// ======================================= //
//            Render Functions             //
// ======================================= //

#define DECLARE_YAR_RENDER_FUNC(ret, name, ...)  \
using name##_fn = ret(*)(__VA_ARGS__);          \
extern name##_fn name;                          \

DECLARE_YAR_RENDER_FUNC(void, add_swapchain, bool vsync, SwapChain** swapchain);
DECLARE_YAR_RENDER_FUNC(void, add_buffer,  BufferDesc* desc, Buffer** buffer);
DECLARE_YAR_RENDER_FUNC(void, add_shader, ShaderDesc* desc, Shader** shader);
DECLARE_YAR_RENDER_FUNC(void, add_pipeline, PipelineDesc* desc, Pipeline** pipeline);
DECLARE_YAR_RENDER_FUNC(void, add_queue, CmdQueueDesc* desc, CmdQueue** queue);
DECLARE_YAR_RENDER_FUNC(void, add_cmd, CmdBufferDesc* desc, CmdBuffer** cmd);
DECLARE_YAR_RENDER_FUNC(void, remove_buffer, Buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void*, map_buffer, Buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void, unmap_buffer, Buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void, cmd_bind_pipeline, CmdBuffer* cmd, Pipeline* pipeline);
DECLARE_YAR_RENDER_FUNC(void, cmd_bind_vertex_buffer, CmdBuffer* cmd, Buffer* buffer, uint32_t offset, uint32_t stride);
DECLARE_YAR_RENDER_FUNC(void, cmd_bind_index_buffer, CmdBuffer* cmd, Buffer* buffer); // maybe for other render api there should be more params
DECLARE_YAR_RENDER_FUNC(void, cmd_draw, CmdBuffer* cmd, uint32_t first_vertex, uint32_t count);
DECLARE_YAR_RENDER_FUNC(void, cmd_draw_indexed, CmdBuffer* cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex);
DECLARE_YAR_RENDER_FUNC(void, queue_submit, CmdQueue* queue);

void init_render();
