#pragma once

#include <cstdint>

// ======================================= //
//            Render Structures            //
// ======================================= //

/*
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
struct SwapChain
{
    bool vsync;
    void* window;
    void(*swap_buffers)(void*);
};

struct BufferDesc
{
    uint32_t size;
    // It is better to create enums for target and usage
    // but leave as it right now
    uint32_t target;
    uint32_t memory_usage;
};

struct Buffer
{
    uint32_t id;
    uint32_t target;
};

enum ShaderStage : uint8_t
{
    kShaderStageNone = 0,
    kShaderStageVert = 0x00000001,
    kShaderStageFrag = 0x00000002,
    kShaderStageGeom = 0x00000004,
    kShaderStageComp = 0x00000008,
};

struct ShaderStageDesc
{
    void* byte_code;
    uint32_t byte_code_size;
    const char* entry_point;
};

struct ShaderDesc
{
    ShaderStage stage;
    ShaderStageDesc vert;
    ShaderStageDesc frag;
    ShaderStageDesc geom;
    ShaderStageDesc comp;
};

struct Shader
{
    ShaderStage stages;
    uint32_t program;
};

// ======================================= //
//            Render Functions             //
// ======================================= //

#define DECLARE_YAR_RENDER_FUNC(ret, name, ...) \
using name##_fn = ret(*)(__VA_ARGS__);          \
extern name##_fn name;                          \

DECLARE_YAR_RENDER_FUNC(void, add_swapchain, bool vsync, SwapChain** swapchain);
DECLARE_YAR_RENDER_FUNC(void, add_buffer,  BufferDesc* desc, Buffer** buffer);
DECLARE_YAR_RENDER_FUNC(void, add_shader, ShaderDesc* desc, Shader**);
DECLARE_YAR_RENDER_FUNC(void, remove_buffer, Buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void, map_buffer, Buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void, unmap_buffer, Buffer* buffer);

void init_render();
