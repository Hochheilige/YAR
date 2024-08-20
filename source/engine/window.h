#pragma once
bool init_window();
bool update_window();
void terminate_window();

typedef void (*swap_buffers)(void*);
swap_buffers get_swap_buffers_func();
void* get_window();