#include <window.h>
#include <imgui_layer.h>
#include <render.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>

auto main() -> int {
	init_window();
	
	init_render();

	SwapChain* swapchain;
	add_swapchain(true, &swapchain);

	float vertices[] = {
		// positions          // colors           // texture coords
		 0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // top right
		 0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // bottom right
		-0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // bottom left
		-0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f    // top left 
	};

	uint32_t indexes[] =
	{
		0, 1, 2,
		2, 3, 0
	};

	float rgb[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	int32_t width, height, channels;
	std::string name = "assets/hp-logo.png";
	stbi_set_flip_vertically_on_load(true);
	uint8_t* image_data = stbi_load(name.c_str(), &width, &height, &channels, 0);
	Texture* texture;
	if (image_data)
	{
		TextureDesc texture_desc{};
		texture_desc.width = width;
		texture_desc.height = height;
		texture_desc.mip_levels = 1;
		texture_desc.type = kTextureType2D;
		texture_desc.format = kTextureFormatRGBA8;
		add_texture(&texture_desc, &texture);
	}
	else
	{
		std::cerr << "Failed to load texture: " << name << std::endl;
	}
	//stbi_image_free(image_data);

	Sampler* sampler;
	SamplerDesc sampler_desc{};
	sampler_desc.min_filter = kFilterTypeNearest;
	sampler_desc.mag_filter = kFilterTypeNearest;
	sampler_desc.wrap_u = kWrapModeClampToEdge;
	sampler_desc.wrap_v = kWrapModeClampToEdge;
	add_sampler(&sampler_desc, &sampler);

	BufferDesc buffer_desc;
	buffer_desc.size = sizeof(vertices);
	buffer_desc.flags = kBufferFlagGPUOnly;
	Buffer* vbo = nullptr;
	add_buffer(&buffer_desc, &vbo);

	buffer_desc.size = sizeof(indexes);
	Buffer* ebo = nullptr;
	add_buffer(&buffer_desc, &ebo);

	constexpr uint32_t image_count = 2;
	uint32_t frame_index = 0;
	buffer_desc.size = sizeof(rgb);
	buffer_desc.flags = kBufferFlagMapWrite;
	Buffer* ubos[image_count] = { nullptr, nullptr };
	for (auto& ubo : ubos)
		add_buffer(&buffer_desc, &ubo);

	// Need to make something with shaders path 
	ShaderLoadDesc shader_load_desc = {};
	shader_load_desc.stages[0] = { "shaders/base_vert.hlsl", "main", ShaderStage::kShaderStageVert };
	shader_load_desc.stages[1] = { "shaders/base_frag.hlsl", "main", ShaderStage::kShaderStageFrag };
	ShaderDesc* shader_desc = nullptr;
	load_shader(&shader_load_desc, &shader_desc);
	Shader* shader;
	add_shader(shader_desc, &shader);

	ResourceUpdateDesc resource_update_desc;
	{ // update buffers data
		BufferUpdateDesc update_desc{};
		resource_update_desc = &update_desc;
		update_desc.buffer = vbo;
		update_desc.size = sizeof(vertices);
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, vertices, sizeof(vertices));
		end_update_resource(resource_update_desc);

		update_desc.buffer = ebo;
		update_desc.size = sizeof(indexes);
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, indexes, sizeof(indexes));
		end_update_resource(resource_update_desc);

		update_desc.buffer = ubos[frame_index];
		update_desc.size = sizeof(rgb);
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, rgb, sizeof(rgb));
		end_update_resource(resource_update_desc);

		TextureUpdateDesc tex_update_desc{};
		resource_update_desc = &tex_update_desc;
		tex_update_desc.size = width * height * channels;
		tex_update_desc.texture = texture;
		tex_update_desc.data = image_data;
		begin_update_resource(resource_update_desc);
		std::memcpy(tex_update_desc.mapped_data, image_data, tex_update_desc.size);
		end_update_resource(resource_update_desc);
	}

	VertexLayout layout = {0};
	layout.attrib_count = 3;
	layout.attribs[0].size = 3;
	layout.attribs[0].format = GL_FLOAT;
	layout.attribs[0].offset = 0;
	layout.attribs[1].size = 3;
	layout.attribs[1].format = GL_FLOAT;
	layout.attribs[1].offset = 3 * sizeof(float);
	layout.attribs[2].size = 2;
	layout.attribs[2].format = GL_FLOAT;
	layout.attribs[2].offset = 6 * sizeof(float);

	DescriptorSetDesc set_desc;
	set_desc.max_sets = image_count;
	set_desc.update_freq = kUpdateFreqPerFrame;
	set_desc.shader = shader;
	DescriptorSet* set;
	add_descriptor_set(&set_desc, &set);

	UpdateDescriptorSetDesc update_set_desc;
	// Maybe it can be done using loop for all buffers
	update_set_desc.buffers = { ubos[0], ubos[1] };
	update_descriptor_set(&update_set_desc, set);

	set_desc.max_sets = 1;
	set_desc.update_freq = kUpdateFreqNone;
	DescriptorSet* texture_set;
	add_descriptor_set(&set_desc, &texture_set);
	update_set_desc = {};
	update_set_desc.textures = { texture };
	update_set_desc.samplers = { sampler };
	update_descriptor_set(&update_set_desc, texture_set);

	PipelineDesc pipeline_desc = { 0 };
	pipeline_desc.shader = shader;
	pipeline_desc.vertex_layout = &layout;

	Pipeline* graphics_pipeline;
	add_pipeline(&pipeline_desc, &graphics_pipeline);

	CmdQueueDesc queue_desc;
	CmdQueue* queue;
	add_queue(&queue_desc, &queue);

	CmdBufferDesc cmd_desc;
	cmd_desc.current_queue = queue;
	CmdBuffer* cmd;
	add_cmd(&cmd_desc, &cmd);

	uint32_t rgb_index = 0;

	while(update_window())
	{
		rgb[rgb_index] = sin(glfwGetTime()) * 0.5f + 0.5f;
		{ // update buffer data
			BufferUpdateDesc update;
			update.buffer = ubos[frame_index];
			update.size = sizeof(rgb);
			resource_update_desc = &update;
			begin_update_resource(resource_update_desc);
			std::memcpy(update.mapped_data, rgb, sizeof(rgb));
			end_update_resource(resource_update_desc);
		}


        imgui_begin_frame();

        glClearColor(gBackGroundColor[0], gBackGroundColor[1], gBackGroundColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

		cmd_bind_pipeline(cmd, graphics_pipeline);
		cmd_bind_vertex_buffer(cmd, vbo, 0, sizeof(float) * 8);
		cmd_bind_index_buffer(cmd, ebo);
		cmd_bind_descriptor_set(cmd, set, frame_index);
		cmd_bind_descriptor_set(cmd, texture_set, 0);
		cmd_draw_indexed(cmd, 6, 0, 0);
		//cmd_draw(cmd, 0, 3);
		queue_submit(queue);

        imgui_render();
        imgui_end_frame();

        swapchain->swap_buffers(swapchain->window);
        glfwPollEvents();
		
		frame_index = (frame_index + 1) % image_count;
		rgb_index = (rgb_index + 1) % 3;
	}

	terminate_window();
}