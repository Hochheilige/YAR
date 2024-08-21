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

// ======================================= //
//            Render Functions             //
// ======================================= //

#define DECLARE_YAR_RENDER_FUNC(ret, name, ...) \
using name##_fn = ret(*)(__VA_ARGS__);          \
extern name##_fn name;                          \

DECLARE_YAR_RENDER_FUNC(void, add_swapchain, SwapChain* swapchain, bool vsync);
DECLARE_YAR_RENDER_FUNC(void, add_buffer, Buffer* buffer, BufferDesc* desc);
DECLARE_YAR_RENDER_FUNC(void, remove_buffer, Buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void, map_buffer, Buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void, unmap_buffer, Buffer* buffer);

void init_render();
