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
	buffer_desc.flags = BufferFlag::kBufferFlagMapWrite;
	Buffer* vbo = nullptr;
	add_buffer(&buffer_desc, &vbo);

	ShaderLoadDesc shader_load_desc = {};
	shader_load_desc.stages[0] = { "../source/shaders/base_vert.hlsl", "main", ShaderStage::kShaderStageVert };
	shader_load_desc.stages[1] = { "../source/shaders/base_frag.hlsl", "main", ShaderStage::kShaderStageFrag };
	ShaderDesc* shader_desc = nullptr;
	load_shader(&shader_load_desc, &shader_desc);
	Shader* shader;
	add_shader(shader_desc, &shader);

	{ // update buffer data
		void* buffer = map_buffer(vbo);
		size_t err = glGetError();
		std::memcpy(buffer, vertices, sizeof(vertices));
		unmap_buffer(vbo);
	}

	VertexLayout layout = {0};
	layout.attrib_count = 1;
	layout.attribs[0].size = 3;
	layout.attribs[0].format = GL_FLOAT;
	layout.attribs[0].offset = 0;

	PipelineDesc pipeline_desc = { 0 };
	pipeline_desc.shader = shader;
	pipeline_desc.vertex_layout = &layout;

	Pipeline* graphics_pipeline;
	add_pipeline(&pipeline_desc, &graphics_pipeline);

	while(update_window())
	{
        imgui_begin_frame();

        glClearColor(gBackGroundColor[0], gBackGroundColor[1], gBackGroundColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(shader->program);
		cmd_bind_pipeline(graphics_pipeline);
		cmd_bind_vertex_buffer(vbo, 0, sizeof(float) * 3);
		glDrawArrays(GL_TRIANGLES, 0, 3);

        imgui_render();
        imgui_end_frame();

        swapchain->swap_buffers(swapchain->window);
        glfwPollEvents();
    
	}

	terminate_window();
}