#include <window.h>
#include <imgui_layer.h>
#include <render.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

auto main() -> int {
	init_window();
	
	init_render();

	SwapChain swapchain;
	add_swapchain(&swapchain, true);

	while(update_window())
	{
        imgui_begin_frame();

        glClearColor(gBackGroundColor[0], gBackGroundColor[1], gBackGroundColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        imgui_render();
        imgui_end_frame();

        swapchain.swap_buffers(swapchain.window);
        glfwPollEvents();
    
	}

	terminate_window();
}