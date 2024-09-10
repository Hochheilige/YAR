#include <window.h>
#include <imgui_layer.h>
#include <render.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

struct CameraUniform
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
}camera;

auto main() -> int {
	init_window();
	
	init_render();

	SwapChain* swapchain;
	add_swapchain(true, &swapchain);

	float vertices[] = {
		// positions          // tex coords
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 

		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 

		-0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 

		 0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 
		 0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 

		 -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		  0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
		  0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

		 -0.5f,  0.5f, -0.5f,  0.0f, 0.0f,
		  0.5f,  0.5f, -0.5f,  1.0f, 0.0f,
		  0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		 -0.5f,  0.5f,  0.5f,  0.0f, 1.0f 
	};


	uint32_t indexes[] = {
		0, 1, 2,
		2, 3, 0,

		4, 5, 6,
		6, 7, 4,

		8, 9, 10,
		10, 11, 8,

		12, 13, 14,
		14, 15, 12,

		16, 17, 18,
		18, 19, 16,

		20, 21, 22,
		22, 23, 20
	};

	glm::vec3 cube_positions[] = {
		glm::vec3(0.0f,  0.0f,  0.0f),
		glm::vec3(2.0f,  5.0f, -15.0f),
		glm::vec3(-1.5f, -2.2f, -2.5f),
		glm::vec3(-3.8f, -2.0f, -12.3f),
		glm::vec3(2.4f, -0.4f, -3.5f),
		glm::vec3(-1.7f,  3.0f, -7.5f),
		glm::vec3(1.3f, -2.0f, -2.5f),
		glm::vec3(1.5f,  2.0f, -2.5f),
		glm::vec3(1.5f,  0.2f, -1.5f),
		glm::vec3(-1.3f,  1.0f, -1.5f)
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

	buffer_desc.size = sizeof(camera);
	buffer_desc.flags |= kBufferFlagDynamic;
	Buffer* camera_buf[image_count] = { nullptr, nullptr };
	for (auto& cam : camera_buf)
		add_buffer(&buffer_desc, &cam);

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

		update_desc.buffer = camera_buf[frame_index];
		update_desc.size = sizeof(camera);
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, &camera, sizeof(camera));
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
	layout.attrib_count = 2;
	layout.attribs[0].size = 3;
	layout.attribs[0].format = GL_FLOAT;
	layout.attribs[0].offset = 0;
	layout.attribs[1].size = 2;
	layout.attribs[1].format = GL_FLOAT;
	layout.attribs[1].offset = 3 * sizeof(float);

	DescriptorSetDesc set_desc;
	set_desc.max_sets = image_count;
	set_desc.update_freq = kUpdateFreqPerFrame;
	set_desc.shader = shader;
	DescriptorSet* set;
	add_descriptor_set(&set_desc, &set);

	DescriptorSet* cam;
	add_descriptor_set(&set_desc, &cam);

	UpdateDescriptorSetDesc update_set_desc;
	// Maybe it can be done using loop for all buffers
	update_set_desc.buffers = { ubos[0], ubos[1] };
	update_descriptor_set(&update_set_desc, set);

	update_set_desc = {};
	update_set_desc.buffers = { camera_buf[0], camera_buf[1] };
	update_descriptor_set(&update_set_desc, cam);

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

	glEnable(GL_DEPTH_TEST);

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
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		camera.view = glm::mat4(1.0f);
		camera.view = glm::translate(camera.view, glm::vec3(0.0f, 0.0f, -3.0f));
		camera.proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
		for (int i = 0; i < 10; ++i)
		{

			camera.model = glm::mat4(1.0f);
			//camera.model = glm::rotate(camera.model, (float)glfwGetTime() * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));
			camera.model = glm::translate(camera.model, cube_positions[i]);
			float angle = 20.0f * (i + 1);
			camera.model = glm::rotate(camera.model, (float)glfwGetTime() * glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));

			//{
			//	BufferUpdateDesc update;
			//	update.buffer = camera_buf[frame_index];
			//	update.size = sizeof(camera);
			//	resource_update_desc = &update;
			//	begin_update_resource(resource_update_desc);
			//	std::memcpy(update.mapped_data, &camera, sizeof(camera));
			//	end_update_resource(resource_update_desc);
			//}

			cmd_bind_pipeline(cmd, graphics_pipeline);
			cmd_bind_vertex_buffer(cmd, vbo, 0, sizeof(float) * 5);
			cmd_bind_index_buffer(cmd, ebo);
			cmd_bind_descriptor_set(cmd, set, frame_index);
			cmd_bind_descriptor_set(cmd, cam, frame_index);
			cmd_bind_descriptor_set(cmd, texture_set, 0);
			cmd_update_buffer(cmd, camera_buf[frame_index], sizeof(camera), &camera);
			cmd_draw_indexed(cmd, 36, 0, 0);
		}
		//cmd_draw(cmd, 0, 36);
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