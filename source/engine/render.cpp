#include "render.h"
#include "render_internal.h"

static yar_device* device{ nullptr };

void load_shader(yar_shader_load_desc* desc, yar_shader_desc** out)
{
    if (device && device->load_shader)
        device->load_shader(desc, out);
}

void begin_update_resource(yar_resource_update_desc& desc)
{
    if (device && device->begin_update_resource)
        device->begin_update_resource(desc);
}

void end_update_resource(yar_resource_update_desc& desc)
{
    if (device && device->end_update_resource)
        device->end_update_resource(desc);
}

void update_texture(yar_texture_update_desc* desc)
{
    if (device && device->update_texture)
        device->update_texture(desc);
}

void* map_buffer(yar_buffer* buffer)
{
    if (device && device->map_buffer)
        return device->map_buffer(buffer);
    return nullptr;
}

void unmap_buffer(yar_buffer* buffer)
{
    if (device && device->unmap_buffer)
        device->unmap_buffer(buffer);
}

void add_swapchain(yar_swapchain_desc* desc, yar_swapchain** swapchain)
{
    if (device && device->add_swapchain)
        device->add_swapchain(desc, swapchain);
}

void add_buffer(yar_buffer_desc* desc, yar_buffer** buffer)
{
    if (device && device->add_buffer)
        device->add_buffer(desc, buffer);
}

void add_texture(yar_texture_desc* desc, yar_texture** texture)
{
    if (device && device->add_texture)
        device->add_texture(desc, texture);
}

void add_render_target(yar_render_target_desc* desc, yar_render_target** rt)
{
    if (device && device->add_render_target)
        device->add_render_target(desc, rt);
}

void add_sampler(yar_sampler_desc* desc, yar_sampler** sampler)
{
    if (device && device->add_sampler)
        device->add_sampler(desc, sampler);
}

void add_shader(yar_shader_desc* desc, yar_shader** shader)
{
    if (device && device->add_shader)
        device->add_shader(desc, shader);
}

void add_descriptor_set(yar_descriptor_set_desc* desc, yar_descriptor_set** set)
{
    if (device && device->add_descriptor_set)
        device->add_descriptor_set(desc, set);
}

// void add_root_signature(RootSignatureDesc* desc, RootSignature** root_signature)
// {
//     if (device && device->add_root_signature)
//         device->add_root_signature(desc, root_signature);
// }

void add_pipeline(yar_pipeline_desc* desc, yar_pipeline** pipeline)
{
    if (device && device->add_pipeline)
        device->add_pipeline(desc, pipeline);
}

void add_queue(yar_cmd_queue_desc* desc, yar_cmd_queue** queue)
{
    if (device && device->add_queue)
        device->add_queue(desc, queue);
}

void add_cmd(yar_cmd_buffer_desc* desc, yar_cmd_buffer** cmd)
{
    if (device && device->add_cmd)
        device->add_cmd(desc, cmd);
}

void remove_buffer(yar_buffer* buffer)
{
    if (device && device->remove_buffer)
        device->remove_buffer(buffer);
}

void update_descriptor_set(yar_update_descriptor_set_desc* desc, yar_descriptor_set* set)
{
    if (device && device->update_descriptor_set)
        device->update_descriptor_set(desc, set);
}

void acquire_next_image(yar_swapchain* swapchain, uint32_t& swapchain_index)
{
    if (device && device->acquire_next_image)
        device->acquire_next_image(swapchain, swapchain_index);
}

void cmd_bind_pipeline(yar_cmd_buffer* cmd, yar_pipeline* pipeline)
{
    if (device && device->cmd_bind_pipeline)
        device->cmd_bind_pipeline(cmd, pipeline);
}

void cmd_bind_descriptor_set(yar_cmd_buffer* cmd, yar_descriptor_set* set, uint32_t index)
{
    if (device && device->cmd_bind_descriptor_set)
        device->cmd_bind_descriptor_set(cmd, set, index);
}

void cmd_bind_vertex_buffer(yar_cmd_buffer* cmd, yar_buffer* buffer, uint32_t count, uint32_t offset, uint32_t stride)
{
    if (device && device->cmd_bind_vertex_buffer)
        device->cmd_bind_vertex_buffer(cmd, buffer, count, offset, stride);
}

void cmd_bind_index_buffer(yar_cmd_buffer* cmd, yar_buffer* buffer)
{
    if (device && device->cmd_bind_index_buffer)
        device->cmd_bind_index_buffer(cmd, buffer);
}

void cmd_bind_push_constant(yar_cmd_buffer* cmd, void* data)
{
    if (device && device->cmd_bind_push_constant)
        device->cmd_bind_push_constant(cmd, data);
}

void cmd_begin_render_pass(yar_cmd_buffer* cmd, yar_render_pass_desc* desc)
{
    if (device && device->cmd_begin_render_pass)
        device->cmd_begin_render_pass(cmd, desc);
}

void cmd_end_render_pass(yar_cmd_buffer* cmd)
{
    if (device && device->cmd_end_render_pass)
        device->cmd_end_render_pass(cmd);
}

void cmd_draw(yar_cmd_buffer* cmd, uint32_t first_vertex, uint32_t count)
{
    if (device && device->cmd_draw)
        device->cmd_draw(cmd, first_vertex, count);
}

void cmd_draw_indexed(yar_cmd_buffer* cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex)
{
    if (device && device->cmd_draw_indexed)
        device->cmd_draw_indexed(cmd, index_count, first_index, first_vertex);
}

void cmd_dispatch(yar_cmd_buffer* cmd, uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
    if (device && device->cmd_dispatch)
        device->cmd_dispatch(cmd, num_groups_x, num_groups_y, num_groups_z);
}

void cmd_update_buffer(yar_cmd_buffer* cmd, yar_buffer* buffer, size_t offset, size_t size, void* data)
{
    if (device && device->cmd_update_buffer)
        device->cmd_update_buffer(cmd, buffer, offset, size, data);
}

void queue_submit(yar_cmd_queue* queue)
{
    if (device && device->queue_submit)
        device->queue_submit(queue);
}

void queue_present(yar_cmd_queue* queue, yar_queue_present_desc* desc)
{
    if (device && device->queue_present)
        device->queue_present(queue, desc);
}

extern bool gl_init_render(yar_device* device);

void init_render()
{
    if (device == nullptr)
    {
        device = static_cast<yar_device*>(std::malloc(sizeof(yar_device)));
    }

    gl_init_render(device);
}