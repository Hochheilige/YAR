#include "../render.h"
#include "../window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <memory>

// ======================================= //
//            Render Functions             //
// ======================================= //

void gl_addSwapChain(bool vsync, SwapChain** swapchain)
{
    SwapChain* new_swapchain = (SwapChain*)std::malloc(sizeof(SwapChain)); 
    if (new_swapchain == nullptr) 
        // Log error maybe add assert or something
        return;

    new_swapchain->vsync = vsync;
    new_swapchain->window = get_window();
    new_swapchain->swap_buffers = get_swap_buffers_func();

    *swapchain = new_swapchain;
}

void gl_addBuffer(BufferDesc* desc, Buffer** buffer)
{
    Buffer* new_buffer = (Buffer*)std::malloc(sizeof(Buffer));
    if (new_buffer == nullptr)
        // Log error
        return;
    
    new_buffer->target = desc->target;

    glGenBuffers(1, &new_buffer->id);
    glBindBuffer(new_buffer->target, new_buffer->id);
    glBufferData(new_buffer->target, desc->size, nullptr, desc->memory_usage);
    glBindBuffer(new_buffer->target, GL_NONE);

    *buffer = new_buffer;
}

void gl_removeBuffer(Buffer* buffer)
{
    if (buffer)
    {
        glDeleteBuffers(1, &buffer->id);
        std::free(buffer);
    }
}

void gl_mapBuffer(Buffer* buffer)
{
    glBindBuffer(buffer->target, buffer->id);
    glMapBuffer(buffer->id, GL_WRITE_ONLY);
}

void gl_unmapBuffer(Buffer* buffer)
{
    glUnmapBuffer(buffer->target);
}

// Maybe need to add some params to this function in future
bool gl_init_render()
{
    // probably need to add functions here
    add_swapchain = gl_addSwapChain;
    add_buffer    = gl_addBuffer;
    map_buffer    = gl_mapBuffer;
    unmap_buffer  = gl_unmapBuffer;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

    return true;
}





