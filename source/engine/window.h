#pragma once
#include <cstdint>
struct WindowDimensions
{
    uint32_t width;
    uint32_t height;
};

bool init_window();
bool update_window();
void terminate_window();

typedef void (*swap_buffers)(void*);
swap_buffers get_swap_buffers_func();
void* get_window();
const WindowDimensions& get_window_dims();