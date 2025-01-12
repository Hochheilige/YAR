#include "render.h"

extern bool gl_init_render();

// ======================================= //
//            Load Functions               //
// ======================================= //

load_shader_fn           load_shader;

// this 2 functions should be the same for all 
// graphics API
begin_update_resource_fn begin_update_resource;
end_update_resource_fn   end_update_resource;


map_buffer_fn              map_buffer;
unmap_buffer_fn            unmap_buffer;

// ======================================= //
//            Render Functions             //
// ======================================= //

add_swapchain_fn           add_swapchain;
add_buffer_fn              add_buffer;
add_texture_fn             add_texture;
add_sampler_fn             add_sampler;
add_shader_fn              add_shader;
add_descriptor_set_fn      add_descriptor_set;
//add_root_signature_fn     add_root_signature;
add_pipeline_fn            add_pipeline;
add_queue_fn               add_queue;
add_cmd_fn                 add_cmd;
remove_buffer_fn           remove_buffer;
update_descriptor_set_fn   update_descriptor_set;
cmd_bind_pipeline_fn       cmd_bind_pipeline;
cmd_bind_descriptor_set_fn cmd_bind_descriptor_set;
cmd_bind_vertex_buffer_fn  cmd_bind_vertex_buffer;
cmd_bind_index_buffer_fn   cmd_bind_index_buffer;
cmd_bind_push_constant_fn  cmd_bind_push_constant;
cmd_draw_fn                cmd_draw;
cmd_draw_indexed_fn        cmd_draw_indexed;
cmd_dispatch_fn            cmd_dispatch;
cmd_update_buffer_fn       cmd_update_buffer;
queue_submit_fn            queue_submit;

void init_render()
{
    gl_init_render();
}