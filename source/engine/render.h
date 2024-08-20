#pragma once

// ======================================= //
//            Render Structures            //
// ======================================= //

// Maybe sturcts should be in basic file
struct SwapChain
{
    bool vsync;
    void* window;
    void(*swap_buffers)(void*);
};

// ======================================= //
//            Render Functions             //
// ======================================= //

typedef void (*add_swapchain_fn)(SwapChain* swapchain, bool vsync);
extern add_swapchain_fn add_swapchain;

void init_render();