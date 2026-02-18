#pragma once
#include <cstdint>
#include <functional>

struct WindowDimensions
{
    uint32_t width;
    uint32_t height;
};

bool init_window(const std::function<void()>& imgui_layer = nullptr);
bool update_window();
void terminate_window();

typedef void (*swap_buffers)(void*);
swap_buffers get_swap_buffers_func();

typedef void (*swap_interval)(bool);
swap_interval get_swap_interval_func();

void* get_window();
const WindowDimensions& get_window_dims();

using mouse_move_callback = void(*)(double xpos, double ypos);
using scroll_wheel_callback = void(*)(double xoffset, double yoffset);
void register_mouse_callback(mouse_move_callback cb);
void register_scroll_callback(scroll_wheel_callback cb);
