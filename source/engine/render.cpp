#include "render.h"
#include "render_internal.h"

static yar_device* device{ nullptr };

void load_shader(ShaderLoadDesc* desc, ShaderDesc** out)
{
    if (device && device->load_shader)
        device->load_shader(desc, out);
}

void begin_update_resource(ResourceUpdateDesc& desc)
{
    if (device && device->begin_update_resource)
        device->begin_update_resource(desc);
}

void end_update_resource(ResourceUpdateDesc& desc)
{
    if (device && device->end_update_resource)
        device->end_update_resource(desc);
}

void update_texture(TextureUpdateDesc* desc)
{
    if (device && device->update_texture)
        device->update_texture(desc);
}

void* map_buffer(Buffer* buffer)
{
    if (device && device->map_buffer)
        return device->map_buffer(buffer);
    return nullptr;
}

void unmap_buffer(Buffer* buffer)
{
    if (device && device->unmap_buffer)
        device->unmap_buffer(buffer);
}

void add_swapchain(bool vsync, SwapChain** swapchain)
{
    if (device && device->add_swapchain)
        device->add_swapchain(vsync, swapchain);
}

void add_buffer(BufferDesc* desc, Buffer** buffer)
{
    if (device && device->add_buffer)
        device->add_buffer(desc, buffer);
}

void add_texture(TextureDesc* desc, Texture** texture)
{
    if (device && device->add_texture)
        device->add_texture(desc, texture);
}

void add_sampler(SamplerDesc* desc, Sampler** sampler)
{
    if (device && device->add_sampler)
        device->add_sampler(desc, sampler);
}

void add_shader(ShaderDesc* desc, Shader** shader)
{
    if (device && device->add_shader)
        device->add_shader(desc, shader);
}

void add_descriptor_set(DescriptorSetDesc* desc, DescriptorSet** set)
{
    if (device && device->add_descriptor_set)
        device->add_descriptor_set(desc, set);
}

// void add_root_signature(RootSignatureDesc* desc, RootSignature** root_signature)
// {
//     if (device && device->add_root_signature)
//         device->add_root_signature(desc, root_signature);
// }

void add_pipeline(const PipelineDesc* desc, Pipeline** pipeline)
{
    if (device && device->add_pipeline)
        device->add_pipeline(desc, pipeline);
}

void add_queue(CmdQueueDesc* desc, CmdQueue** queue)
{
    if (device && device->add_queue)
        device->add_queue(desc, queue);
}

void add_cmd(CmdBufferDesc* desc, CmdBuffer** cmd)
{
    if (device && device->add_cmd)
        device->add_cmd(desc, cmd);
}

void remove_buffer(Buffer* buffer)
{
    if (device && device->remove_buffer)
        device->remove_buffer(buffer);
}

void update_descriptor_set(UpdateDescriptorSetDesc* desc, DescriptorSet* set)
{
    if (device && device->update_descriptor_set)
        device->update_descriptor_set(desc, set);
}

void cmd_bind_pipeline(CmdBuffer* cmd, Pipeline* pipeline)
{
    if (device && device->cmd_bind_pipeline)
        device->cmd_bind_pipeline(cmd, pipeline);
}

void cmd_bind_descriptor_set(CmdBuffer* cmd, DescriptorSet* set, uint32_t index)
{
    if (device && device->cmd_bind_descriptor_set)
        device->cmd_bind_descriptor_set(cmd, set, index);
}

void cmd_bind_vertex_buffer(CmdBuffer* cmd, Buffer* buffer, uint32_t count, uint32_t offset, uint32_t stride)
{
    if (device && device->cmd_bind_vertex_buffer)
        device->cmd_bind_vertex_buffer(cmd, buffer, count, offset, stride);
}

void cmd_bind_index_buffer(CmdBuffer* cmd, Buffer* buffer)
{
    if (device && device->cmd_bind_index_buffer)
        device->cmd_bind_index_buffer(cmd, buffer);
}

void cmd_bind_push_constant(CmdBuffer* cmd, void* data)
{
    if (device && device->cmd_bind_push_constant)
        device->cmd_bind_push_constant(cmd, data);
}

void cmd_draw(CmdBuffer* cmd, uint32_t first_vertex, uint32_t count)
{
    if (device && device->cmd_draw)
        device->cmd_draw(cmd, first_vertex, count);
}

void cmd_draw_indexed(CmdBuffer* cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex)
{
    if (device && device->cmd_draw_indexed)
        device->cmd_draw_indexed(cmd, index_count, first_index, first_vertex);
}

void cmd_dispatch(CmdBuffer* cmd, uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
    if (device && device->cmd_dispatch)
        device->cmd_dispatch(cmd, num_groups_x, num_groups_y, num_groups_z);
}

void cmd_update_buffer(CmdBuffer* cmd, Buffer* buffer, size_t offset, size_t size, void* data)
{
    if (device && device->cmd_update_buffer)
        device->cmd_update_buffer(cmd, buffer, offset, size, data);
}

void queue_submit(CmdQueue* queue)
{
    if (device && device->queue_submit)
        device->queue_submit(queue);
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