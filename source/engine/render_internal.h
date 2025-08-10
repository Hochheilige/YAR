#pragma once

#include "render.h"

struct yar_device
{
    // ======================================= //
    //            Load Functions               //
    // ======================================= //

    void (*load_shader)(ShaderLoadDesc* desc, ShaderDesc** out);
    void (*begin_update_resource)(ResourceUpdateDesc& desc);
    void (*end_update_resource)(ResourceUpdateDesc& desc);
    void (*update_texture)(TextureUpdateDesc* desc);
    void* (*map_buffer)(Buffer* buffer);
    void (*unmap_buffer)(Buffer* buffer);

    // ======================================= //
    //            Render Functions             //
    // ======================================= //

    void (*add_swapchain)(bool vsync, SwapChain** swapchain);
    void (*add_buffer)(BufferDesc* desc, Buffer** buffer);
    void (*add_texture)(TextureDesc* desc, Texture** texture);
    void (*add_sampler)(SamplerDesc* desc, Sampler** sampler);
    void (*add_shader)(ShaderDesc* desc, Shader** shader);
    void (*add_descriptor_set)(DescriptorSetDesc* desc, DescriptorSet** shader);
    // void (*add_root_signature)(RootSignatureDesc* desc, RootSignature** root_signature);
    void (*add_pipeline)(const PipelineDesc* desc, Pipeline** pipeline);
    void (*add_queue)(CmdQueueDesc* desc, CmdQueue** queue);
    void (*add_cmd)(CmdBufferDesc* desc, CmdBuffer** cmd);
    void (*remove_buffer)(Buffer* buffer);
    void (*update_descriptor_set)(UpdateDescriptorSetDesc* desc, DescriptorSet* set);
    void (*cmd_bind_pipeline)(CmdBuffer* cmd, Pipeline* pipeline);
    void (*cmd_bind_descriptor_set)(CmdBuffer* cmd, DescriptorSet* set, uint32_t index);
    void (*cmd_bind_vertex_buffer)(CmdBuffer* cmd, Buffer* buffer, uint32_t count, uint32_t offset, uint32_t stride);
    void (*cmd_bind_index_buffer)(CmdBuffer* cmd, Buffer* buffer);
    void (*cmd_bind_push_constant)(CmdBuffer* cmd, void* data);
    void (*cmd_draw)(CmdBuffer* cmd, uint32_t first_vertex, uint32_t count);
    void (*cmd_draw_indexed)(CmdBuffer* cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex);
    void (*cmd_dispatch)(CmdBuffer* cmd, uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z);
    void (*cmd_update_buffer)(CmdBuffer* cmd, Buffer* buffer, size_t offset, size_t size, void* data);
    void (*queue_submit)(CmdQueue* queue);
};
