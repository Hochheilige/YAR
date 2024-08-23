#include "../render.h"
#include "../window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <string_view>
#include <filesystem>
#include <iostream>
#include <fstream>

// ======================================= //
//            Utils Functions             //
// ======================================= //
uint32_t util_shader_stage_to_gl_stage(ShaderStage stage)
{
    switch (stage)
    {
    case kShaderStageVert:
        return GL_VERTEX_SHADER;
    case kShaderStageFrag:
        return GL_FRAGMENT_SHADER;
    case kShaderStageGeom:
        return GL_GEOMETRY_SHADER;
    case kShaderStageComp:
        return GL_COMPUTE_SHADER;
    case kShaderStageNone:
    case kShaderStageMax:
    default:
        // error
        return ~0u;
    }
}

std::string_view util_get_file_ext(std::string_view path)
{
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string_view::npos)
        return {};

    return path.substr(dot_pos + 1);
}

ShaderStage util_shader_ext_to_shader_stage(std::string_view ext)
{
    if (!ext.compare("vert"))
        return kShaderStageVert;
    else if (!ext.compare("frag"))
        return kShaderStageFrag;
    else if (!ext.compare("geom"))
        return kShaderStageGeom;
    else if (!ext.compare("comp"))
        return kShaderStageComp;

    return kShaderStageNone;
}

void util_load_shader_binary(ShaderStage stage, ShaderStageLoadDesc* load_desc, ShaderDesc* shader_desc)
{
    uint32_t gl_stage = util_shader_stage_to_gl_stage(stage);
    ShaderStageDesc* stage_desc = nullptr;
    switch (stage)
    {
    case kShaderStageVert:
        stage_desc = &shader_desc->vert;
        break;
    case kShaderStageFrag:
        stage_desc = &shader_desc->frag;
        break;
    case kShaderStageGeom:
        stage_desc = &shader_desc->geom;
        break;
    case kShaderStageComp:
        stage_desc = &shader_desc->comp;
        break;
    case kShaderStageNone:
    case kShaderStageMax:
    default:
        // error
        break;
    }

    std::filesystem::path path(load_desc->file_name);
    if (!std::filesystem::exists(path))
        return;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file.");
    }

    void* buffer = nullptr;
    uint32_t size = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    buffer = std::malloc(stage_desc->byte_code_size);
    if (buffer == nullptr)
        return;

    file.read(reinterpret_cast<char*>(buffer), size);
    file.close();

    uint32_t shader = glCreateShader(gl_stage);
    glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, buffer, size);
    glSpecializeShader(shader, load_desc->entry_point.data(), 0, NULL, NULL);

    stage_desc->byte_code = buffer; 
    stage_desc->byte_code_size = size;
    stage_desc->entry_point = load_desc->entry_point;
    stage_desc->shader = shader;
}

// ======================================= //
//            Load Functions               //
// ======================================= //

void gl_LoadShader(ShaderLoadDesc* desc, ShaderDesc** out_shader_desc)
{
    ShaderDesc* shader_desc = (ShaderDesc*)std::malloc(sizeof(ShaderDesc));
    if (shader_desc == nullptr)
        return;
    
    shader_desc->stages = kShaderStageNone;
    for (size_t i = 0; i < kShaderStageMax; ++i)
    {
        if (!desc->stages[i].file_name.empty())
        {
            std::string_view ext = util_get_file_ext(desc->stages[i].file_name);
            ShaderStage stage = util_shader_ext_to_shader_stage(ext);
            util_load_shader_binary(stage, &desc->stages[i], shader_desc); 
            shader_desc->stages |= stage;
        }
    }

}

// ======================================= //
//            Render Functions             //
// ======================================= //

void gl_addSwapChain(bool vsync, SwapChain** swapchain)
{
    SwapChain* new_swapchain = (SwapChain*)std::malloc(sizeof(SwapChain)); 
    if (new_swapchain == nullptr) 
        // Log error maybe add assert or something
        return;

    new_swapchain->vsync = vsync;
    new_swapchain->window = get_window();
    new_swapchain->swap_buffers = get_swap_buffers_func();

    *swapchain = new_swapchain;
}

void gl_addBuffer(BufferDesc* desc, Buffer** buffer)
{
    Buffer* new_buffer = (Buffer*)std::malloc(sizeof(Buffer));
    if (new_buffer == nullptr)
        // Log error
        return;
    
    new_buffer->target = desc->target;

    glGenBuffers(1, &new_buffer->id);
    glBindBuffer(new_buffer->target, new_buffer->id);
    glBufferData(new_buffer->target, desc->size, nullptr, desc->memory_usage);
    glBindBuffer(new_buffer->target, GL_NONE);

    *buffer = new_buffer;
}

void gl_addShader(ShaderDesc* desc, Shader** out_shader)
{
    Shader* shader = (Shader*)std::malloc(sizeof(Shader));
    if (shader == nullptr)
        return;
    
    shader->program = glCreateProgram();
    shader->stages = desc->stages;

    for (size_t i = 0; i < kShaderStageMax; ++i)
    {
        ShaderStage stage_mask = (ShaderStage)(1 << i);
        ShaderStageDesc* stage = nullptr;

        if (stage_mask != (shader->stages & stage_mask))
            continue;

        switch (stage_mask)
        {
        case kShaderStageVert:
            stage = &desc->vert;
            break;
        case kShaderStageFrag:
            stage = &desc->frag;
            break;
        case kShaderStageGeom:
            stage = &desc->geom;
            break;
        case kShaderStageComp:
            stage = &desc->comp;
            break;
        case kShaderStageNone:
        case kShaderStageMax:
        default:
            // error
            break;
        }

        // Shader created on Compile shader stage
        glAttachShader(shader->program, stage->shader);
        glDeleteShader(stage->shader);
    }

    glLinkProgram(shader->program);
    // Create reflection here or maybe it have to be created somewhere earlier
}

void gl_removeBuffer(Buffer* buffer)
{
    if (buffer)
    {
        glDeleteBuffers(1, &buffer->id);
        std::free(buffer);
    }
}

void gl_mapBuffer(Buffer* buffer)
{
    glBindBuffer(buffer->target, buffer->id);
    glMapBuffer(buffer->id, GL_WRITE_ONLY);
}

void gl_unmapBuffer(Buffer* buffer)
{
    glUnmapBuffer(buffer->target);
}

// Maybe need to add some params to this function in future
bool gl_init_render()
{
    load_shader   = gl_LoadShader;
    // probably need to add functions here
    add_swapchain = gl_addSwapChain;
    add_buffer    = gl_addBuffer;
    add_shader    = gl_addShader;
    map_buffer    = gl_mapBuffer;
    unmap_buffer  = gl_unmapBuffer;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

    return true;
}





