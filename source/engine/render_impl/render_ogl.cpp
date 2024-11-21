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
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

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

static GLenum util_shader_stage_to_gl_stage(ShaderStage stage)
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

static std::wstring util_stage_to_target_profile(ShaderStage stage)
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

static std::string_view util_get_file_ext(std::string_view path)
{
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string_view::npos)
        return {};

    return path.substr(dot_pos + 1);
}

// Helper function to convert std::wstring to LPCWSTR
static LPCWSTR util_to_LPCWSTR(const std::wstring& s) 
{
    return s.c_str();
}

static std::wstring util_convert_to_wstring(std::string_view str_view)
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

static bool util_compile_hlsl_to_spirv(ShaderStageLoadDesc* load_desc, std::vector<uint8_t>& spirv) {
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
        L"-spirv",            // Output format to SPIR-V
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

static void util_load_shader_binary(ShaderStage stage, ShaderStageLoadDesc* load_desc, ShaderDesc* shader_desc)
{
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
        throw std::runtime_error("Wrong byte-code size");
    }

    new(&stage_desc->byte_code) std::vector<uint8_t>(std::move(buffer));
    stage_desc->entry_point = load_desc->entry_point;
}

static GLbitfield util_buffer_flags_to_gl_storage_flag(BufferFlag flags)
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

static GLenum util_buffer_flags_to_map_access(BufferFlag flags)
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

static ResourceType util_convert_spv_resource_type(SpvReflectResourceType type)
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

static void util_create_shader_reflection(std::vector<uint8_t>& spirv, std::vector<ShaderResource>& resources)
{
    // In case I use spirv_cross to convert sprir-v to glsl
    // maybe there is no needs in spirv_reflect and it is possible to get reflection from spirv cross

    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &module);

    uint32_t desc_size;
    result = spvReflectEnumerateDescriptorBindings(&module, &desc_size, nullptr);

    std::vector<SpvReflectDescriptorBinding*> descriptors(desc_size);
    result = spvReflectEnumerateDescriptorBindings(&module, &desc_size, descriptors.data());

    for (auto& descriptor : descriptors)
    {
        ShaderResource resource;
        resource.name = descriptor->name;
        resource.binding = descriptor->binding;
        resource.set = descriptor->set;
        resource.type = util_convert_spv_resource_type(descriptor->resource_type);
        resources.push_back(resource);
    }

    spvReflectDestroyShaderModule(&module);
}

static GLenum util_get_gl_internal_format(TextureFormat format)
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

static GLenum util_get_gl_format(TextureFormat format)
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

static GLenum util_get_gl_tetxure_data_type(TextureFormat format)
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

static GLenum util_get_gl_texture_target(TextureType type)
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

static GLint util_get_gl_min_filter(FilterType min, FilterType mipmap)
{
    if (mipmap == kFilterTypeNone)
    {
        return (min == kFilterTypeNearest) ? GL_NEAREST : GL_LINEAR;
    }

    bool isNearestMipmap = (mipmap == kFilterTypeNearest);
    if (min == kFilterTypeNearest)
    {
        return isNearestMipmap ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
    }
    else
    {
        return isNearestMipmap ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
    }
}

static GLint util_get_gl_wrap_mode(WrapMode mode)
{
    switch (mode)
    {
    case kWrapModeMirrored: return GL_MIRRORED_REPEAT;
    case kWrapModeRepeat: return GL_REPEAT;
    case kWrapModeClampToEdge: return GL_CLAMP_TO_EDGE;
    case KWrapModeClampToBorder: return GL_CLAMP_TO_BORDER;
    default: return GL_REPEAT;
    }
}

static GLenum util_get_gl_attrib_format(VertexAttribFormat format)
{
    switch (format)
    {
    case kAttribFormatFloat:
        return GL_FLOAT;
    default:
        return GL_FLOAT;
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
        // maybe it is better to remove buffer later
        // or have more than one staging buffer and remove it only
        // when new one is needed
        if (staging_buffer != nullptr)
            remove_buffer(staging_buffer);

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
    if (pixel_buffer != nullptr)
        remove_buffer(pixel_buffer);

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

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer->id); 
    glBindTexture(GL_TEXTURE_2D, desc->texture->id);
    glTextureSubImage2D(desc->texture->id, 0, 0, 0, width, height, format, type, nullptr);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); 
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
        }, desc
    ); 
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
        }, desc
    );
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
    case kTextureType1DArray:
    case kTextureType2DArray:
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

void gl_addSampler(SamplerDesc* desc, Sampler** sampler)
{
    Sampler* new_sampler = (Sampler*)std::malloc(sizeof(Sampler));
    if (new_sampler == nullptr)
        return;

    GLuint id;
    glCreateSamplers(1, &id);
    
    GLint min_filter = util_get_gl_min_filter(desc->min_filter, desc->mip_map_filter);
    GLint mag_filter = (desc->mag_filter == kFilterTypeNearest) ? GL_NEAREST : GL_LINEAR;
    glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, min_filter);
    glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER, mag_filter);

    GLint wrap_u = util_get_gl_wrap_mode(desc->wrap_u);
    GLint wrap_v = util_get_gl_wrap_mode(desc->wrap_v);
    glSamplerParameteri(id, GL_TEXTURE_WRAP_S, wrap_u);
    glSamplerParameteri(id, GL_TEXTURE_WRAP_T, wrap_v);

    new_sampler->id = id;
    *sampler = new_sampler;
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

        // First of all create shader reflection to use binding and set
        // from spirv to set up it in glsl code for combined image samplers
        util_create_shader_reflection(stage->byte_code, shader->resources);

        GLenum gl_stage = util_shader_stage_to_gl_stage(stage_mask);
        const auto& buffer = stage->byte_code;

        std::vector<uint32_t> spirv(buffer.size() / 4);
        std::memcpy(spirv.data(), buffer.data(), buffer.size());

        spirv_cross::CompilerGLSL glsl_compiler(spirv);
        spirv_cross::CompilerGLSL::Options options;
        options.version = 450;
        options.es = false;
        options.separate_shader_objects = true;
        glsl_compiler.set_common_options(options);
        glsl_compiler.build_combined_image_samplers();

        // here need to set up bindings for generated combined image samplers
        // they should be the same as they were in hlsl code
        const auto& combined_image_samplers = glsl_compiler.get_combined_image_samplers();
        const auto& separate_images = glsl_compiler.get_shader_resources().separate_images;

        for (const auto& image : combined_image_samplers)
        {
            // Find texture that was used to create this combined image sampler
            const auto& separate_image = 
                std::find_if(std::begin(separate_images), std::end(separate_images),
                [&](const spirv_cross::Resource& img)
                {
                    return img.id == image.image_id;
                }
            );
            if (separate_image != std::end(separate_images))
            {
                // Using texture name find resource from shader reflection
                // that store correct binding and set
                std::string_view texture_name(separate_image->name);
                const auto& resources = shader->resources;
                const auto& resource = std::find_if(resources.begin(), resources.end(),
                    [&](const ShaderResource& res)
                    {
                        return res.name == texture_name;
                    }
                );
                if (resource != resources.end())
                {
                    const uint32_t binding = resource->binding;
                    const uint32_t set = resource->set;
                    const spv::Decoration dec_binding = spv::Decoration::DecorationBinding;
                    const spv::Decoration dec_set = spv::Decoration::DecorationDescriptorSet;
                    glsl_compiler.set_name(image.combined_id, texture_name.data());
                    glsl_compiler.set_decoration(image.combined_id, dec_binding, binding);
                    glsl_compiler.set_decoration(image.combined_id, dec_set, set);
                }
            }

        }

        std::string glsl_code;
        try {
            glsl_code = glsl_compiler.compile();
            std::cout << "Generated GLSL code: \n" << glsl_code << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        GLuint gl_shader = glCreateShader(gl_stage);
        const char* src = glsl_code.c_str();
        glShaderSource(gl_shader, 1, &src, nullptr);
        glCompileShader(gl_shader);

        GLint status;
        glGetShaderiv(gl_shader, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLint logLength;
            glGetShaderiv(gl_shader, GL_INFO_LOG_LENGTH, &logLength);
            std::string log(logLength, ' ');
            glGetShaderInfoLog(gl_shader, logLength, &logLength, &log[0]);
            std::cerr << "Shader compilation error: " << log << std::endl;
            glDeleteShader(gl_shader);
        }

        glAttachShader(shader->program, gl_shader);
        glDeleteShader(gl_shader);
    }

    glLinkProgram(shader->program);
#if defined(_DEBUG)
    // TODO: move it to some kind of macros or func
    GLint linkStatus;
    glGetProgramiv(shader->program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE) {
        GLint logLength;
        glGetProgramiv(shader->program, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> errorLog(logLength);
        glGetProgramInfoLog(shader->program, logLength, &logLength, &errorLog[0]);

        std::cerr << "Shader linking failed: " << &errorLog[0] << std::endl;
    }
#endif
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

    std::vector<ShaderResource> tmp;
    std::copy_if(desc->shader->resources.begin(), desc->shader->resources.end(),
        std::back_inserter(tmp),
        [=](ShaderResource& resource) {
            return resource.set == new_set->update_freq;
        });
    new (&new_set->descriptors) std::set<ShaderResource>(tmp.begin(), tmp.end());
    new (&new_set->infos) std::vector<std::vector<DescriptorInfo>>(new_set->max_set);

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
        GLenum format  = util_get_gl_attrib_format(desc->vertex_layout->attribs[i].format);
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
    if (new_queue == nullptr)
        return;

    new (&new_queue->queue) std::vector<CmdBuffer*>();
    new_queue->queue.reserve(8); // just random 
    *queue = new_queue;
}

void gl_addCmd(CmdBufferDesc* desc, CmdBuffer** cmd)
{
    using Command = std::function<void()>;

    CmdBuffer* new_cmd = (CmdBuffer*)std::malloc(sizeof(CmdBuffer));
    if (new_cmd == nullptr)
        return;

    new (&new_cmd->commands) std::vector<Command>();
    desc->current_queue->queue.push_back(new_cmd);

    if (desc->use_push_constant)
    {
        PushConstant* new_pc = 
            (PushConstant*)std::malloc(sizeof(PushConstant));
        if (new_pc == nullptr)
            return;
            
        BufferDesc pc_desc{};
        pc_desc.size = desc->pc_desc->size;
        pc_desc.flags = kBufferFlagDynamic;
        add_buffer(&pc_desc, &new_pc->buffer);

        const auto& descriptors = desc->pc_desc->shader->resources;
        std::string_view name(desc->pc_desc->name);
        auto pc_reflection = std::find_if(
            descriptors.begin(), descriptors.end(),
            [&](const ShaderResource& res) {return res.name == name; }
        );

        if (pc_reflection != descriptors.end())
        {
            new_pc->binding = pc_reflection->binding;
        }

        new_pc->size = desc->pc_desc->size;
        new_pc->shader_program = desc->pc_desc->shader->program;

        new_cmd->push_constant = new_pc;
    }

    *cmd = new_cmd;
}

void gl_removeBuffer(Buffer* buffer)
{
    if (buffer)
    {
        glDeleteBuffers(1, &buffer->id);
        std::free(buffer);
        buffer = nullptr;
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
    // In case in opengl we don't need to actually
    // update descriptor set with data
    // we just going to save descriptor ids in vectors
    if (desc->index > set->max_set)
        return; // alert

    uint32_t index = desc->index;
    set->infos[index] = std::move(desc->infos);
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
        for (const auto& descriptor : set->descriptors)
        {
            const auto& infos = set->infos[index];
            if (descriptor.type & kResourceTypeCBV)
            {
                const auto& info_iter = std::find_if(infos.begin(), infos.end(),
                    [&](const DescriptorInfo& info)
                    {
                        return std::holds_alternative<Buffer*>(info.descriptor);
                    }
                );

                if (info_iter != infos.end())
                {
                    const Buffer* buffer = std::get<Buffer*>(info_iter->descriptor);
                    glBindBufferBase(GL_UNIFORM_BUFFER, descriptor.binding, buffer->id);
                }
            }

            if (descriptor.type & kResourceTypeSRV)
            {
                using CombTextureSampler = DescriptorInfo::CombinedTextureSample;
                const auto& info_iter = std::find_if(infos.begin(), infos.end(),
                    [&](const DescriptorInfo& info)
                    {
                        return 
                            std::holds_alternative<CombTextureSampler>(info.descriptor) 
                            && info.name == descriptor.name;
                    }
                );

                if (info_iter != infos.end())
                {
                    const auto& comb = std::get<CombTextureSampler>(info_iter->descriptor);
                    const auto& sampler_iter = std::find_if(infos.begin(), infos.end(),
                        [&](const DescriptorInfo& info)
                        {
                            return
                                std::holds_alternative<Sampler*>(info.descriptor)
                                && info.name == comb.sampler_name;
                        }
                    );
                    if (sampler_iter != infos.end())
                    {
                        const Sampler* sampler = std::get<Sampler*>(sampler_iter->descriptor);
                        glBindTextureUnit(descriptor.binding, comb.texture->id);
                        // In OpenGL in case of it use CombinedTextureSampler
                        // binding point for sampler should be equal to texture binding point
                        glBindSampler(descriptor.binding, sampler->id);
                    }
                    else
                    {
                        // alert we have no sampler for texture
                    }
                }
                else
                {
                    // alert we have no texture with this name
                }
            }
        }
    });
}

void gl_cmdBindVertexBuffer(CmdBuffer* cmd, Buffer* buffer, uint32_t count, uint32_t offset, uint32_t stride)
{
    GLuint& vao = current_vao;
    cmd->commands.push_back([=]() {
        glVertexArrayVertexBuffer(vao, 0, buffer->id, offset, stride); // maybe need to store binding?
        for (int i = 0; i < count; ++i)
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

void gl_cmdBindPushConstant(CmdBuffer* cmd, void* data)
{
    uint32_t size = cmd->push_constant->size;
    uint32_t pc_id = cmd->push_constant->buffer->id;
    uint32_t shader = cmd->push_constant->shader_program;
    uint32_t binding = cmd->push_constant->binding;
    std::vector<uint8_t> data_copy((uint8_t*)data, (uint8_t*)data + size);
    
    cmd->commands.push_back([=]() {
        glNamedBufferSubData(pc_id, 0, size, data_copy.data());
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, pc_id);
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

void gl_cmdUpdateBuffer(CmdBuffer* cmd, Buffer* buffer, size_t offset, size_t size, void* data)
{
    // HACK HACK HACK
    std::vector<uint8_t> data_copy((uint8_t*)data, (uint8_t*)data + size);

    cmd->commands.push_back([=]() {
        glNamedBufferSubData(buffer->id, offset, size, data_copy.data());
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
    add_sampler             = gl_addSampler;
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
    cmd_bind_push_constant  = gl_cmdBindPushConstant;
    cmd_draw                = gl_cmdDraw;
    cmd_draw_indexed        = gl_cmdDrawIndexed;
    cmd_update_buffer       = gl_cmdUpdateBuffer;
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





