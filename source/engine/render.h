#pragma once

#include <cstdint>
#include <string>
#include <string_view>

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

// Interesting thing got it from The-Forge
#define MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)                                                                        \
	static inline ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b) { return (ENUM_TYPE)((TYPE)(a) | (TYPE)(b)); } \
	static inline ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b) { return (ENUM_TYPE)((TYPE)(a) & (TYPE)(b)); } \
	static inline ENUM_TYPE operator|=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a | b); }                      \
	static inline ENUM_TYPE operator&=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a & b); }

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
    const std::string_view file_name;
    const std::string_view entry_point;
    // maybe need to add some shader macros support here
};

struct ShaderLoadDesc
{
    ShaderStageLoadDesc stages[ShaderStage::kShaderStageMax];
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
DECLARE_YAR_RENDER_FUNC(void, remove_buffer, Buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void, map_buffer, Buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void, unmap_buffer, Buffer* buffer);

void init_render();
