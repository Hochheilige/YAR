#include "../render.h"
#include "../window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <string_view>
#include <filesystem>
#include <iostream>
#include <fstream>

#include <Windows.h>
#include <atlbase.h>
#include <dxcapi.h>
#include <vector>
#include <string>
#include <codecvt>

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

std::wstring util_stage_to_target_profile(ShaderStage stage)
{
    switch (stage)
    {

    case kShaderStageVert:
        return L"vs_6_0";
    case kShaderStageFrag:
        return L"ps_6_0";
    case kShaderStageGeom:
    case kShaderStageComp:
    case kShaderStageTese:
    case kShaderStageNone:
    case kShaderStageMax:
    default:
        return {};
    }
}

std::string_view util_get_file_ext(std::string_view path)
{
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string_view::npos)
        return {};

    return path.substr(dot_pos + 1);
}

// Helper function to convert std::wstring to LPCWSTR
LPCWSTR util_to_LPCWSTR(const std::wstring& s) 
{
    return s.c_str();
}

std::wstring util_convert_to_wstring(std::string_view str_view)
 {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string str(str_view);
    return converter.from_bytes(str);
}

bool util_compile_hlsl_to_spirv(ShaderStageLoadDesc* load_desc, std::vector<uint8_t>& spirv) {
    // Initialize the Dxc compiler and utils
    CComPtr<IDxcCompiler> compiler;
    CComPtr<IDxcLibrary> library;
    CComPtr<IDxcBlobEncoding> sourceBlob;
    CComPtr<IDxcOperationResult> result;

    HRESULT hr;

    // Initialize the DXC components
    if (FAILED(hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))
        return false;

    if (FAILED(hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library))))
        return false;

    std::wstring shader_path = util_convert_to_wstring(load_desc->file_name.data());
    std::wstring entry_point = util_convert_to_wstring(load_desc->entry_point.data());
    std::wstring target_profile = util_stage_to_target_profile(load_desc->stage);
    // Load the HLSL shader source code
    
    if (FAILED(hr = library->CreateBlobFromFile(shader_path.c_str(), nullptr, &sourceBlob)))
        return false;

    // Prepare the arguments for the compilation
    LPCWSTR arguments[] = {
        L"-spirv",                 // Output format to SPIR-V
    };

    // Compile the shader
    compiler->Compile(
        sourceBlob,            // Source blob
        shader_path.c_str(), // Source file name (used for error reporting)
        entry_point.c_str(),    // Entry point function
        target_profile.c_str(), // Target profile
        arguments, _countof(arguments), // Compilation arguments
        nullptr, 0,            // No defines
        nullptr,               // No include handler
        &result                // Compilation result
    );

    // Check the result of the compilation
    result->GetStatus(&hr);
    if (FAILED(hr)) {
        CComPtr<IDxcBlobEncoding> errors;
        result->GetErrorBuffer(&errors);
        std::cerr << "Compilation failed with errors:\n" << (const char*)errors->GetBufferPointer() << std::endl;
        return false;
    }

    // Retrieve the compiled SPIR-V code
    CComPtr<IDxcBlob> spirvBlob;
    result->GetResult(&spirvBlob);

    spirv.resize(spirvBlob->GetBufferSize());
    memcpy(spirv.data(), spirvBlob->GetBufferPointer(), spirvBlob->GetBufferSize());

    return true;
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

    std::vector<uint8_t> buffer;
    util_compile_hlsl_to_spirv(load_desc, buffer);

    uint32_t shader = glCreateShader(gl_stage);
    glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, buffer.data(), buffer.size());
    glSpecializeShader(shader, load_desc->entry_point.data(), 0, NULL, NULL);

    stage_desc->byte_code = buffer.data(); 
    stage_desc->byte_code_size = buffer.size();
    stage_desc->entry_point = load_desc->entry_point;
    stage_desc->shader = shader;
}

GLbitfield util_buffer_flags_to_gl_storage_flag(BufferFlag flags)
{
    bool is_map_flag = flags & kBufferFlagMapRead
        || flags & kBufferFlagMapWrite
        || flags & kBufferFlagMapReadWrite;
    if (flags & kBufferFlagMapPersistent && !is_map_flag)
        // TODO: add assert
        return GL_NONE;

    GLbitfield gl_flags = GL_NONE;
    if (flags & kBufferFlagDynamic)       gl_flags |= GL_DYNAMIC_STORAGE_BIT;
    if (flags & kBufferFlagMapRead)       gl_flags |= GL_MAP_READ_BIT;
    if (flags & kBufferFlagMapWrite)      gl_flags |= GL_MAP_WRITE_BIT;
    if (flags & kBufferFlagMapReadWrite)  gl_flags |= (GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
    if (flags & kBufferFlagMapPersistent) gl_flags |= GL_MAP_PERSISTENT_BIT;
    if (flags & kBufferFlagMapCoherent)   gl_flags |= GL_MAP_COHERENT_BIT;

    return gl_flags;
}

GLenum util_buffer_flags_to_map_access(BufferFlag flags)
{
    if (flags & kBufferFlagMapRead)
        return GL_READ_ONLY;
    if (flags & kBufferFlagMapWrite)
        return GL_WRITE_ONLY;
    if (flags & kBufferFlagMapReadWrite)
        return GL_READ_WRITE;

    // TODO: add assert
    return GL_NONE;
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
            ShaderStage stage = desc->stages[i].stage;
            util_load_shader_binary(stage, &desc->stages[i], shader_desc); 
            shader_desc->stages |= stage;
        }
    }

    *out_shader_desc = shader_desc;
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

    GLbitfield flags = util_buffer_flags_to_gl_storage_flag(desc->flags);
    new_buffer->flags = desc->flags;

    glCreateBuffers(1, &new_buffer->id);
    glNamedBufferStorage(new_buffer->id, desc->size, nullptr, flags);

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

    *out_shader = shader;
}

void gl_removeBuffer(Buffer* buffer)
{
    if (buffer)
    {
        glDeleteBuffers(1, &buffer->id);
        std::free(buffer);
    }
}

void* gl_mapBuffer(Buffer* buffer)
{
    GLenum map_access = util_buffer_flags_to_map_access(buffer->flags);
    return glMapNamedBuffer(buffer->id, map_access);
}

void gl_unmapBuffer(Buffer* buffer)
{
    glUnmapNamedBuffer(buffer->id);
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





