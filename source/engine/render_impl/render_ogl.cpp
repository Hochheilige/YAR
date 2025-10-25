#include "../render.h"
#include "../window.h"
#include "../render_internal.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <string_view>
#include <filesystem>
#include <iostream>
#include <fstream>

#include <Windows.h>
#include <vector>
#include <string>
#include <codecvt>
#include <spirv_reflect.h>
#include <spirv_glsl.hpp>
#include <spirv_parser.hpp>

#define YAR_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// ======================================= //
//            Load Variables               //
// ======================================= //

// TODO: Need to make something to check that current staging buffer
// doesn't use right now and can be used to map and update resource
static yar_buffer* staging_buffer = nullptr;
static yar_buffer* pixel_buffer   = nullptr;

// ======================================= //
//            OpenGL Structs               //
// ======================================= //
enum yar_gl_texture_type
{
    yar_type_texture = 0,
    yar_type_renderbuffer
};

struct yar_gl_swapchain
{
    yar_swapchain swapchain;
    void* window_handle;
    GLuint fbo;
};

struct yar_gl_texture
{
    yar_texture common;
    yar_gl_texture_type type;
    GLuint id;
    GLuint internal_format;
    GLuint gl_format;
    GLuint gl_type;
};

struct yar_gl_shader
{
    yar_shader shader;

    GLuint vs;
    GLuint ps;
    GLuint gs;
    GLuint cs;

    std::vector<yar_shader_resource> resources;
};

struct yar_gl_pipeline
{
    yar_pipeline pipeline;

    yar_gl_shader* shader;
    GLuint program_pipeline;

    // Rasterizer State
    GLenum fill_mode;
    GLenum cull_mode;
    bool cull_enable;
    bool front_counter_clockwise;

    // Depth Stencil State
    bool depth_enable;
    bool depth_write;
    bool stencil_enable;
    GLenum depth_func;
    GLenum stencil_func;
    GLenum fail;
    GLenum zfail;
    GLenum zpass;

    // Blend State
    bool blend_enable;
    GLenum src_factor;
    GLenum dst_factor;
    GLenum src_alpha_factor;
    GLenum dst_alpha_factor;
    GLenum rgb_equation;
    GLenum alpha_equation;

    // Vertex Layout
    GLuint vao;

    // Topology
    GLenum topology;
};

struct yar_gl_cmd_buffer
{
    yar_cmd_buffer cmd;
    GLuint vao;
    GLenum topology;
    GLuint fbo;
    bool scissor_enabled;
};

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
    case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "yar_shader Compiler"; break;
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

static GLenum util_shader_stage_to_gl_stage(yar_shader_stage stage)
{
    switch (stage)
    {
    case yar_shader_stage_vert:
        return GL_VERTEX_SHADER;
    case yar_shader_stage_pixel:
        return GL_FRAGMENT_SHADER;
    case yar_shader_stage_geom:
        return GL_GEOMETRY_SHADER;
    case yar_shader_stage_comp:
        return GL_COMPUTE_SHADER;
    case yar_shader_stage_none:
    case yar_shader_stage_max:
    default:
        // error
        return ~0u;
    }
}

static std::wstring util_stage_to_target_profile(yar_shader_stage stage)
{
    switch (stage)
    {
    case yar_shader_stage_vert:
        return L"vs_6_0";
    case yar_shader_stage_pixel:
        return L"ps_6_0";
    case yar_shader_stage_geom:
    case yar_shader_stage_comp:
    case yar_shader_stage_tese:
    case yar_shader_stage_none:
    case yar_shader_stage_max:
    default:
        return {};
    }
}

struct SpirvHeader {
    uint32_t magicNumber;
    uint32_t version;
    uint32_t generatorMagicNumber;
    uint32_t bound;
    uint32_t instructionOffset;
};

static bool util_load_spirv(yar_shader_stage_load_desc* load_desc, std::vector<uint8_t>& spirv) 
{
    // Probably can escape it and store bytecode just in header files
    std::filesystem::path path = load_desc->file_name + ".spv";

    std::ifstream byte_code(path, std::ios::binary | std::ios::ate);
    if (!byte_code.is_open()) {
        std::cerr << "Failed to open SPIR-V file: " << path << std::endl;
        return false;
    }
    
    std::streamsize size = byte_code.tellg();
    if (size <= 0) {
        std::cerr << "SPIR-V file is empty or invalid: " << path << std::endl;
        return false;
    }

    spirv.resize(static_cast<size_t>(size));
    byte_code.seekg(0, std::ios::beg);
    if (!byte_code.read(reinterpret_cast<char*>(spirv.data()), size)) {
        std::cerr << "Failed to read SPIR-V file: " << path << std::endl;
        return false;
    }

    return true;
}

static void util_load_shader_binary(yar_shader_stage stage, yar_shader_stage_load_desc* load_desc, yar_shader_desc* shader_desc)
{
    yar_shader_stage_desc* stage_desc = nullptr;
    switch (stage)
    {
    case yar_shader_stage_vert:
        stage_desc = &shader_desc->vert;
        break;
    case yar_shader_stage_pixel:
        stage_desc = &shader_desc->pixel;
        break;
    case yar_shader_stage_geom:
        stage_desc = &shader_desc->geom;
        break;
    case yar_shader_stage_comp:
        stage_desc = &shader_desc->comp;
        break;
    case yar_shader_stage_none:
    case yar_shader_stage_max:
    default:
        // error
        break;
    }

    std::vector<uint8_t> buffer;
    util_load_spirv(load_desc, buffer);

    if (buffer.size() % 4 != 0) {
        throw std::runtime_error("Wrong byte-code size");
    }

    new(&stage_desc->byte_code) std::vector<uint8_t>(std::move(buffer));
    stage_desc->entry_point = load_desc->entry_point;
}

static GLbitfield util_buffer_flags_to_gl_storage_flag(yar_buffer_flag flags)
{
    bool is_map_flag = flags & yar_buffer_flag_map_read
        || flags & yar_buffer_flag_map_write
        || flags & yar_buffer_flag_map_read_write;
    if (flags & yar_buffer_flag_map_persistent && !is_map_flag)
        // TODO: add assert
        return GL_NONE;

    GLbitfield gl_flags = GL_NONE; 

    // Using GL_NONE as a flag in OpenGL creates GPU only buffer
    if (flags & yar_buffer_flag_gpu_only)       gl_flags = GL_NONE;

    // Allows to update buffer with glNamedBufferSubData
    if (flags & yar_buffer_flag_dynamic)       gl_flags |= GL_DYNAMIC_STORAGE_BIT;

    // Allow to map for read and/or write
    if (flags & yar_buffer_flag_map_read)       gl_flags |= GL_MAP_READ_BIT;
    if (flags & yar_buffer_flag_map_write)      gl_flags |= GL_MAP_WRITE_BIT;
    if (flags & yar_buffer_flag_map_read_write)  gl_flags |= (GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);

    // Allows buffer to be mapped almost infinitely but 
    // all synchronization have to be perfomed by user
    if (flags & yar_buffer_flag_map_persistent) gl_flags |= GL_MAP_PERSISTENT_BIT;

    // Allows perfom read and write to Persistent buffer
    // without thinking about synchronization
    // But still need to use Fence 
    if (flags & yar_buffer_flag_map_coherent)   gl_flags |= GL_MAP_COHERENT_BIT;

    // It is CPU only buffer that can be used as a Staging buffer
    // and it is possible to copy from it to GPU only buffer
    // through glCopyBufferSubData 
    if (flags & yar_buffer_flag_client_storage) gl_flags |= GL_CLIENT_STORAGE_BIT;

    return gl_flags;
}

static GLenum util_buffer_flags_to_map_access(yar_buffer_flag flags)
{
    if (flags & yar_buffer_flag_map_read)
        return GL_READ_ONLY;
    if (flags & yar_buffer_flag_map_write)
        return GL_WRITE_ONLY;
    if (flags & yar_buffer_flag_map_read_write)
        return GL_READ_WRITE;

    // TODO: add assert
    return GL_NONE;
}

static yar_resource_type util_convert_spv_resource_type(SpvReflectResourceType type)
{
    switch (type)
    {
    case SPV_REFLECT_RESOURCE_FLAG_UNDEFINED:
        return yar_resource_type_undefined;
    case SPV_REFLECT_RESOURCE_FLAG_SAMPLER:
        return yar_resource_type_sampler;
    case SPV_REFLECT_RESOURCE_FLAG_CBV:
        return yar_resource_type_cbv;
    case SPV_REFLECT_RESOURCE_FLAG_SRV:
        return yar_resource_type_srv;
    case SPV_REFLECT_RESOURCE_FLAG_UAV:
        return yar_resource_type_uav;
    default:
        return yar_resource_type_undefined;
    }
}

static void util_create_shader_reflection(std::vector<uint8_t>& spirv, std::vector<yar_shader_resource>& resources)
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
        yar_shader_resource resource;
        resource.name = descriptor->name;
        resource.binding = descriptor->binding;
        resource.set = descriptor->set;
        resource.type = util_convert_spv_resource_type(descriptor->resource_type);
        resources.push_back(resource);
    }

    spvReflectDestroyShaderModule(&module);
}

static GLenum util_get_gl_internal_format(yar_texture_format format)
{
    switch (format)
    {
    case yar_texture_format_r8:
        return GL_R8;
    case yar_texture_format_rgb8:
        return GL_RGB8;
    case yar_texture_format_rgba8:
        return GL_RGBA8;
    case yar_texture_format_srgb8:
        return GL_SRGB8;
    case yar_texture_format_srgba8:
        return GL_SRGB8_ALPHA8;
    case yar_texture_format_rgb16f:
        return GL_RGB16F;
    case yar_texture_format_rgba16f:
        return GL_RGBA16F;
    case yar_texture_format_rgba32f:
        return GL_RGBA32F;
    case yar_texture_format_depth16:
        return GL_DEPTH_COMPONENT16;
    case yar_texture_format_depth24:
        return GL_DEPTH_COMPONENT24;
    case yar_texture_format_depth32f:
        return GL_DEPTH_COMPONENT32F;
    case yar_texture_format_depth24_stencil8:
        return GL_DEPTH24_STENCIL8;
    default:
        return GL_NONE;
    }
}

static GLenum util_get_gl_format(yar_texture_format format)
{
    switch (format)
    {
    case yar_texture_format_r8:
        return GL_RED;
    case yar_texture_format_rgb8:
    case yar_texture_format_srgb8:
    case yar_texture_format_rgb16f:
        return GL_RGB;
    case yar_texture_format_rgba8:
    case yar_texture_format_srgba8:
    case yar_texture_format_rgba16f:
    case yar_texture_format_rgba32f:
        return GL_RGBA;
    case yar_texture_format_depth16:
    case yar_texture_format_depth24:
    case yar_texture_format_depth32f:
        return GL_DEPTH_COMPONENT;
    case yar_texture_format_depth24_stencil8:
        return GL_DEPTH_STENCIL;
    default:
        return GL_NONE;
    }
}

static GLenum util_get_gl_texture_data_type(yar_texture_format format)
{
    switch (format)
    {
    case yar_texture_format_r8:
    case yar_texture_format_rgb8:
    case yar_texture_format_rgba8:
    case yar_texture_format_srgb8:
    case yar_texture_format_srgba8:
        return GL_UNSIGNED_BYTE;
    case yar_texture_format_rgb16f:
    case yar_texture_format_rgba16f:
        return GL_HALF_FLOAT;
    case yar_texture_format_rgba32f:
        return GL_FLOAT;
    case yar_texture_format_depth16:
        return GL_UNSIGNED_SHORT;
    case yar_texture_format_depth24:
        return GL_UNSIGNED_INT;
    case yar_texture_format_depth32f:
        return GL_FLOAT;
    case yar_texture_format_depth24_stencil8:
        return GL_UNSIGNED_INT_24_8;
    default:
        return GL_NONE;
    }
}

static GLenum util_get_gl_texture_target(yar_texture_type type)
{
    switch (type)
    {
    case yar_texture_type_1d:
        return GL_TEXTURE_1D;
    case yar_texture_type_2d:
        return GL_TEXTURE_2D;
    case yar_texture_type_3d:
        return GL_TEXTURE_3D;
    case yar_texture_type_1d_array:
        return GL_TEXTURE_1D_ARRAY;
    case yar_texture_type_2d_array:
        return GL_TEXTURE_2D_ARRAY;
    case yar_texture_type_cube_map:
        return GL_TEXTURE_CUBE_MAP;
    default:
        return GL_NONE; // unknown type
    }
}

static GLint util_get_gl_min_filter(yar_filter_type min, yar_filter_type mipmap)
{
    if (mipmap == yar_filter_type_none)
    {
        return (min == yar_filter_type_nearest) ? GL_NEAREST : GL_LINEAR;
    }

    bool isNearestMipmap = (mipmap == yar_filter_type_nearest);
    if (min == yar_filter_type_nearest)
    {
        return isNearestMipmap ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
    }
    else
    {
        return isNearestMipmap ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
    }
}

static GLint util_get_gl_wrap_mode(yar_wrap_mode mode)
{
    switch (mode)
    {
    case yar_wrap_mode_mirrored: return GL_MIRRORED_REPEAT;
    case yar_wrap_mode_repeat: return GL_REPEAT;
    case yar_wrap_mode_clamp_edge: return GL_CLAMP_TO_EDGE;
    case yar_wrap_mode_clamp_border: return GL_CLAMP_TO_BORDER;
    default: return GL_REPEAT;
    }
}

static GLenum util_get_gl_attrib_format(yar_vertex_attrib_format format)
{
    switch (format)
    {
    case yar_attrib_format_float:
        return GL_FLOAT;
    case yar_attrib_format_half_float:
        return GL_HALF_FLOAT;
    case yar_attrib_format_byte:  
        return GL_BYTE;
    case yar_attrib_format_ubyte:
        return GL_UNSIGNED_BYTE;
    case yar_attrib_format_short:
        return GL_SHORT;
    case yar_attrib_format_ushort:
        return GL_UNSIGNED_SHORT;
    case yar_attrib_format_int:
        return GL_INT;
    case yar_attrib_format_uint:
        return GL_UNSIGNED_INT;
    default:
        return GL_FLOAT;
    }
}

static GLenum util_get_gl_index_type(yar_index_type type)
{
    switch (type)
    {
    case yar_index_type_uint:
        return GL_UNSIGNED_INT;
    case yar_index_type_ushort:
        return GL_UNSIGNED_SHORT;
    default:
        return GL_UNSIGNED_INT;
    }
}

static GLsizei util_get_gl_index_stride(yar_index_type type)
{
    switch (type)
    {
    case yar_index_type_uint:
        return sizeof(uint32_t);
    case yar_index_type_ushort:
        return sizeof(uint16_t);
    default:
        return sizeof(uint32_t);
    }
}

static GLenum util_get_gl_depth_stencil_func(yar_depth_stencil_func func)
{
    switch (func)
    {
    case yar_depth_stencil_func_always:
        return GL_ALWAYS;
    case yar_depth_stencil_func_never:
        return GL_NEVER;
    case yar_depth_stencil_func_less:
        return GL_LESS;
    case yar_depth_stencil_func_equal:
        return GL_EQUAL;
    case yar_depth_stencil_func_less_equal:
        return GL_LEQUAL;
    case yar_depth_stencil_func_greater:
        return GL_GREATER;
    case yar_depth_stencil_func_not_equal:
        return GL_NOTEQUAL;
    case yar_depth_stencil_func_great_equal:
        return GL_GEQUAL;
    }
}

static GLenum util_get_gl_blend_factor(yar_blend_factor factor)
{
    switch (factor)
    {
    case yar_blend_factor_zero:
        return GL_ZERO;
    case yar_blend_factor_one:
        return GL_ONE;
    case yar_blend_factor_src_color:
        return GL_SRC_COLOR;
    case yar_blend_factor_one_minus_src_color:
        return GL_ONE_MINUS_SRC_COLOR;
    case yar_blend_factor_dst_color:
        return GL_DST_COLOR;
    case yar_blend_factor_one_minus_dst_color:
        return GL_ONE_MINUS_DST_COLOR;
    case yar_blend_factor_src_alpha:
        return GL_SRC_ALPHA;
    case yar_blend_factor_one_minus_src_alpha:
        return GL_ONE_MINUS_SRC_ALPHA;
    case yar_blend_factor_dst_alpha:
        return GL_DST_ALPHA;
    case yar_blned_factor_one_minus_dst_alpha:
        return GL_ONE_MINUS_DST_ALPHA;
    }
}

static GLenum util_get_gl_blend_equation(yar_blend_op op)
{
    switch (op)
    {
    case yar_blend_op_add:
        return GL_FUNC_ADD;
    case yar_blend_op_subtract:
        return GL_FUNC_SUBTRACT;
    case yar_blend_op_reverse_subtract:
        return GL_FUNC_REVERSE_SUBTRACT;
    case yar_blend_op_min:
        return GL_MIN;
    case yar_blend_op_max:
        return GL_MAX;
    }
}

static GLenum util_get_gl_fill_mode(yar_fill_mode mode)
{
    switch (mode)
    {
    case yar_fill_mode_solid:
        return GL_FILL;
    case yar_fill_mode_wireframe:
        return GL_LINE;
    default:
        return GL_FILL;
    }
}

static GLenum util_get_gl_cull_mode(yar_cull_mode mode)
{
    switch (mode)
    {
    case yar_cull_mode_front:
        return GL_FRONT;
    case yar_cull_mode_back:
        return GL_BACK;
    case yar_cull_mode_none:
    default:
        return 0;
    }
}

static GLenum util_get_gl_topology(yar_primitive_topology topology)
{
    switch (topology)
    {
    case yar_primitive_topology_triangle_list:
        return GL_TRIANGLES;
    case yar_primitive_topology_triangle_strip:
        return GL_TRIANGLE_STRIP;
    case yar_primitive_topology_line_list:
        return GL_LINES;
    case yar_primitive_topology_line_strip:
        return GL_LINE_STRIP;
    case yar_primitive_topology_point_list:
        return GL_POINTS;
    default:
        return GL_TRIANGLES; // better assert here
    }
}

// ======================================= //
//            Load Functions               //
// ======================================= //

void gl_loadShader(yar_shader_load_desc* desc, yar_shader_desc** out_shader_desc)
{
    yar_shader_desc* shader_desc = (yar_shader_desc*)std::malloc(sizeof(yar_shader_desc));
    if (shader_desc == nullptr)
        return;
    
    shader_desc->stages = yar_shader_stage_none;
    for (size_t i = 0; i < yar_shader_stage_max; ++i)
    {
        if (!desc->stages[i].file_name.empty())
        {
            yar_shader_stage stage = desc->stages[i].stage;
            util_load_shader_binary(stage, &desc->stages[i], shader_desc); 
            shader_desc->stages |= stage;
        }
    }

    *out_shader_desc = shader_desc;
}

void gl_beginUpdateBuffer(yar_buffer_update_desc* desc)
{
    if (desc->buffer->flags & yar_buffer_flag_gpu_only)
    {
        // maybe it is better to remove buffer later
        // or have more than one staging buffer and remove it only
        // when new one is needed
        if (staging_buffer != nullptr)
            remove_buffer(staging_buffer);

        // This is GPU only buffer that has to be updated 
        // throug the Staging CPU only buffer and
        // glCopyBufferSubData function call 
        yar_buffer_desc staging_buffer_desc;
        staging_buffer_desc.flags = yar_buffer_flag_client_storage | yar_buffer_flag_map_write;
        staging_buffer_desc.usage = yar_buffer_usage_transfer_src;
        staging_buffer_desc.size = desc->size;
        staging_buffer_desc.name = "staging_buffer";
        add_buffer(&staging_buffer_desc, &staging_buffer);
        desc->mapped_data = map_buffer(staging_buffer);
    }
    else
    {
        // Just regular CPU/GPU buffer that can be mapped
        desc->mapped_data = map_buffer(desc->buffer);
    }
}

void gl_endUpdateBuffer(yar_buffer_update_desc* desc)
{
    if (desc->buffer->flags & yar_buffer_flag_gpu_only)
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

void gl_beginUpdateTexture(yar_texture_update_desc* desc)
{
    if (pixel_buffer != nullptr)
        remove_buffer(pixel_buffer);

    yar_buffer_desc pbo_desc;
    pbo_desc.flags = yar_buffer_flag_map_write | yar_buffer_flag_dynamic;
    pbo_desc.usage = yar_buffer_usage_transfer_src;
    pbo_desc.size = desc->size;
    pbo_desc.name = "pbo_buffer";
    add_buffer(&pbo_desc, &pixel_buffer);
    desc->mapped_data = map_buffer(pixel_buffer);
}

void gl_endUpdateTexture(yar_texture_update_desc* desc)
{
    unmap_buffer(pixel_buffer);
    
    yar_gl_texture* texture = reinterpret_cast<yar_gl_texture*>(desc->texture);

    GLsizei width = texture->common.width;
    GLsizei height = texture->common.height;
    GLsizei depth = texture->common.depth;
    GLenum format = texture->gl_format;
    GLenum type = texture->gl_type;
    uint32_t mip_count = texture->common.mip_levels;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer->id); 
    switch (desc->texture->type)
    {
    case yar_texture_type_1d:
        glTextureSubImage1D(texture->id, 0, 0, width, format, type, nullptr);
        break;
    case yar_texture_type_2d:
        glTextureSubImage2D(texture->id, 0, 0, 0, width, height, 
            format, type, nullptr);
        break;
    case yar_texture_type_3d:
        glTextureSubImage3D(texture->id, 0, 0, 0, 0, width, height, depth,
            format, type, nullptr);
        break;
    case yar_texture_type_1d_array:
        glTextureSubImage2D(texture->id, 0, 0, 0, width, height,
            format, type, nullptr);
        break;
    case yar_texture_type_2d_array:
        glTextureSubImage3D(texture->id, 0, 0, 0, 0, width, height, 
            depth, format, type, nullptr);
        break;
    case yar_texture_type_cube_map:
        glTextureSubImage3D(texture->id, 0, 0, 0, 0, width, height,
            6, format, type, nullptr);
        break;
    case yar_texture_type_none:
    default:
        break;
    }
    
    if (mip_count > 1)
        glGenerateTextureMipmap(texture->id);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); 
}

void gl_beginUpdateResource(yar_resource_update_desc& desc)
{
    std::visit([](auto* resource) {
        using T = std::decay_t<decltype(*resource)>;

        if constexpr (std::is_same_v<T, yar_texture_update_desc>) 
        {
            gl_beginUpdateTexture(resource);
        }
        else if constexpr (std::is_same_v<T, yar_buffer_update_desc>) 
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

void gl_endUpdateResource(yar_resource_update_desc& desc)
{
    std::visit([](auto* resource) {
        using T = std::decay_t<decltype(*resource)>;

        if constexpr (std::is_same_v<T, yar_texture_update_desc>)
        {
            gl_endUpdateTexture(resource);
        }
        else if constexpr (std::is_same_v<T, yar_buffer_update_desc>)
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

void gl_addSwapChain(yar_swapchain_desc* desc, yar_swapchain** swapchain)
{
    auto new_swapchain = static_cast<yar_gl_swapchain*>(std::calloc(1, sizeof(yar_gl_swapchain))); 
    
    *swapchain = &new_swapchain->swapchain;

    uint32_t buffer_count = desc->buffer_count;

    new_swapchain->swapchain.vsync = desc->vsync;
    new_swapchain->swapchain.buffer_count = buffer_count;
    new_swapchain->window_handle = desc->window_handle;
    
    new_swapchain->swapchain.render_targets = static_cast<yar_render_target**>(
        std::calloc(buffer_count, sizeof(yar_render_target*))
    );

    yar_render_target_desc rt_desc{};
    rt_desc.format = desc->format;
    rt_desc.width = desc->width;
    rt_desc.height = desc->height;
    rt_desc.type = yar_texture_type_2d;
    rt_desc.usage = yar_texture_usage_render_target;
    rt_desc.mip_levels = 1;
    for (uint32_t i = 0; i < buffer_count; ++i)
    {
        yar_render_target* rt;
        add_render_target(&rt_desc, &rt);
        new_swapchain->swapchain.render_targets[i] = rt;
    }

    glCreateFramebuffers(1, &new_swapchain->fbo);
}

void gl_addBuffer(yar_buffer_desc* desc, yar_buffer** buffer)
{
    yar_buffer* new_buffer = (yar_buffer*)std::malloc(sizeof(yar_buffer));
    if (new_buffer == nullptr)
        // Log error
        return;

    GLbitfield flags = util_buffer_flags_to_gl_storage_flag(desc->flags);
    new_buffer->flags = desc->flags;

    glCreateBuffers(1, &new_buffer->id);
    glNamedBufferStorage(new_buffer->id, desc->size, nullptr, flags);

#if _DEBUG
    if (desc->name)
        glObjectLabel(GL_BUFFER, new_buffer->id, -1, desc->name);
#endif

    *buffer = new_buffer;
}

void gl_addTexture(yar_texture_desc* desc, yar_texture** texture)
{
    yar_gl_texture* new_texture = static_cast<yar_gl_texture*>(
        std::calloc(1, sizeof(yar_gl_texture))
    );
    *texture = &new_texture->common;
    
    GLuint gl_target = util_get_gl_texture_target(desc->type);
    GLenum gl_internal_format = util_get_gl_internal_format(desc->format);
    GLsizei width = desc->width;
    GLsizei height = desc->height;
    GLsizei depth = desc->depth;
    GLsizei array_size = desc->array_size;
    GLsizei mip_levels = desc->mip_levels;
    GLuint& id = new_texture->id;

    bool need_shader_access = (desc->usage &
        (yar_texture_usage::yar_texture_usage_shader_resource | yar_texture_usage::yar_texture_usage_unordered_access));
    bool need_rt_only = (desc->usage & 
        (yar_texture_usage::yar_texture_usage_render_target | yar_texture_usage::yar_texture_usage_depth_stencil))
        && !need_shader_access;

    if (need_rt_only)
    {
        new_texture->type = yar_gl_texture_type::yar_type_renderbuffer;
        glCreateRenderbuffers(1, &id);
        glNamedRenderbufferStorage(id, gl_internal_format, width, height);
    }
    else
    {
        new_texture->type = yar_gl_texture_type::yar_type_texture;
        glCreateTextures(gl_target, 1, &id);

        switch (desc->type)
        {
        case yar_texture_type_1d:
            glTextureStorage1D(id, mip_levels, gl_internal_format,
                width);
            break;
        case yar_texture_type_2d:
        case yar_texture_type_cube_map:
            glTextureStorage2D(id, mip_levels, gl_internal_format,
                width, height);
            break;
        case yar_texture_type_3d:
            glTextureStorage3D(id, mip_levels, gl_internal_format,
                width, height, depth);
            break;
        case yar_texture_type_1d_array:
        case yar_texture_type_2d_array:
            glTextureStorage3D(id, mip_levels, gl_internal_format,
                width, height, array_size);
            break;
        default:
            break;
        }
    }
        
    new_texture->internal_format = gl_internal_format;
    new_texture->gl_format = util_get_gl_format(desc->format);
    new_texture->gl_type = util_get_gl_texture_data_type(desc->format);
    new_texture->common.type = desc->type;
    new_texture->common.format = desc->format;
    new_texture->common.width = width;
    new_texture->common.height = height;
    new_texture->common.depth = depth;
    new_texture->common.array_size = array_size;
    new_texture->common.mip_levels = mip_levels;

#if _DEBUG
    if (desc->name)
        glObjectLabel(GL_TEXTURE, new_texture->id, -1, desc->name);
#endif
}

void gl_addRenderTarget(yar_render_target_desc* desc, yar_render_target** rt)
{
    auto new_rt = static_cast<yar_render_target*>(calloc(1, sizeof(yar_render_target)));

    yar_texture_desc tex_desc{};
    tex_desc.type = desc->type;
    tex_desc.format = desc->format;
    tex_desc.usage = desc->usage;
    tex_desc.array_size = desc->array_size;
    tex_desc.depth = desc->depth;
    tex_desc.height = desc->height;
    tex_desc.mip_levels = desc->mip_levels;
    tex_desc.width = desc->width;
    gl_addTexture(&tex_desc, &new_rt->texture);

    new_rt->width = desc->width;
    new_rt->height = desc->height;
    *rt = new_rt;
}

void gl_addSampler(yar_sampler_desc* desc, yar_sampler** sampler)
{
    yar_sampler* new_sampler = (yar_sampler*)std::malloc(sizeof(yar_sampler));
    if (new_sampler == nullptr)
        return;

    GLuint id;
    glCreateSamplers(1, &id);
    
    GLint min_filter = util_get_gl_min_filter(desc->min_filter, desc->mip_map_filter);
    GLint mag_filter = (desc->mag_filter == yar_filter_type_nearest) ? GL_NEAREST : GL_LINEAR;
    glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, min_filter);
    glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER, mag_filter);

    GLint wrap_u = util_get_gl_wrap_mode(desc->wrap_u);
    GLint wrap_v = util_get_gl_wrap_mode(desc->wrap_v);
    GLint wrap_w = util_get_gl_wrap_mode(desc->wrap_w);
    glSamplerParameteri(id, GL_TEXTURE_WRAP_S, wrap_u);
    glSamplerParameteri(id, GL_TEXTURE_WRAP_T, wrap_v);
    glSamplerParameteri(id, GL_TEXTURE_WRAP_R, wrap_w);

    if (desc->wrap_u == yar_wrap_mode_clamp_border || desc->wrap_v == yar_wrap_mode_clamp_border ||
        desc->wrap_w == yar_wrap_mode_clamp_border)
    {
        float border_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glSamplerParameterfv(id, GL_TEXTURE_BORDER_COLOR, border_color);
    }

    new_sampler->id = id;
    *sampler = new_sampler;
}

void gl_addShader(yar_shader_desc* desc, yar_shader** out_shader)
{
    yar_gl_shader* new_shader = static_cast<yar_gl_shader*>(
        std::calloc(1, sizeof(yar_gl_shader))
    );
    *out_shader = &new_shader->shader;

    new_shader->shader.stages = desc->stages;
    new (&new_shader->resources) std::vector<yar_shader_resource>();

    for (size_t i = 0; i < yar_shader_stage_max; ++i)
    {
        yar_shader_stage stage_mask = (yar_shader_stage)(1 << i);
        yar_shader_stage_desc* stage = nullptr;

        if (stage_mask != (new_shader->shader.stages & stage_mask))
            continue;

        GLuint* gl_shader = nullptr;
        switch (stage_mask)
        {
        case yar_shader_stage_vert:
            stage = &desc->vert;
            gl_shader = &new_shader->vs;
            break;
        case yar_shader_stage_pixel:
            stage = &desc->pixel;
            gl_shader = &new_shader->ps;
            break;
        case yar_shader_stage_geom:
            stage = &desc->geom;
            gl_shader = &new_shader->gs;
            break;
        case yar_shader_stage_comp:
            stage = &desc->comp;
            gl_shader = &new_shader->cs;
            break;
        case yar_shader_stage_none:
        case yar_shader_stage_max:
        default:
            // error
            break;
        }

        // First of all create shader reflection to use binding and set
        // from spirv to set up it in glsl code for combined image samplers
        util_create_shader_reflection(stage->byte_code, new_shader->resources);

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
        glsl_compiler.build_dummy_sampler_for_combined_images();
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
                const auto& resources = new_shader->resources;
                const auto& resource = std::find_if(resources.begin(), resources.end(),
                    [&](const yar_shader_resource& res)
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

        const char* src = glsl_code.c_str();
        *gl_shader = glCreateShaderProgramv(gl_stage, 1, &src);
        
        GLint status;
        glGetProgramiv(*gl_shader, GL_LINK_STATUS, &status);
        if (status == GL_FALSE) {
            GLint log_length;
            glGetProgramiv(*gl_shader, GL_INFO_LOG_LENGTH, &log_length);
            std::string log(log_length, ' ');
            glGetShaderInfoLog(*gl_shader, log_length, &log_length, &log[0]);
            std::cerr << "yar_shader compilation error: " << log << std::endl;
            glDeleteProgram(*gl_shader);
        }

        glProgramParameteri(*gl_shader, GL_PROGRAM_SEPARABLE, GL_TRUE);
    }
}

void gl_addDescriptorSet(yar_descriptor_set_desc* desc, yar_descriptor_set** set)
{
    yar_descriptor_set* new_set = (yar_descriptor_set*)std::malloc(sizeof(yar_descriptor_set));
    if (new_set == nullptr)
        return;

    new_set->update_freq = desc->update_freq;
    new_set->max_set = desc->max_sets;
    
    auto gl_shader = reinterpret_cast<yar_gl_shader*>(desc->shader);

    std::vector<yar_shader_resource> tmp;
    std::copy_if(gl_shader->resources.begin(), gl_shader->resources.end(),
        std::back_inserter(tmp),
        [=](yar_shader_resource& resource) {
            return resource.set == new_set->update_freq;
        });
    new (&new_set->descriptors) std::set<yar_shader_resource>(tmp.begin(), tmp.end());
    new (&new_set->infos) std::vector<std::vector<yar_descriptor_info>>(new_set->max_set);

    *set = new_set;
}

void gl_addPipeline(yar_pipeline_desc* desc, yar_pipeline** pipeline)
{
    yar_gl_pipeline* new_pipeline = static_cast<yar_gl_pipeline*>(
        std::calloc(1, sizeof(yar_gl_pipeline))
    );
    *pipeline = &new_pipeline->pipeline;

    new_pipeline->shader = reinterpret_cast<yar_gl_shader*>(desc->shader);
    glGenProgramPipelines(1, &new_pipeline->program_pipeline);
    for (size_t i = 0; i < yar_shader_stage_max; ++i)
    {
        yar_shader_stage stage_mask = (yar_shader_stage)(1 << i);
        if (stage_mask != (new_pipeline->shader->shader.stages & stage_mask))
            continue;
        
        GLbitfield stage;
        GLuint* gl_shader = nullptr;
        switch (stage_mask)
        {
        case yar_shader_stage_vert:
            stage = GL_VERTEX_SHADER_BIT;
            gl_shader = &new_pipeline->shader->vs;
            break;
        case yar_shader_stage_pixel:
            stage = GL_FRAGMENT_SHADER_BIT;
            gl_shader = &new_pipeline->shader->ps;
            break;
        case yar_shader_stage_geom:
            stage = GL_GEOMETRY_SHADER_BIT;
            gl_shader = &new_pipeline->shader->gs;
            break;
        case yar_shader_stage_comp:
            stage = GL_COMPUTE_SHADER_BIT;
            gl_shader = &new_pipeline->shader->cs;
            break;
        case yar_shader_stage_none:
        case yar_shader_stage_max:
        default:
            // error
            break;
        }

        glUseProgramStages(new_pipeline->program_pipeline, stage, *gl_shader);
    }

    new_pipeline->fill_mode = util_get_gl_fill_mode(desc->rasterizer_state.fill_mode);
    new_pipeline->cull_mode = util_get_gl_cull_mode(desc->rasterizer_state.cull_mode);
    new_pipeline->cull_enable = new_pipeline->cull_mode;
    new_pipeline->front_counter_clockwise = desc->rasterizer_state.front_counter_clockwise;
    new_pipeline->depth_write = desc->depth_stencil_state.depth_write;

    new_pipeline->depth_enable = desc->depth_stencil_state.depth_enable;
    if (new_pipeline->depth_enable)
    {
        new_pipeline->depth_func = util_get_gl_depth_stencil_func(desc->depth_stencil_state.depth_func);
    }

    new_pipeline->stencil_enable = desc->depth_stencil_state.stencil_enable;
    if (new_pipeline->stencil_enable)
    {
        new_pipeline->stencil_func = util_get_gl_depth_stencil_func(desc->depth_stencil_state.stencil_func);
        new_pipeline->fail = desc->depth_stencil_state.sfail;
        new_pipeline->zfail = desc->depth_stencil_state.dpfail;
        new_pipeline->zpass = desc->depth_stencil_state.dppass;
    }

    new_pipeline->blend_enable = desc->blend_state.blend_enable;
    if (new_pipeline->blend_enable)
    {
        new_pipeline->src_factor = util_get_gl_blend_factor(desc->blend_state.src_factor);
        new_pipeline->dst_factor = util_get_gl_blend_factor(desc->blend_state.dst_factor);
        new_pipeline->src_alpha_factor = util_get_gl_blend_factor(desc->blend_state.src_alpha_factor);
        new_pipeline->dst_alpha_factor = util_get_gl_blend_factor(desc->blend_state.dst_alpha_factor);
        new_pipeline->rgb_equation = util_get_gl_blend_equation(desc->blend_state.op);
        new_pipeline->alpha_equation = util_get_gl_blend_equation(desc->blend_state.alpha_op);
    }

    GLuint& vao = new_pipeline->vao;
    glCreateVertexArrays(1, &vao);
    for (int i = 0; i < desc->vertex_layout.attrib_count; ++i)
    {
        GLint size     = desc->vertex_layout.attribs[i].size;
        GLenum format  = util_get_gl_attrib_format(desc->vertex_layout.attribs[i].format);
        GLuint offset  = desc->vertex_layout.attribs[i].offset;
        GLuint binding = desc->vertex_layout.attribs[i].binding;
        GLboolean normalize = format == GL_UNSIGNED_BYTE; // Stupid temp hack to normalize imgui colors
        glVertexArrayAttribFormat(vao, i, size, format, normalize, offset);
        glVertexArrayAttribBinding(vao, i, binding);
    } 

    new_pipeline->topology = util_get_gl_topology(desc->topology);
}

void gl_addQueue([[maybe_unused]] yar_cmd_queue_desc* desc, yar_cmd_queue** queue)
{
    yar_cmd_queue* new_queue = (yar_cmd_queue*)std::malloc(sizeof(yar_cmd_queue));
    if (new_queue == nullptr)
        return;

    new (&new_queue->queue) std::vector<yar_cmd_buffer*>();
    new_queue->queue.reserve(8); // just random 
    *queue = new_queue;
}

void gl_addCmd(yar_cmd_buffer_desc* desc, yar_cmd_buffer** cmd)
{
    using Command = std::function<void()>;

    yar_gl_cmd_buffer* new_cmd = static_cast<yar_gl_cmd_buffer*>(
        std::calloc(1, sizeof(yar_gl_cmd_buffer))
    );
    *cmd = &new_cmd->cmd;
    
    new (&new_cmd->cmd.commands) std::vector<Command>();
    desc->current_queue->queue.push_back(&new_cmd->cmd);

    if (desc->use_push_constant)
    {
        yar_push_constant* new_pc = 
            (yar_push_constant*)std::malloc(sizeof(yar_push_constant));
        if (new_pc == nullptr)
            return;
            
        yar_buffer_desc pc_desc{};
        pc_desc.size = desc->pc_desc->size;
        pc_desc.flags = yar_buffer_flag_dynamic;
        add_buffer(&pc_desc, &new_pc->buffer);

        auto gl_shader = reinterpret_cast<yar_gl_shader*>(desc->pc_desc->shader);
        const auto& descriptors = gl_shader->resources;
        std::string_view name(desc->pc_desc->name);
        auto pc_reflection = std::find_if(
            descriptors.begin(), descriptors.end(),
            [&](const yar_shader_resource& res) {return res.name == name; }
        );

        if (pc_reflection != descriptors.end())
        {
            new_pc->binding = pc_reflection->binding;
        }

        new_pc->size = desc->pc_desc->size;
        new_cmd->cmd.push_constant = new_pc;
    }

    glCreateFramebuffers(1, &new_cmd->fbo);
}

void gl_removeBuffer(yar_buffer* buffer)
{
    if (buffer)
    {
        glDeleteBuffers(1, &buffer->id);
        std::free(buffer);
        buffer = nullptr;
    }
}

void* gl_mapBuffer(yar_buffer* buffer)
{
    GLenum map_access = util_buffer_flags_to_map_access(buffer->flags);
    return glMapNamedBuffer(buffer->id, map_access);
}

void gl_unmapBuffer(yar_buffer* buffer)
{
    glUnmapNamedBuffer(buffer->id);
}

void gl_updateDescriptorSet(yar_update_descriptor_set_desc* desc, yar_descriptor_set* set)
{
    // In case in opengl we don't need to actually
    // update descriptor set with data
    // we just going to save descriptor ids in vectors
    if (desc->index > set->max_set)
        return; // alert

    uint32_t index = desc->index;
    set->infos[index] = std::move(desc->infos);
}

void gl_acquireNextImage(yar_swapchain* swapchain, uint32_t& swapchain_index)
{
    auto gl_swapchain = reinterpret_cast<yar_gl_swapchain*>(swapchain);
    swapchain_index = swapchain->buffer_index % swapchain->buffer_count;
    auto rt = swapchain->render_targets[swapchain_index];
    auto gl_texture = reinterpret_cast<yar_gl_texture*>(rt->texture);
    glNamedFramebufferRenderbuffer(gl_swapchain->fbo,
        GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gl_texture->id);
}

void gl_cmdBindPipeline(yar_cmd_buffer* cmd, yar_pipeline* pipeline)
{
    cmd->commands.push_back([=]() {
        yar_gl_pipeline* gl_pipeline = reinterpret_cast<yar_gl_pipeline*>(pipeline);

        glBindProgramPipeline(gl_pipeline->program_pipeline);
        
        if (gl_pipeline->depth_enable)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(gl_pipeline->depth_func);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }

        if (gl_pipeline->depth_write)
        {
            glDepthMask(GL_TRUE);
        }
        else
        {
            glDepthMask(GL_FALSE);
        }

        if (gl_pipeline->stencil_enable)
        {
            glEnable(GL_STENCIL_TEST);
            // Ref and Mask usually 1 and 0xFF 
            glStencilFunc(gl_pipeline->stencil_func, 1, 0xFF);
            glStencilOp(gl_pipeline->fail, gl_pipeline->zfail, gl_pipeline->zpass);
        }
        else
        {
            glDisable(GL_STENCIL_TEST);
        }

        if (gl_pipeline->blend_enable)
        {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(gl_pipeline->src_factor, gl_pipeline->dst_factor,
                gl_pipeline->src_alpha_factor, gl_pipeline->dst_alpha_factor);
            glBlendEquationSeparate(gl_pipeline->rgb_equation, gl_pipeline->alpha_equation);
        }
        else
        {
            glDisable(GL_BLEND);
        }

        glPolygonMode(GL_FRONT_AND_BACK, gl_pipeline->fill_mode);
        if (gl_pipeline->cull_enable)
        {
            glEnable(GL_CULL_FACE);
            glCullFace(gl_pipeline->cull_mode);
        }
        else
        {
            glDisable(GL_CULL_FACE);
        }

        gl_pipeline->front_counter_clockwise ? glFrontFace(GL_CCW) : glFrontFace(GL_CW);

        yar_gl_cmd_buffer* gl_cmd = reinterpret_cast<yar_gl_cmd_buffer*>(cmd);
        gl_cmd->vao = gl_pipeline->vao;
        gl_cmd->topology = gl_pipeline->topology;
    });
}

void gl_cmdBindDescriptorSet(yar_cmd_buffer* cmd, yar_descriptor_set* set, uint32_t index)
{
    if (index > set->max_set)
        return;

    cmd->commands.push_back([=]() {
        for (const auto& descriptor : set->descriptors)
        {
            const auto& infos = set->infos[index];
            if (descriptor.type & yar_resource_type_cbv)
            {
                const auto& info_iter = std::find_if(infos.begin(), infos.end(),
                    [&](const yar_descriptor_info& info)
                    {
                        return std::holds_alternative<yar_buffer*>(info.descriptor);
                    }
                );

                if (info_iter != infos.end())
                {
                    const yar_buffer* buffer = std::get<yar_buffer*>(info_iter->descriptor);
                    glBindBufferBase(GL_UNIFORM_BUFFER, descriptor.binding, buffer->id);
                }
            }

            if (descriptor.type & yar_resource_type_srv)
            {
                using CombTextureSampler = yar_descriptor_info::yar_combined_texture_sample;
                const auto& info_iter = std::find_if(infos.begin(), infos.end(),
                    [&](const yar_descriptor_info& info)
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
                        [&](const yar_descriptor_info& info)
                        {
                            return
                                std::holds_alternative<yar_sampler*>(info.descriptor)
                                && info.name == comb.sampler_name;
                        }
                    );
                    if (sampler_iter != infos.end())
                    {
                        const yar_sampler* sampler = std::get<yar_sampler*>(sampler_iter->descriptor);
                        const yar_gl_texture* texture = reinterpret_cast<yar_gl_texture*>(comb.texture);
                        glBindTextureUnit(descriptor.binding, texture->id);
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

            // TODO: properly set up UAV
            if (descriptor.type & yar_resource_type_uav)
            {
                const auto& info_iter = std::find_if(infos.begin(), infos.end(),
                    [&](const yar_descriptor_info& info)
                    {
                        return std::holds_alternative<yar_texture*>(info.descriptor);
                    }
                );

                if (info_iter != infos.end())
                {
                    const yar_gl_texture* uav = reinterpret_cast<yar_gl_texture*>(
                        std::get<yar_texture*>(info_iter->descriptor)
                    );
                    glBindImageTexture(0, uav->id, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
                }
            }
        }
    });
}

void gl_cmdBindVertexBuffer(yar_cmd_buffer* cmd, yar_buffer* buffer, uint32_t count, uint32_t offset, uint32_t stride)
{
    cmd->commands.push_back([=]() {
        auto gl_cmd = reinterpret_cast<yar_gl_cmd_buffer*>(cmd);
        GLuint vao = gl_cmd->vao;

        glVertexArrayVertexBuffer(vao, 0, buffer->id, offset, stride); // maybe need to store binding?
        for (int i = 0; i < count; ++i)
            glEnableVertexArrayAttrib(vao, i); // probably need to store somewhere
    });
}

void gl_cmdBindIndexBuffer(yar_cmd_buffer* cmd, yar_buffer* buffer)
{
    cmd->commands.push_back([=]() {
        auto gl_cmd = reinterpret_cast<yar_gl_cmd_buffer*>(cmd);
        GLuint vao = gl_cmd->vao;

        glVertexArrayElementBuffer(vao, buffer->id);
    });
}

void gl_cmdBindPushConstant(yar_cmd_buffer* cmd, void* data)
{
    uint32_t size = cmd->push_constant->size;
    uint32_t pc_id = cmd->push_constant->buffer->id;
    uint32_t binding = cmd->push_constant->binding;
    std::vector<uint8_t> data_copy((uint8_t*)data, (uint8_t*)data + size);
    
    cmd->commands.push_back([=]() {
        glNamedBufferSubData(pc_id, 0, size, data_copy.data());
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, pc_id);
    });
}

void gl_cmdBeginRenderPass(yar_cmd_buffer* cmd, yar_render_pass_desc* desc)
{
    cmd->commands.push_back([=]() {
        auto gl_cmd = reinterpret_cast<yar_gl_cmd_buffer*>(cmd);
        uint8_t color_attachment_count = desc->color_attachment_count;
        for (size_t i = 0; i < color_attachment_count; ++i)
        {
            yar_render_target* target = desc->color_attachments[i].target;
            auto gl_texture = reinterpret_cast<yar_gl_texture*>(target->texture);

            if (gl_texture->type == yar_gl_texture_type::yar_type_texture)
            {
                glNamedFramebufferTexture(gl_cmd->fbo,
                    GL_COLOR_ATTACHMENT0 + i, gl_texture->id, 0);
            }
            else
            {
                glNamedFramebufferRenderbuffer(gl_cmd->fbo,
                    GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, gl_texture->id);
            }
        }

        yar_render_target* ds_target = desc->depth_stencil_attachment.target;
        if (ds_target)
        {
            auto gl_ds_texture = reinterpret_cast<yar_gl_texture*>(ds_target->texture);

            // TODO: add separate for depth and stencil
            if (gl_ds_texture->type == yar_gl_texture_type::yar_type_texture)
            {
                if (gl_ds_texture->common.format)
                    glNamedFramebufferTexture(gl_cmd->fbo,
                        GL_DEPTH_ATTACHMENT, gl_ds_texture->id, 0);
            }
            else
            {
                glNamedFramebufferRenderbuffer(gl_cmd->fbo,
                    GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl_ds_texture->id);
            }
        }

        GLenum bufs[kMaxColorAttachments];
        if (color_attachment_count)
        {
            for (uint8_t i = 0; i < color_attachment_count; ++i)
                bufs[i] = GL_COLOR_ATTACHMENT0 + i;
            glNamedFramebufferDrawBuffers(gl_cmd->fbo, color_attachment_count, bufs);
        }
        else
        {
            glNamedFramebufferDrawBuffers(gl_cmd->fbo, 0, nullptr);
            glNamedFramebufferReadBuffer(gl_cmd->fbo, GL_NONE);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, gl_cmd->fbo);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            printf("Framebuffer incomplete: 0x%x\n", status);
        }


        // We must allow to write into depth buffer before clear
        glDepthMask(GL_TRUE); 
        // TODO: add color here
        glClearColor(0.0f, 0.5f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    });
}

void gl_cmdEndRenderPass(yar_cmd_buffer* cmd)
{
    cmd->commands.push_back([=]() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    });
}

void gl_cmdDraw(yar_cmd_buffer* cmd, uint32_t first_vertex, uint32_t count)
{
    cmd->commands.push_back([=]() {
        auto gl_cmd = reinterpret_cast<yar_gl_cmd_buffer*>(cmd);
        GLuint vao = gl_cmd->vao;
        GLenum topology = gl_cmd->topology;

        glBindVertexArray(vao);
        glDrawArrays(topology, first_vertex, count);

        if (gl_cmd->scissor_enabled)
        {
            glDisable(GL_SCISSOR_TEST);
            gl_cmd->scissor_enabled = false;
        }
    });
}

void gl_cmdDrawIndexed(yar_cmd_buffer* cmd, uint32_t index_count, yar_index_type type, uint32_t first_index, uint32_t first_vertex)
{
    // probably need to change function signature
    cmd->commands.push_back([=]() {
        auto gl_cmd = reinterpret_cast<yar_gl_cmd_buffer*>(cmd);
        GLuint vao = gl_cmd->vao;
        GLenum topology = gl_cmd->topology;
        GLenum gl_type = util_get_gl_index_type(type);
        GLsizei index_stride = util_get_gl_index_stride(type);

        const void* index_offset = reinterpret_cast<const void*>(
            static_cast<uintptr_t>(first_index * index_stride)
        );

        glBindVertexArray(vao);
        glDrawElementsBaseVertex(topology, index_count, gl_type, index_offset, first_vertex);

        if (gl_cmd->scissor_enabled)
        {
            glDisable(GL_SCISSOR_TEST);
            gl_cmd->scissor_enabled = false;
        }
    });
}

void gl_cmdDispatch(yar_cmd_buffer* cmd, uint32_t num_group_x, uint32_t num_group_y, uint32_t num_group_z)
{
    cmd->commands.push_back([=]() {
        glDispatchCompute(num_group_x, num_group_y, num_group_z);
        // TODO: there is probably should be separate function for barriers
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        }
    );
}

void gl_cmdUpdateBuffer(yar_cmd_buffer* cmd, yar_buffer* buffer, size_t offset, size_t size, void* data)
{
    // HACK HACK HACK
    std::vector<uint8_t> data_copy((uint8_t*)data, (uint8_t*)data + size);

    cmd->commands.push_back([=]() {
        glNamedBufferSubData(buffer->id, offset, size, data_copy.data());
    });
}

void gl_cmdSetViewport(yar_cmd_buffer* cmd, uint32_t width, uint32_t height)
{
    cmd->commands.push_back([=]() {
        glViewport(0, 0, width, height);
    });
}

void gl_cmdSetScissor(yar_cmd_buffer* cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    cmd->commands.push_back([=]() {
        auto* gl_cmd = reinterpret_cast<yar_gl_cmd_buffer*>(cmd);
        gl_cmd->scissor_enabled = true;
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, width, height);
    });
}

void gl_queueSubmit(yar_cmd_queue* queue)
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

void gl_queuePresent(yar_cmd_queue* queue, yar_queue_present_desc* desc)
{
    auto set_vsync = get_swap_interval_func();
    auto swap = get_swap_buffers_func();

    set_vsync(desc->swapchain->vsync);

    auto swapchain = desc->swapchain;
    auto gl_swapchain = reinterpret_cast<yar_gl_swapchain*>(swapchain);
    uint32_t index = swapchain->buffer_index % swapchain->buffer_count;
    yar_render_target* rt = swapchain->render_targets[index];
    auto gl_rt = reinterpret_cast<yar_gl_texture*>(rt->texture);

    glBindFramebuffer(GL_FRAMEBUFFER, gl_swapchain->fbo);

    if (gl_rt->type == yar_gl_texture_type::yar_type_texture) {
        glNamedFramebufferTexture(gl_swapchain->fbo, GL_COLOR_ATTACHMENT0, gl_rt->id, 0);
    }
    else {
        glNamedFramebufferRenderbuffer(gl_swapchain->fbo, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gl_rt->id);
    }

    GLenum bufs[1] = { GL_COLOR_ATTACHMENT0 };
    glNamedFramebufferDrawBuffers(gl_swapchain->fbo, 1, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("swapchain FBO incomplete!\n");
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gl_swapchain->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(
        0, 0, rt->width, rt->height,
        0, 0, rt->width, rt->height,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    );

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    swap(gl_swapchain->window_handle);
    swapchain->buffer_index++;
}

// Maybe need to add some params to this function in future
bool gl_init_render(yar_device* device)
{
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

    device->load_shader             = gl_loadShader;
    device->begin_update_resource   = gl_beginUpdateResource;
    device->end_update_resource     = gl_endUpdateResource;

    device->add_swapchain           = gl_addSwapChain;
    device->add_buffer              = gl_addBuffer;
    device->add_texture             = gl_addTexture;
    device->add_render_target       = gl_addRenderTarget;
    device->add_sampler             = gl_addSampler;
    device->add_shader              = gl_addShader;
    device->add_descriptor_set      = gl_addDescriptorSet;
    device->add_pipeline            = gl_addPipeline;
    device->add_queue               = gl_addQueue;
    device->add_cmd                 = gl_addCmd;
    device->remove_buffer           = gl_removeBuffer;
    device->map_buffer              = gl_mapBuffer;
    device->unmap_buffer            = gl_unmapBuffer;
    device->update_descriptor_set   = gl_updateDescriptorSet;
    device->acquire_next_image      = gl_acquireNextImage;
    device->cmd_bind_pipeline       = gl_cmdBindPipeline;
    device->cmd_bind_descriptor_set = gl_cmdBindDescriptorSet;
    device->cmd_bind_vertex_buffer  = gl_cmdBindVertexBuffer;
    device->cmd_bind_index_buffer   = gl_cmdBindIndexBuffer;
    device->cmd_bind_push_constant  = gl_cmdBindPushConstant;
    device->cmd_begin_render_pass   = gl_cmdBeginRenderPass;
    device->cmd_end_render_pass     = gl_cmdEndRenderPass;
    device->cmd_draw                = gl_cmdDraw;
    device->cmd_draw_indexed        = gl_cmdDrawIndexed;
    device->cmd_dispatch            = gl_cmdDispatch;
    device->cmd_update_buffer       = gl_cmdUpdateBuffer;
    device->cmd_set_viewport        = gl_cmdSetViewport;
    device->cmd_set_scissor         = gl_cmdSetScissor;
    device->queue_submit            = gl_queueSubmit;
    device->queue_present           = gl_queuePresent;

#if _DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(util_debug_message_callback, nullptr);
#endif

    return true;
}





