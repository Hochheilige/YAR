#include "render.h"

extern bool gl_init_render();

// ======================================= //
//            Load Functions               //
// ======================================= //

load_shader_fn load_shader;

// ======================================= //
//            Render Functions             //
// ======================================= //

add_swapchain_fn          add_swapchain;
add_buffer_fn             add_buffer;
add_shader_fn             add_shader;
add_pipeline_fn           add_pipeline;
add_queue_fn              add_queue;
add_cmd_fn                add_cmd;
remove_buffer_fn          remove_buffer;
map_buffer_fn             map_buffer;
unmap_buffer_fn           unmap_buffer;
cmd_bind_pipeline_fn      cmd_bind_pipeline;
cmd_bind_vertex_buffer_fn cmd_bind_vertex_buffer;
cmd_bind_index_buffer_fn  cmd_bind_index_buffer;
cmd_draw_fn               cmd_draw;
cmd_draw_indexed_fn       cmd_draw_indexed;
queue_submit_fn           queue_submit;

void init_render()
{
    gl_init_render();
}