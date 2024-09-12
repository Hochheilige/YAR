#include <window.h>
#include <imgui.h>
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

struct MVP
{
	glm::mat4 model[10];
	glm::mat4 view;
	glm::mat4 proj;
}mvp;

struct Camera
{
	glm::vec3 pos = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
} camera;

bool firstMouse = true;
float yaw = -90.0f;	
float pitch = 0.0f;
float lastX = 800.0f / 2.0;
float lastY = 600.0 / 2.0;
float fov = 45.0f;

float deltaTime = 0.0f;	
float lastFrame = 0.0f;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void process_input(GLFWwindow* window);

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

	buffer_desc.size = sizeof(mvp);
	buffer_desc.flags |= kBufferFlagDynamic;
	Buffer* mvp_buf[image_count] = { nullptr, nullptr };
	for (auto& cam : mvp_buf)
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

		update_desc.buffer = mvp_buf[frame_index];
		update_desc.size = sizeof(mvp);
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, &mvp, sizeof(mvp));
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
	update_set_desc.buffers = { mvp_buf[0], mvp_buf[1] };
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

	PushConstantDesc pc_desc{};
	pc_desc.name = "push_constant";
	pc_desc.shader = shader;
	pc_desc.size = sizeof(uint32_t);
	CmdBufferDesc cmd_desc;
	cmd_desc.current_queue = queue;
	cmd_desc.use_push_constant = true;
	cmd_desc.pc_desc = &pc_desc;
	CmdBuffer* cmd;
	add_cmd(&cmd_desc, &cmd);

	uint32_t rgb_index = 0;


	glEnable(GL_DEPTH_TEST);

	while(update_window())
	{
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		process_input((GLFWwindow*)get_window());


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

		mvp.view = glm::mat4(1.0f);
		mvp.view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);
		mvp.proj = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 0.1f, 100.0f);

		BufferUpdateDesc update;
		update.buffer = mvp_buf[frame_index];
		update.size = sizeof(mvp);
		resource_update_desc = &update;
		begin_update_resource(resource_update_desc);

		for (uint32_t i = 0; i < 10; ++i)
		{
			mvp.model[i] = glm::mat4(1.0f);
			mvp.model[i] = glm::translate(mvp.model[i], cube_positions[i]);
			float angle = 20.0f * (i + 1);
			mvp.model[i] = glm::rotate(mvp.model[i], (float)glfwGetTime() * glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
			std::memcpy(update.mapped_data, &mvp, sizeof(mvp));

			cmd_bind_pipeline(cmd, graphics_pipeline);
			cmd_bind_vertex_buffer(cmd, vbo, 0, sizeof(float) * 5);
			cmd_bind_index_buffer(cmd, ebo);
			cmd_bind_descriptor_set(cmd, set, frame_index);
			cmd_bind_descriptor_set(cmd, cam, frame_index);
			cmd_bind_descriptor_set(cmd, texture_set, 0);
			cmd_bind_push_constant(cmd, &i);
			cmd_draw_indexed(cmd, 36, 0, 0);
		}
		end_update_resource(resource_update_desc);
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

void process_input(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	float cameraSpeed = static_cast<float>(2.5 * deltaTime);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.pos += cameraSpeed * camera.front;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.pos -= cameraSpeed * camera.front;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.pos -= glm::normalize(glm::cross(camera.front, camera.up)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.pos += glm::normalize(glm::cross(camera.front, camera.up)) * cameraSpeed;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
	if (ImGui::GetIO().WantCaptureMouse || 
		glfwGetKey(window, GLFW_KEY_LEFT_ALT) != GLFW_PRESS) {
		return;
	}

	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; 
	lastX = xpos;
	lastY = ypos;

	float sensitivity = 0.1f; 
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	camera.front = glm::normalize(front);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	fov -= (float)yoffset;
	if (fov < 1.0f)
		fov = 1.0f;
	if (fov > 45.0f)
		fov = 45.0f;
}
