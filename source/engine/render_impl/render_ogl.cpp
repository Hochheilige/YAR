#include "../render.h"
#include "../window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <memory>

// ======================================= //
//            Render Functions             //
// ======================================= //

void gl_addSwapChain(SwapChain* swapchain, bool vsync)
{
    swapchain->vsync = vsync;
    swapchain->window = get_window();
    swapchain->swap_buffers = get_swap_buffers_func();
}

void gl_addBuffer(Buffer* buffer, BufferDesc* desc)
{
    buffer = (Buffer*)std::malloc(sizeof(Buffer));
    buffer->target = desc->target;

    glGenBuffers(1, &buffer->id);
    glBindBuffer(buffer->target, buffer->id);
    glBufferData(buffer->target, desc->size, nullptr, desc->memory_usage);
    glBindBuffer(buffer->target, GL_NONE);
}

void gl_removeBuffer(Buffer* buffer)
{
    if (buffer)
    {
        glDeleteBuffers(1, &buffer->id);
        free(buffer);
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





