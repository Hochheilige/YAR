#include <window.h>
#include <imgui_layer.h>
#include <render.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

auto main() -> int {
	init_window();
	
	init_render();

	SwapChain* swapchain;
	add_swapchain(true, &swapchain);

	// add just vertices for Triangle
	float vertices[] = {
    	-0.5f, -0.5f, 0.0f,
    	 0.5f, -0.5f, 0.0f,
    	 0.0f,  0.5f, 0.0f
	}; 
	BufferDesc buffer_desc;
	buffer_desc.size = sizeof(vertices);
	buffer_desc.memory_usage = GL_STATIC_DRAW;
	buffer_desc.target = GL_ARRAY_BUFFER;
	Buffer* vbo = nullptr;
	add_buffer(&buffer_desc, &vbo);
	{ // update buffer data
		void* buffer = map_buffer(vbo);
		std::memcpy(buffer, vertices, sizeof(vertices));
		unmap_buffer(vbo);
	}

	while(update_window())
	{
        imgui_begin_frame();

        glClearColor(gBackGroundColor[0], gBackGroundColor[1], gBackGroundColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        imgui_render();
        imgui_end_frame();

        swapchain->swap_buffers(swapchain->window);
        glfwPollEvents();
    
	}

	terminate_window();
}