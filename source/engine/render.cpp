#include "render.h"

extern bool gl_init_render();

// ======================================= //
//            Load Functions               //
// ======================================= //

load_shader_fn load_shader;

// ======================================= //
//            Render Functions             //
// ======================================= //

add_swapchain_fn add_swapchain;
add_buffer_fn    add_buffer;
add_shader_fn    add_shader;
add_pipeline_fn  add_pipeline;
remove_buffer_fn remove_buffer;
map_buffer_fn    map_buffer;
unmap_buffer_fn  unmap_buffer;
cmd_bind_pipeline_fn cmd_bind_pipeline;
cmd_bind_vertex_buffer_fn cmd_bind_vertex_buffer;

void init_render()
{
    gl_init_render();
}