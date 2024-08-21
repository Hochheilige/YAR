#pragma once

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

// ======================================= //
//            Render Functions             //
// ======================================= //

#define DECLARE_YAR_RENDER_FUNC(ret, name, ...) \
using name##_fn = ret(*)(__VA_ARGS__);          \
extern name##_fn name;                          \

DECLARE_YAR_RENDER_FUNC(void, add_swapchain, SwapChain* swapchain, bool vsync);

void init_render();