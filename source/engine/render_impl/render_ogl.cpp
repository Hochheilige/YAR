#include "../render.h"
#include "../window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ======================================= //
//            Render Functions             //
// ======================================= //

void gl_addSwapChain(SwapChain* swapchain, bool vsync)
{
    swapchain->vsync = vsync;
    swapchain->window = get_window();
    swapchain->swap_buffers = get_swap_buffers_func();
}

// Maybe need to add some params to this function in future
bool gl_init_render()
{
    // probably need to add functions here
    add_swapchain = gl_addSwapChain;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

    return true;
}





