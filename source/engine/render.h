#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <queue>
#include <functional>
#include <variant>
#include <set>

// ======================================= //
//            Load Structures              //
// ======================================= //

struct yar_buffer;
struct yar_texture;

struct yar_buffer_update_desc
{
    yar_buffer* buffer;
    uint64_t size;
    // also can add offset if need to update part of a buffer
    // or buffer from some point
    void* mapped_data;
};

struct yar_texture_update_desc
{
    yar_texture* texture;
    uint64_t size;
    uint8_t* data;
    void* mapped_data;
};

using yar_resource_update_desc = std::variant<yar_buffer_update_desc*, yar_texture_update_desc*>;

// ======================================= //
//            Render Structures            //
// ======================================= //

/*
    Need to make all this things more abstract in case of using different 
    render api, or just add defines to separate parts of this structs

    Plans to add:
        - Render yar_pipeline that should include:
            - Blend state
            - Rasterization state
            - Depth|Stencil state
            - Input Assembly 
            - Vertex Input
            - Viewport|Scissor
            - Multisampling (?)
        - yar_shader Reflection
            - Need to add spirv dependency to make it
            - want to use it 
        - Buffers and Textures
            - If we speak about uniforms there probably should be
                a thing like yar_descriptor_set
            - Another thing - Vertex and Index buffers
        - Command Queue 
            - as far as OpenGL render immediately 
*/

// TODO: Command probably have to be a struct for other rander API
using yar_command = std::function<void()>;
using yar_descriptor_index_map = std::unordered_map<std::string, uint32_t>;

// Interesting thing got it from The-Forge
#define MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)                                                                        \
	static inline ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b) { return (ENUM_TYPE)((TYPE)(a) | (TYPE)(b)); } \
	static inline ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b) { return (ENUM_TYPE)((TYPE)(a) & (TYPE)(b)); } \
	static inline ENUM_TYPE operator|=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a | b); }                      \
	static inline ENUM_TYPE operator&=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a & b); }

constexpr uint8_t kMaxVertexAttribCount = 16u;
constexpr uint8_t kMaxColorAttachments = 8u;

struct yar_swapchain
{
    // this implementation is probably good only for OpenGL
    bool vsync;
    void* window;
    void(*swap_buffers)(void*);
};

enum yar_buffer_usage : uint8_t
{
    yar_buffer_usage_none          = 0,
    yar_buffer_usage_transfer_src   = 0x00000001,
    yar_buffer_usage_transfer_dst   = 0x00000002,
    yar_buffer_usage_uniform_buffer = 0x00000004,
    yar_buffer_usage_storage_buffer = 0x00000008,
    yar_buffer_usage_index_buffer   = 0x00000010,
    yar_buffer_usage_vertex_buffer  = 0x00000020,
    // there are more types but I use only this now
    yar_buffe_usage_max           = 6
};

enum yar_buffer_flag : uint8_t
{
    yar_buffer_flag_none          = 0, 
    yar_buffer_flag_gpu_only       = 0x00000001,
    yar_buffer_flag_dynamic       = 0x00000002,
    yar_buffer_flag_map_read       = 0x00000004,
    yar_buffer_flag_map_write      = 0x00000008,
    yar_buffer_flag_map_read_write  = 0x00000010,
    yar_buffer_flag_map_persistent = 0x00000020,
    yar_buffer_flag_map_coherent   = 0x00000040,
    yar_buffer_flag_client_storage = 0x00000080,
    yar_buffer_flag_max           = 7
};
MAKE_ENUM_FLAG(uint8_t, yar_buffer_flag);

struct yar_buffer_desc
{
    uint32_t size;
    yar_buffer_usage usage;
    yar_buffer_flag flags;
};

struct yar_buffer
{
    uint32_t id;
    yar_buffer_flag flags;
};

enum yar_texture_type
{
    yar_texture_type_none = 0,
    yar_texture_type_1d,
    yar_texture_type_2d,
    yar_texture_type_3d,
    yar_texture_type_1d_array,
    yar_texture_type_2d_array,
    yar_texture_type_cube_map
};

enum yar_texture_format : uint8_t
{
    yar_texture_format_none = 0,
    yar_texture_format_r8,
    yar_texture_format_rgb8,
    yar_texture_format_rgba8,
    yar_texture_format_rgb16f,
    yar_texture_format_rgba16f,
    yar_texture_format_rgba32f,
    yar_texture_format_depth16,
    yar_texture_format_depth24,
    yar_texture_format_depth32f,
    yar_texture_format_depth24_stencil8,
};

enum yar_texture_usage : uint8_t
{
    yar_texture_usage_none = 0, 
    yar_texture_usage_render_target = 1 << 0, // WEIRD
    yar_texture_usage_depth_stencil = 1 << 1,
    yar_texture_usage_shader_resource = 1 << 2,
    yar_texture_usage_unordered_access = 1 << 3,
};
MAKE_ENUM_FLAG(uint8_t, yar_texture_usage);

struct yar_texture_desc
{
    yar_texture_type type;
    yar_texture_format format;
    yar_texture_usage usage;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_size;
    uint32_t mip_levels;
};

struct yar_texture
{
    yar_texture_type type;
    yar_texture_format format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_size;
    uint32_t mip_levels;
};

enum yar_filter_type
{
    yar_filter_type_none = 0,
    yar_filter_type_nearest,
    yar_filter_type_linear,
};

enum yar_wrap_mode
{
    yar_wrap_mode_repeat,
    yar_wrap_mode_mirrored,
    yar_wrap_mode_clamp_edge,
    yar_wrap_mode_clamp_border
};

struct yar_sampler_desc
{
    yar_filter_type min_filter;
    yar_filter_type mag_filter;
    yar_filter_type mip_map_filter;
    yar_wrap_mode wrap_u;
    yar_wrap_mode wrap_v;
    yar_wrap_mode wrap_w;
};

struct yar_sampler
{
    uint32_t id;
};

enum yar_shader_stage : uint8_t
{
    yar_shader_stage_none = 0,
    yar_shader_stage_vert = 0x00000001,
    yar_shader_stage_pixel = 0x00000002,
    yar_shader_stage_geom = 0x00000004,
    yar_shader_stage_comp = 0x00000008,
    yar_shader_stage_tese = 0x00000010,
    yar_shader_stage_max = 5
};
MAKE_ENUM_FLAG(uint8_t, yar_shader_stage);

enum yar_resource_type : uint8_t
{
    yar_resource_type_undefined = 0x00000000,
    yar_resource_type_cbv = 0x00000001,
    yar_resource_type_srv = 0x00000002,
    yar_resource_type_uav = 0x00000004,
    yar_resource_type_sampler = 0x00000008,
};
MAKE_ENUM_FLAG(uint8_t, yar_resource_type);

enum yar_vertex_attrib_format : uint8_t
{
    yar_attrib_format_float = 0,
};

enum yar_depth_stencil_func : uint8_t
{
    yar_depth_stencil_func_always = 0,
    yar_depth_stencil_func_never,      
    yar_depth_stencil_func_less,       
    yar_depth_stencil_func_equal,      
    yar_depth_stencil_func_less_equal,  
    yar_depth_stencil_func_greater,   
    yar_depth_stencil_func_not_equal,   
    yar_depth_stencil_func_great_equal 
};

enum yar_stencil_op : uint8_t
{
    yar_stencil_op_keep = 0,
    yar_stencil_op_zero,
    yar_stencil_op_replace,
    yar_stencil_op_incr,
    yar_stencil_op_incr_wrap,
    yar_stencil_op_decr,
    yar_stencil_op_decr_wrap,
    yar_stencil_op_invert
};

enum yar_blend_op : uint8_t
{
    yar_blend_op_add = 0,
    yar_blend_op_subtract,
    yar_blend_op_reverse_subtract,
    yar_blend_op_min,
    yar_blend_op_max
};

enum yar_blend_factor : uint8_t
{
    yar_blend_factor_zero = 0,
    yar_blend_factor_one,
    yar_blend_factor_src_color,
    yar_blend_factor_one_minus_src_color,
    yar_blend_factor_dst_color,
    yar_blend_factor_one_minus_dst_color,
    yar_blend_factor_src_alpha,
    yar_blend_factor_one_minus_src_alpha,
    yar_blend_factor_dst_alpha,
    yar_blned_factor_one_minus_dst_alpha
};

enum yar_fill_mode : uint8_t
{
    yar_fill_mode_solid = 0,
    yar_fill_mode_wireframe
};

enum yar_cull_mode : uint8_t
{
    yar_cull_mode_none = 0,
    yar_cull_mode_front,
    yar_cull_mode_back
};

enum yar_primitive_topology : uint8_t
{
    yar_primitive_topology_triangle_list = 0,
    yar_primitive_topology_triangle_strip,
    yar_primitive_topology_line_list,
    yar_primitive_topology_line_strip,
    yar_primitive_topology_point_list
};

enum yar_pipeline_type : uint8_t
{
    yar_pipeline_type_graphics = 0,
    yar_pipeline_type_compute
};

enum yar_descriptor_set_update_frequency : uint8_t
{
    yar_update_freq_none = 0,
    yar_update_freq_per_frame = 1,
    yar_update_freq_per_draw = 2,
    yar_update_freq_max = 3
};

struct yar_shader_resource
{
    std::string name;
    yar_resource_type type;
    uint32_t binding;
    uint32_t set;
    // maybe also need to add texture specific things

    bool operator<(const yar_shader_resource& other) const {
        if (type == other.type)
            return name < other.name;
        return type < other.type;
    }
};

struct yar_shader_stage_load_desc
{
    std::string file_name;
    std::string entry_point;
    yar_shader_stage stage;
    // maybe need to add some shader macros support here
};

struct yar_shader_load_desc
{
    std::array<yar_shader_stage_load_desc, yar_shader_stage::yar_shader_stage_max> stages;
};

struct yar_shader_stage_desc
{
    std::vector<uint8_t> byte_code;
    std::string_view entry_point;
};

struct yar_shader_desc
{
    yar_shader_stage stages;
    yar_shader_stage_desc vert;
    yar_shader_stage_desc pixel;
    yar_shader_stage_desc geom;
    yar_shader_stage_desc comp;
};

struct yar_shader
{
    yar_shader_stage stages;
};

struct yar_vertex_attrib
{
    uint32_t size;
    yar_vertex_attrib_format format;
    uint32_t binding;
    uint32_t offset;    
};

struct yar_vertex_layout
{
    uint32_t attrib_count;
    yar_vertex_attrib attribs[kMaxVertexAttribCount];
};

struct yar_depth_stencil_state
{
    bool depth_enable;
    bool stencil_enable;
    yar_depth_stencil_func depth_func;
    yar_depth_stencil_func stencil_func;
    yar_stencil_op sfail;
    yar_stencil_op dpfail;
    yar_stencil_op dppass;
};

struct yar_blend_state
{
    bool blend_enable;
    yar_blend_factor src_factor;
    yar_blend_factor dst_factor;
    yar_blend_factor src_alpha_factor;
    yar_blend_factor dst_alpha_factor;
    yar_blend_op op;
    yar_blend_op alpha_op;
};

struct yar_rasterizer_state
{
    yar_fill_mode fill_mode;
    yar_cull_mode cull_mode;
    bool front_counter_clockwise;
    // TODO: add depth bias and stuff
};

// Tried to add RootSignature abstraction 
// but don't see the point of it here right now

//struct RootSignatureDesc
//{
//    std::vector<yar_shader*> shaders;
//};
//
//struct RootSignature
//{
//    std::vector<ShaderResource> descriptors;
//    DescriptorIndexMap name_to_index;
//};

struct yar_descriptor_set_desc
{
    yar_descriptor_set_update_frequency update_freq;
    uint32_t max_sets;
    yar_shader* shader;
};

struct yar_descriptor_info
{
    // probably need only for ogl
    struct yar_combined_texture_sample
    {
        yar_texture* texture;
        std::string sampler_name;
    };

    std::string name;
    std::variant<yar_buffer*, yar_sampler*, yar_combined_texture_sample, yar_texture*> descriptor;
};

struct yar_update_descriptor_set_desc
{
    uint32_t index;
    std::vector<yar_descriptor_info> infos;
};

// Idea of Descriptor Set doesn't look really useful in OpenGL
// but I want to make this abstraction in case of using
// modern Graphics API like Vulkan in future
struct yar_descriptor_set
{
    // I don't want to have uniform buffer directly in pipeline object
    // so it is better to use Descriptor Set that is linked with current
    // buffer to bind buffer with current context
    yar_descriptor_set_update_frequency update_freq;
    uint32_t max_set;
    std::set<yar_shader_resource> descriptors;
    std::vector<std::vector<yar_descriptor_info>> infos;
};

struct yar_push_constant_desc
{
    yar_shader* shader;
    std::string name;
    uint32_t size;
};

struct yar_push_constant
{
    yar_buffer* buffer;
    uint32_t size;
    uint32_t binding;
};

struct yar_pipeline_desc
{
    // Split shader to vertex and pixel
    yar_shader*             shader;
    yar_rasterizer_state    rasterizer_state;
    yar_depth_stencil_state depth_stencil_state;
    yar_blend_state         blend_state;
    yar_vertex_layout       vertex_layout;
    yar_primitive_topology  topology;
    // TODO: add samples
};

// TODO: split it to Graphics and Compute pipeline
struct yar_pipeline
{
    yar_pipeline_type type;
};

struct yar_cmd_queue_desc
{
    uint8_t dummy;
};

struct yar_cmd_buffer;

struct yar_cmd_queue
{
    std::vector<yar_cmd_buffer*> queue;
};

struct yar_cmd_buffer_desc
{
    yar_cmd_queue* current_queue;
    bool use_push_constant;
    yar_push_constant_desc* pc_desc;
};

struct yar_cmd_buffer
{
    std::vector<yar_command> commands;
    yar_push_constant* push_constant;
};

struct yar_render_target_desc
{
    // It is literally the same as texture_desc now
    yar_texture_type type;
    yar_texture_format format;
    yar_texture_usage usage;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_size;
    uint32_t mip_levels;
};

struct yar_render_target
{
    yar_texture* texture;
    uint32_t width;
    uint32_t height;
};

struct yar_attachment_desc
{
    yar_render_target* target;
};

struct yar_render_pass_desc
{
    uint8_t color_attachment_count;
    yar_attachment_desc color_attachments[kMaxColorAttachments];
    yar_attachment_desc depth_stencil_attachment;
};

// ======================================= //
//            Load Functions               //
// ======================================= //

#define DECLARE_YAR_LOAD_FUNC(ret, name, ...) \
ret name(__VA_ARGS__)                         \

DECLARE_YAR_LOAD_FUNC(void, load_shader, yar_shader_load_desc* desc, yar_shader_desc** out);
DECLARE_YAR_LOAD_FUNC(void, begin_update_resource, yar_resource_update_desc& desc);
DECLARE_YAR_LOAD_FUNC(void, end_update_resource, yar_resource_update_desc& desc);
DECLARE_YAR_LOAD_FUNC(void, update_texture, yar_texture_update_desc* desc);
DECLARE_YAR_LOAD_FUNC(void*, map_buffer, yar_buffer* buffer);
DECLARE_YAR_LOAD_FUNC(void, unmap_buffer, yar_buffer* buffer);

// ======================================= //
//            Render Functions             //
// ======================================= //

#define DECLARE_YAR_RENDER_FUNC(ret, name, ...) \
ret name(__VA_ARGS__)                           \

DECLARE_YAR_RENDER_FUNC(void, add_swapchain, bool vsync, yar_swapchain** swapchain);
DECLARE_YAR_RENDER_FUNC(void, add_buffer, yar_buffer_desc* desc, yar_buffer** buffer);
DECLARE_YAR_RENDER_FUNC(void, add_texture, yar_texture_desc* desc, yar_texture** texture);
DECLARE_YAR_RENDER_FUNC(void, add_render_target, yar_render_target_desc* desc, yar_render_target* rt);
DECLARE_YAR_RENDER_FUNC(void, add_sampler, yar_sampler_desc* desc, yar_sampler** sampler);
DECLARE_YAR_RENDER_FUNC(void, add_shader, yar_shader_desc* desc, yar_shader** shader);
DECLARE_YAR_RENDER_FUNC(void, add_descriptor_set, yar_descriptor_set_desc* desc, yar_descriptor_set** set);
//DECLARE_YAR_RENDER_FUNC(void, add_root_signature, RootSignatureDesc* desc, RootSignature** root_signature);
DECLARE_YAR_RENDER_FUNC(void, add_pipeline, yar_pipeline_desc* desc, yar_pipeline** pipeline);
DECLARE_YAR_RENDER_FUNC(void, add_queue, yar_cmd_queue_desc* desc, yar_cmd_queue** queue);
DECLARE_YAR_RENDER_FUNC(void, add_cmd, yar_cmd_buffer_desc* desc, yar_cmd_buffer** cmd);
DECLARE_YAR_RENDER_FUNC(void, remove_buffer, yar_buffer* buffer);
DECLARE_YAR_RENDER_FUNC(void, update_descriptor_set, yar_update_descriptor_set_desc* desc, yar_descriptor_set* set);
DECLARE_YAR_RENDER_FUNC(void, cmd_bind_pipeline, yar_cmd_buffer* cmd, yar_pipeline* pipeline);
DECLARE_YAR_RENDER_FUNC(void, cmd_bind_descriptor_set, yar_cmd_buffer* cmd, yar_descriptor_set* set, uint32_t index);
DECLARE_YAR_RENDER_FUNC(void, cmd_bind_vertex_buffer, yar_cmd_buffer* cmd, yar_buffer* buffer, uint32_t count, uint32_t offset, uint32_t stride);
DECLARE_YAR_RENDER_FUNC(void, cmd_bind_index_buffer, yar_cmd_buffer* cmd, yar_buffer* buffer); // maybe for other render api there should be more params
DECLARE_YAR_RENDER_FUNC(void, cmd_bind_push_constant, yar_cmd_buffer* cmd, void* data);
DECLARE_YAR_RENDER_FUNC(void, cmd_begin_render_pass, yar_cmd_buffer* cmd, yar_render_pass_desc* desc);
DECLARE_YAR_RENDER_FUNC(void, cmd_end_render_pass, yar_cmd_buffer* cmd);
DECLARE_YAR_RENDER_FUNC(void, cmd_draw, yar_cmd_buffer* cmd, uint32_t first_vertex, uint32_t count);
DECLARE_YAR_RENDER_FUNC(void, cmd_draw_indexed, yar_cmd_buffer* cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex);
DECLARE_YAR_RENDER_FUNC(void, cmd_dispatch, yar_cmd_buffer* cmd, uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z);
DECLARE_YAR_RENDER_FUNC(void, cmd_update_buffer, yar_cmd_buffer* cmd, yar_buffer* buffer, size_t offset, size_t size, void* data);
DECLARE_YAR_RENDER_FUNC(void, queue_submit, yar_cmd_queue* queue);

void init_render();
