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
#include <spirv_reflect.h>
#include <spirv_glsl.hpp>
#include <spirv_parser.hpp>

// ======================================= //
//            Utils Variables              //
// ======================================= //
static GLuint current_vao = 0u;

// ======================================= //
//            Load Variables               //
// ======================================= //

// Need to make something to check that current staging buffer
// doesn't use right now and can be used to map and update resource
static Buffer* staging_buffer = nullptr;
static Buffer* pixel_buffer   = nullptr;


// ======================================= //
//            Utils Functions              //
// ======================================= //

void APIENTRY util_debug_message_callback(GLenum source, GLenum type,
    GLuint id, GLenum severity, GLsizei length,
    const GLchar* message, const void* userParam)
{
    std::string sourceStr;
    switch (source) 
    {
    case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Application"; break;
    case GL_DEBUG_SOURCE_OTHER:           sourceStr = "Other"; break;
    default:                              sourceStr = "Unknown"; break;
    }

    std::string typeStr;
    switch (type) 
    {
    case GL_DEBUG_TYPE_ERROR:               typeStr = "Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated Behavior"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "Undefined Behavior"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              typeStr = "Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          typeStr = "Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP:           typeStr = "Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER:               typeStr = "Other"; break;
    default:                                typeStr = "Unknown"; break;
    }

    std::string severityStr;
    switch (severity) 
    {
    case GL_DEBUG_SEVERITY_HIGH:         severityStr = "High"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       severityStr = "Medium"; break;
    case GL_DEBUG_SEVERITY_LOW:          severityStr = "Low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: severityStr = "Notification"; break;
    default:                             severityStr = "Unknown"; break;
    }

    std::stringstream log;
    log << "OpenGL Debug Message (" << id << "):\n"
        << "Source: " << sourceStr << "\n"
        << "Type: " << typeStr << "\n"
        << "Severity: " << severityStr << "\n"
        << "Message: " << message << "\n";

    std::cout << log.str() << std::endl;
}

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

struct SpirvHeader {
    uint32_t magicNumber;
    uint32_t version;
    uint32_t generatorMagicNumber;
    uint32_t bound;
    uint32_t instructionOffset;
};

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
        L"-spirv", L"-fspv-target-env=vulkan1.2"              // Output format to SPIR-V
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

    if (buffer.size() % 4 != 0) {
        throw std::runtime_error("Размер байт-кода некорректен: он должен быть кратен 4.");
    }

    std::vector<uint32_t> spirv(buffer.size() / 4);
    std::memcpy(spirv.data(), buffer.data(), buffer.size());

    spirv_cross::CompilerGLSL glsl_compiler(spirv);

    // Опциональные настройки для GLSL
    spirv_cross::CompilerGLSL::Options options;
    options.version = 450; // Версия GLSL
    options.es = false;    // Это не GLSL ES
    options.separate_shader_objects = true; // Включить раздельные шейдерные объекты (SOs)
    glsl_compiler.set_common_options(options);

    glsl_compiler.build_combined_image_samplers();
    std::string glsl_code;
    try {
        // Компиляция SPIR-V в GLSL
        glsl_code = glsl_compiler.compile();

        // Вывод GLSL-кода
        std::cout << "GLSL-код: \n" << glsl_code << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        //return -1;
    }

    uint32_t shader = glCreateShader(gl_stage);
    const char* src = glsl_code.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(logLength, ' ');
        glGetShaderInfoLog(shader, logLength, &logLength, &log[0]);
        std::cerr << "Ошибка компиляции шейдера: " << log << std::endl;
        glDeleteShader(shader);
        //return 0;
    }

    //glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, buffer.data(), buffer.size());
    //glSpecializeShader(shader, load_desc->entry_point.data(), 0, NULL, NULL);

    new(&stage_desc->byte_code) std::vector<uint8_t>(std::move(buffer));
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

    // Using GL_NONE as a flag in OpenGL creates GPU only buffer
    if (flags & kBufferFlagGPUOnly)       gl_flags = GL_NONE;

    // Allows to update buffer with glNamedBufferSubData
    if (flags & kBufferFlagDynamic)       gl_flags |= GL_DYNAMIC_STORAGE_BIT;

    // Allow to map for read and/or write
    if (flags & kBufferFlagMapRead)       gl_flags |= GL_MAP_READ_BIT;
    if (flags & kBufferFlagMapWrite)      gl_flags |= GL_MAP_WRITE_BIT;
    if (flags & kBufferFlagMapReadWrite)  gl_flags |= (GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);

    // Allows buffer to be mapped almost infinitely but 
    // all synchronization have to be perfomed by user
    if (flags & kBufferFlagMapPersistent) gl_flags |= GL_MAP_PERSISTENT_BIT;

    // Allows perfom read and write to Persistent buffer
    // without thinking about synchronization
    // But still need to use Fence 
    if (flags & kBufferFlagMapCoherent)   gl_flags |= GL_MAP_COHERENT_BIT;

    // It is CPU only buffer that can be used as a Staging buffer
    // and it is possible to copy from it to GPU only buffer
    // through glCopyBufferSubData 
    if (flags & kBufferFlagClientStorage) gl_flags |= GL_CLIENT_STORAGE_BIT;

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

ResourceType util_convert_spv_resource_type(SpvReflectResourceType type)
{
    switch (type)
    {
    case SPV_REFLECT_RESOURCE_FLAG_UNDEFINED:
        return kResourceTypeUndefined;
    case SPV_REFLECT_RESOURCE_FLAG_SAMPLER:
        return kResourceTypeSampler;
    case SPV_REFLECT_RESOURCE_FLAG_CBV:
        return kResourceTypeCBV;
    case SPV_REFLECT_RESOURCE_FLAG_SRV:
        return kResourceTypeSRV;
    case SPV_REFLECT_RESOURCE_FLAG_UAV:
        return kResourceTypeUAV;
    default:
        return kResourceTypeUndefined;
    }
}

void util_create_shader_reflection(std::vector<uint8_t>& spirv, std::vector<ShaderResource>& resources)
{
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &module);

    uint32_t desc_size;
    result = spvReflectEnumerateDescriptorBindings(&module, &desc_size, nullptr);

    std::vector<SpvReflectDescriptorBinding*> descriptors(desc_size);
    result = spvReflectEnumerateDescriptorBindings(&module, &desc_size, descriptors.data());

    resources.resize(desc_size);
    uint32_t i = 0;
    for (auto& descriptor : descriptors)
    {
        auto& resource = resources[i];
        resource.name = descriptor->name;
        resource.binding = descriptor->binding;
        resource.set = descriptor->set;
        resource.type = util_convert_spv_resource_type(descriptor->resource_type);
        resource.index = i;
        ++i;
    }

    spvReflectDestroyShaderModule(&module);
}

GLenum util_get_gl_internal_format(TextureFormat format)
{
    switch (format)
    {
    case kTextureFormatR8:
        return GL_R8;
    case kTextureFormatRGB8:
        return GL_RGB8;
    case kTextureFormatRGBA8:
        return GL_RGBA8;
    case kTextureFormatRGB16F:
        return GL_RGB16F;
    case kTextureFormatRGBA16F:
        return GL_RGBA16F;
    case kTextureFormatRGBA32F:
        return GL_RGBA32F;
    case kTextureFormatDepth16:
        return GL_DEPTH_COMPONENT16;
    case kTextureFormatDepth24:
        return GL_DEPTH_COMPONENT24;
    case kTextureFormatDepth32F:
        return GL_DEPTH_COMPONENT32F;
    case kTextureFormatDepth24Stencil8:
        return GL_DEPTH24_STENCIL8;
    default:
        return GL_NONE; // Неизвестный формат
    }
}

GLenum util_get_gl_format(TextureFormat format)
{
    switch (format)
    {
    case kTextureFormatR8:
        return GL_RED;
    case kTextureFormatRGB8:
    case kTextureFormatRGB16F:
        return GL_RGB;
    case kTextureFormatRGBA8:
    case kTextureFormatRGBA16F:
    case kTextureFormatRGBA32F:
        return GL_RGBA;
    case kTextureFormatDepth16:
    case kTextureFormatDepth24:
    case kTextureFormatDepth32F:
        return GL_DEPTH_COMPONENT;
    case kTextureFormatDepth24Stencil8:
        return GL_DEPTH_STENCIL;
    default:
        return GL_NONE;
    }
}

GLenum GetGLInternalFormat(TextureFormat format)
{
    switch (format)
    {
    case kTextureFormatR8:
        return GL_R8;
    case kTextureFormatRGB8:
        return GL_RGB8;
    case kTextureFormatRGBA8:
        return GL_RGBA8;
    case kTextureFormatRGB16F:
        return GL_RGB16F;
    case kTextureFormatRGBA16F:
        return GL_RGBA16F;
    case kTextureFormatRGBA32F:
        return GL_RGBA32F;
    case kTextureFormatDepth16:
        return GL_DEPTH_COMPONENT16;
    case kTextureFormatDepth24:
        return GL_DEPTH_COMPONENT24;
    case kTextureFormatDepth32F:
        return GL_DEPTH_COMPONENT32F;
    case kTextureFormatDepth24Stencil8:
        return GL_DEPTH24_STENCIL8;
    default:
        return GL_NONE; // Неизвестный формат
    }
}

GLenum util_get_gl_tetxure_data_type(TextureFormat format)
{
    switch (format)
    {
    case kTextureFormatR8:
    case kTextureFormatRGB8:
    case kTextureFormatRGBA8:
        return GL_UNSIGNED_BYTE;
    case kTextureFormatRGB16F:
    case kTextureFormatRGBA16F:
        return GL_HALF_FLOAT;
    case kTextureFormatRGBA32F:
        return GL_FLOAT;
    case kTextureFormatDepth16:
        return GL_UNSIGNED_SHORT;
    case kTextureFormatDepth24:
        return GL_UNSIGNED_INT;
    case kTextureFormatDepth32F:
        return GL_FLOAT;
    case kTextureFormatDepth24Stencil8:
        return GL_UNSIGNED_INT_24_8;
    default:
        return GL_NONE;
    }
}

GLenum util_get_gl_texture_target(TextureType type)
{
    switch (type)
    {
    case kTextureType1D:
        return GL_TEXTURE_1D;
    case kTextureType2D:
        return GL_TEXTURE_2D;
    case kTextureType3D:
        return GL_TEXTURE_3D;
    case kTextureType1DArray:
        return GL_TEXTURE_1D_ARRAY;
    case kTextureType2DArray:
        return GL_TEXTURE_2D_ARRAY;
    case kTextureTypeCubeMap:
        return GL_TEXTURE_CUBE_MAP;
    default:
        return GL_NONE; // Неизвестный тип
    }
}

// ======================================= //
//            Load Functions               //
// ======================================= //

void gl_loadShader(ShaderLoadDesc* desc, ShaderDesc** out_shader_desc)
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

void gl_beginUpdateBuffer(BufferUpdateDesc* desc)
{
    if (desc->buffer->flags & kBufferFlagGPUOnly)
    {
        // This is GPU only buffer that has to be updated 
        // throug the Staging CPU only buffer and
        // glCopyBufferSubData function call 
        BufferDesc staging_buffer_desc;
        staging_buffer_desc.flags = kBufferFlagClientStorage | kBufferFlagMapWrite;
        staging_buffer_desc.usage = kBufferUsageTransferSrc;
        staging_buffer_desc.size = desc->size;
        add_buffer(&staging_buffer_desc, &staging_buffer);
        desc->mapped_data = map_buffer(staging_buffer);
    }
    else
    {
        // Just regular CPU/GPU buffer that can be mapped
        desc->mapped_data = map_buffer(desc->buffer);
    }
}

void gl_endUpdateBuffer(BufferUpdateDesc* desc)
{
    if (desc->buffer->flags & kBufferFlagGPUOnly)
    {
        unmap_buffer(staging_buffer);
        uint32_t read_buffer = staging_buffer->id;
        uint32_t write_buffer = desc->buffer->id;

        // Probably need to add some kind of cmdCopyBuffer function
        // but I don't see for now how to use command buffer here
        glCopyNamedBufferSubData(read_buffer, write_buffer, 0, 0, desc->size);

        // if we remove buffer here it is probably better to wait
        // copy command to complete
        GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);

        // maybe it is better to remove buffer later
        // or have more than one staging buffer and remove it only
        // when new one is needed
        remove_buffer(staging_buffer);
    }
    else
    {
        unmap_buffer(desc->buffer);
    }
    desc->size = 0;
    desc->mapped_data = nullptr;
}

void gl_beginUpdateTexture(TextureUpdateDesc* desc)
{
    BufferDesc pbo_desc;
    pbo_desc.flags = kBufferFlagMapWrite | kBufferFlagDynamic;
    pbo_desc.usage = kBufferUsageTransferSrc;
    pbo_desc.size = desc->size;
    add_buffer(&pbo_desc, &pixel_buffer);
    desc->mapped_data = map_buffer(pixel_buffer);
}

void gl_endUpdateTexture(TextureUpdateDesc* desc)
{
    unmap_buffer(pixel_buffer);

    GLenum target = desc->texture->target;
    GLsizei width = desc->texture->width;
    GLsizei height = desc->texture->height;
    GLenum format = desc->texture->gl_format;
    GLenum type = desc->texture->gl_type;

    /*glTextureSubImage2D(desc->texture->id, 0, 0, 0, width, height, format, type, desc->data);*/
    
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer->id); 
    glBindTexture(GL_TEXTURE_2D, desc->texture->id);
    glTextureSubImage2D(desc->texture->id, 0, 0, 0, width, height, format, type, nullptr);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    //remove_buffer(pixel_buffer);
}

void gl_beginUpdateResource(ResourceUpdateDesc& desc)
{
    std::visit([](auto* resource) {
        using T = std::decay_t<decltype(*resource)>;

        if constexpr (std::is_same_v<T, TextureUpdateDesc>) 
        {
            gl_beginUpdateTexture(resource);
        }
        else if constexpr (std::is_same_v<T, BufferUpdateDesc>) 
        {
            gl_beginUpdateBuffer(resource);
        }
        else 
        {
            return;
        }
        }, desc); 
}

void gl_endUpdateResource(ResourceUpdateDesc& desc)
{
    std::visit([](auto* resource) {
        using T = std::decay_t<decltype(*resource)>;

        if constexpr (std::is_same_v<T, TextureUpdateDesc>)
        {
            gl_endUpdateTexture(resource);
        }
        else if constexpr (std::is_same_v<T, BufferUpdateDesc>)
        {
            gl_endUpdateBuffer(resource);
        }
        else
        {
            return;
        }
        }, desc);
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

void gl_addTexture(TextureDesc* desc, Texture** texture)
{
    Texture* new_texture = (Texture*)std::malloc(sizeof(Texture));
    if (!new_texture)
        return;

    GLuint gl_target = util_get_gl_texture_target(desc->type);
    GLenum gl_internal_format = util_get_gl_internal_format(desc->format);
    GLsizei width = desc->width;
    GLsizei height = desc->height;
    GLsizei depth = desc->depth;
    GLsizei array_size = desc->array_size;
    GLsizei mip_levels = desc->mip_levels;
    GLuint& id = new_texture->id;

    glCreateTextures(gl_target, 1, &id);

    switch (desc->type)
    {
    case kTextureType1D:
        glTextureStorage1D(id, mip_levels, gl_internal_format, 
            width);
        break;
    case kTextureType2D:
    case kTextureTypeCubeMap:
        glTextureStorage2D(id, mip_levels, gl_internal_format, 
            width, height);
        break;
    case kTextureType3D:
        glTextureStorage3D(id, mip_levels, gl_internal_format,
            width, height, depth);
    case kTextureType2DArray:
    case kTextureType1DArray:
        glTextureStorage3D(id, mip_levels, gl_internal_format, 
            width, height, array_size);
        break;
    default:
        break;
    }

    new_texture->target = gl_target;
    new_texture->internal_format = gl_internal_format;
    new_texture->gl_format = util_get_gl_format(desc->format);
    new_texture->gl_type = util_get_gl_tetxure_data_type(desc->format);
    new_texture->type = desc->type;
    new_texture->format = desc->format;
    new_texture->width = width;
    new_texture->height = height;
    new_texture->depth = depth;
    new_texture->array_size = array_size;
    new_texture->mip_levels = mip_levels;

    *texture = new_texture;
}

void gl_addShader(ShaderDesc* desc, Shader** out_shader)
{
    Shader* shader = (Shader*)std::malloc(sizeof(Shader));
    if (shader == nullptr)
        return;
    
    shader->program = glCreateProgram();
    shader->stages = desc->stages;
    new (&shader->resources) std::vector<ShaderResource>();

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

        util_create_shader_reflection(stage->byte_code, shader->resources);
    }

    glLinkProgram(shader->program);
    
    *out_shader = shader;
}

void gl_addDescriptorSet(DescriptorSetDesc* desc, DescriptorSet** set)
{
    DescriptorSet* new_set = (DescriptorSet*)std::malloc(sizeof(DescriptorSet));
    if (new_set == nullptr)
        return;

    new_set->update_freq = desc->update_freq;
    new_set->max_set = desc->max_sets;
    new_set->program = desc->shader->program;

    new (&new_set->descriptors) std::vector<ShaderResource>();
    std::copy_if(desc->shader->resources.begin(), desc->shader->resources.end(),
        std::back_inserter(new_set->descriptors),
        [=](ShaderResource& resource) {
            return resource.set == new_set->update_freq;
        });

    new (&new_set->buffers) std::vector<std::vector<uint32_t>>();

    *set = new_set;
}

void gl_addPipeline(PipelineDesc* desc, Pipeline** pipeline)
{
    Pipeline* new_pipeline = (Pipeline*)std::malloc(sizeof(Pipeline));
    if (new_pipeline == nullptr)
        return;

    new_pipeline->shader = desc->shader;
    GLuint& vao = new_pipeline->vao;

    glCreateVertexArrays(1, &vao);
    for (int i = 0; i < desc->vertex_layout->attrib_count; ++i)
    {
        GLint size     = desc->vertex_layout->attribs[i].size;
        GLenum format  = desc->vertex_layout->attribs[i].format;
        GLuint offset  = desc->vertex_layout->attribs[i].offset;
        GLuint binding = desc->vertex_layout->attribs[i].binding;
        glVertexArrayAttribFormat(vao, i, size, format, GL_FALSE, offset);
        glVertexArrayAttribBinding(vao, i, binding);
    }
    
    *pipeline = new_pipeline;
}

void gl_addQueue([[maybe_unused]] CmdQueueDesc* desc, CmdQueue** queue)
{
    CmdQueue* new_queue = (CmdQueue*)std::malloc(sizeof(CmdQueue));
    new (&new_queue->queue) std::vector<CmdBuffer*>();
    new_queue->queue.reserve(8); // just random 
    *queue = new_queue;
}

void gl_addCmd(CmdBufferDesc* desc, CmdBuffer** cmd)
{
    using Command = std::function<void()>;

    CmdBuffer* new_cmd = (CmdBuffer*)std::malloc(sizeof(CmdBuffer));
    new (&new_cmd->commands) std::vector<Command>();
    desc->current_queue->queue.push_back(new_cmd);
    *cmd = new_cmd;
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

void gl_updateDescriptorSet(UpdateDescriptorSetDesc* desc, DescriptorSet* set)
{
    uint32_t index = 0;
    for (const auto& buffers : desc->buffers)
    {
        if (buffers.size() > set->max_set)
            return;

        set->buffers.push_back(std::vector<uint32_t>());
        for (const auto buffer : buffers)
        {
            set->buffers[index].push_back(buffer->id);
        }
            
        ++index;
    }
   
    for (const auto& descriptor : set->descriptors)
    {
        const uint32_t index = descriptor.index;
        const uint32_t binding = descriptor.binding;
        if (descriptor.type & kResourceTypeCBV)
        {
            glUniformBlockBinding(set->program, index, binding);
        }

        // add for SRV, UAV and Samplers
    }
}

void gl_cmdBindPipeline(CmdBuffer* cmd, Pipeline* pipeline)
{
    current_vao = pipeline->vao;
    cmd->commands.push_back([=]() {
        glUseProgram(pipeline->shader->program); 
    });
}

void gl_cmdBindDescriptorSet(CmdBuffer* cmd, DescriptorSet* set, uint32_t index)
{
    if (index > set->max_set)
        return;

    cmd->commands.push_back([=]() {
        uint32_t buf_index = 0;
        for (const auto& descriptor : set->descriptors)
        {
            if (descriptor.type & kResourceTypeCBV)
            {
                glBindBufferBase(GL_UNIFORM_BUFFER, descriptor.binding, set->buffers[buf_index][index]);
            }
        }
    });
}

void gl_cmdBindVertexBuffer(CmdBuffer* cmd, Buffer* buffer, uint32_t offset, uint32_t stride)
{
    GLuint& vao = current_vao;
    cmd->commands.push_back([=]() {
        glVertexArrayVertexBuffer(vao, 0, buffer->id, offset, stride); // maybe need to store binding?
        for (int i = 0; i < 3; ++i)
            glEnableVertexArrayAttrib(vao, i); // probably need to store somewhere
    });
}

void gl_cmdBindIndexBuffer(CmdBuffer* cmd, Buffer* buffer)
{
    GLuint& vao = current_vao;
    cmd->commands.push_back([=]() {
        glVertexArrayElementBuffer(vao, buffer->id);
    });
}

void gl_cmdDraw(CmdBuffer* cmd, uint32_t first_vertex, uint32_t count)
{
    GLuint& vao = current_vao;
    cmd->commands.push_back([=]() {
        glBindVertexArray(vao);
        // need to add topology somewhere
        glDrawArrays(GL_TRIANGLES, first_vertex, count);
    });
}

void gl_cmdDrawIndexed(CmdBuffer* cmd, uint32_t index_count, 
    [[maybe_unused]] uint32_t first_index, [[maybe_unused]] uint32_t first_vertex)
{
    GLuint& vao = current_vao;
    // probably need to change function signature
    cmd->commands.push_back([=]() {
        glBindVertexArray(vao);
        // need to add topology somewhere

        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0);
    });
}

void gl_queueSubmit(CmdQueue* queue)
{
    for (auto& cmd : queue->queue)
    {
        for (auto& command : cmd->commands)
        {
            command();
        }
        cmd->commands.clear();
    }
}

// Maybe need to add some params to this function in future
bool gl_init_render()
{
    load_shader             = gl_loadShader;
    begin_update_resource   = gl_beginUpdateResource;
    end_update_resource     = gl_endUpdateResource;

    add_swapchain           = gl_addSwapChain;
    add_buffer              = gl_addBuffer;
    add_texture             = gl_addTexture;
    add_shader              = gl_addShader;
    add_descriptor_set      = gl_addDescriptorSet;
    add_pipeline            = gl_addPipeline;
    add_queue               = gl_addQueue;
    add_cmd                 = gl_addCmd;
    remove_buffer           = gl_removeBuffer;
    map_buffer              = gl_mapBuffer;
    unmap_buffer            = gl_unmapBuffer;
    update_descriptor_set   = gl_updateDescriptorSet;
    cmd_bind_pipeline       = gl_cmdBindPipeline;
    cmd_bind_descriptor_set = gl_cmdBindDescriptorSet;
    cmd_bind_vertex_buffer  = gl_cmdBindVertexBuffer;
    cmd_bind_index_buffer   = gl_cmdBindIndexBuffer;
    cmd_draw                = gl_cmdDraw;
    cmd_draw_indexed        = gl_cmdDrawIndexed;
    queue_submit            = gl_queueSubmit;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

#if _DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(util_debug_message_callback, nullptr);
#endif

    return true;
}





