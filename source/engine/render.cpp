#include "render.h"

extern bool gl_init_render();

add_swapchain_fn add_swapchain;
add_buffer_fn    add_buffer;
remove_buffer_fn remove_buffer;
map_buffer_fn    map_buffer;
unmap_buffer_fn  unmap_buffer;

void init_render()
{
    gl_init_render();
}