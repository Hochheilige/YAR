#pragma once

#include "render.h"

struct yar_device
{
    // ======================================= //
    //            Load Functions               //
    // ======================================= //

    void (*load_shader)(yar_shader_load_desc* desc, yar_shader_desc** out);
    void (*begin_update_resource)(yar_resource_update_desc& desc);
    void (*end_update_resource)(yar_resource_update_desc& desc);
    void (*update_texture)(yar_texture_update_desc* desc);
    void* (*map_buffer)(yar_buffer* buffer);
    void (*unmap_buffer)(yar_buffer* buffer);

    // ======================================= //
    //            Render Functions             //
    // ======================================= //

    void (*add_swapchain)(yar_swapchain_desc* desc, yar_swapchain** swapchain);
    void (*add_buffer)(yar_buffer_desc* desc, yar_buffer** buffer);
    void (*add_texture)(yar_texture_desc* desc, yar_texture** texture);
    void (*add_render_target)(yar_render_target_desc* desc, yar_render_target** rt);
    void (*add_sampler)(yar_sampler_desc* desc, yar_sampler** sampler);
    void (*add_shader)(yar_shader_desc* desc, yar_shader** shader);
    void (*add_descriptor_set)(yar_descriptor_set_desc* desc, yar_descriptor_set** shader);
    // void (*add_root_signature)(RootSignatureDesc* desc, RootSignature** root_signature);
    void (*add_pipeline)(yar_pipeline_desc* desc, yar_pipeline** pipeline);
    void (*add_queue)(yar_cmd_queue_desc* desc, yar_cmd_queue** queue);
    void (*add_cmd)(yar_cmd_buffer_desc* desc, yar_cmd_buffer** cmd);
    void (*remove_buffer)(yar_buffer* buffer);
    void (*update_descriptor_set)(yar_update_descriptor_set_desc* desc, yar_descriptor_set* set);
    void (*acquire_next_image)(yar_swapchain* swapchain, uint32_t& swapchain_index);
    void (*cmd_bind_pipeline)(yar_cmd_buffer* cmd, yar_pipeline* pipeline);
    void (*cmd_bind_descriptor_set)(yar_cmd_buffer* cmd, yar_descriptor_set* set, uint32_t index);
    void (*cmd_bind_vertex_buffer)(yar_cmd_buffer* cmd, yar_buffer* buffer, uint32_t count, uint32_t offset, uint32_t stride);
    void (*cmd_bind_index_buffer)(yar_cmd_buffer* cmd, yar_buffer* buffer);
    void (*cmd_bind_push_constant)(yar_cmd_buffer* cmd, void* data);
    void (*cmd_begin_render_pass)(yar_cmd_buffer* cmd, yar_render_pass_desc* desc);
    void (*cmd_end_render_pass)(yar_cmd_buffer* cmd);
    void (*cmd_draw)(yar_cmd_buffer* cmd, uint32_t first_vertex, uint32_t count);
    void (*cmd_draw_indexed)(yar_cmd_buffer* cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex);
    void (*cmd_dispatch)(yar_cmd_buffer* cmd, uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z);
    void (*cmd_update_buffer)(yar_cmd_buffer* cmd, yar_buffer* buffer, size_t offset, size_t size, void* data);
    void (*queue_submit)(yar_cmd_queue* queue);
    void (*queue_present)(yar_cmd_queue* queue, yar_queue_present_desc* desc);
};
