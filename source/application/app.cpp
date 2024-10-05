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

struct Camera
{
	glm::vec3 pos = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
} camera;

struct Material
{
	float shinines;
	float pad[3];
} material[10];

static constexpr uint32_t kDirLightCount = 1;
static constexpr uint32_t kPointLightCount = 1;
static constexpr uint32_t kSpotLightCount = 1;
static constexpr uint64_t kLightSourcesCount = kDirLightCount + kPointLightCount;// +kSpotLightCount;

struct DirectLight
{
	glm::vec4 direction[kDirLightCount];
};

struct PointLight
{
	glm::vec4 position[kPointLightCount];
	glm::vec4 attenuation[kPointLightCount];
};

struct SpotLight
{
	glm::vec4 position[kSpotLightCount];
	glm::vec4 direction[kSpotLightCount];
	// not cool to use vec4 here
	glm::vec4 cutoff[kSpotLightCount];
};

struct LightParams
{
	glm::vec4 ambient[kLightSourcesCount];
	glm::vec4 diffuse[kLightSourcesCount];
	glm::vec4 specular[kLightSourcesCount];
};

struct UBO
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 model[10];
	DirectLight dir_light;
	PointLight point_light;
	//SpotLight spot_light;
	LightParams light_params;
	glm::vec4 view_pos;
}ubo;

bool firstMouse = true;
float yaw = -90.0f;	
float pitch = 0.0f;
float lastX = 1920.0f / 2.0;
float lastY = 1080.0 / 2.0;
float fov = 45.0f;

float deltaTime = 0.0f;	
float lastFrame = 0.0f;

glm::vec4* light_pos = nullptr;

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
		   // positions    // tex coords    // normals
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 0.0f, -1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f, 0.0f, -1.0f,

		-0.5f, -0.5f, -0.5f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,

		 0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f, -1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, -1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 0.0f, -1.0f, 0.0f,

		-0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
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

	glm::vec4 cube_positions[] = {
		glm::vec4(0.0f,  0.0f,  0.0f  , 1.0f),
		glm::vec4(2.0f,  5.0f, -15.0f , 1.0f),
		glm::vec4(-1.5f, -2.2f, -2.5f , 1.0f),
		glm::vec4(-3.8f, -2.0f, -12.3f, 1.0f),
		glm::vec4(2.4f, -0.4f, -3.5f  , 1.0f),
		glm::vec4(-1.7f,  3.0f, -7.5f , 1.0f),
		glm::vec4(1.3f, -2.0f, -2.5f  , 1.0f),
		glm::vec4(1.5f,  2.0f, -2.5f  , 1.0f),
		glm::vec4(1.5f,  0.2f, -1.5f  , 1.0f),
		glm::vec4(-1.3f,  1.0f, -1.5f , 1.0f)
	};


	light_pos = &cube_positions[0];

	// Point Light
	memset(&ubo, 0x00, sizeof(ubo));

	// Dir Light
	ubo.dir_light.direction[0] = glm::vec4(0.0f, 0.0f, 3.0f, 0.0f);
	ubo.light_params.ambient[0] = glm::vec4(0.0f, 0.2f, 0.0f, 0.0f);
	ubo.light_params.diffuse[0] = glm::vec4(0.0f, 0.4f, 0.0f, 0.0f);
	ubo.light_params.specular[0] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

	// Point Light
	ubo.point_light.position[0] = *light_pos;
	ubo.point_light.attenuation[0] = glm::vec4(1.0f, 0.007f, 0.0002f, 0.0f);
	ubo.light_params.ambient[1] = glm::vec4(0.2f, 0.0f, 0.0f, 0.0f);
	ubo.light_params.diffuse[1] = glm::vec4(0.5f, 0.0f, 0.0f, 0.0f);
	ubo.light_params.specular[1] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

	// Spotlight

	Texture* diffuse_map_tex;
	Texture* specular_map_tex;

	int32_t width, height, channels;
	std::string name = "assets/container2.png";
	stbi_set_flip_vertically_on_load(true);
	uint8_t* diffuse_map = stbi_load(name.c_str(), &width, &height, &channels, 0);

	if (diffuse_map)
	{
		TextureDesc texture_desc{};
		texture_desc.width = width;
		texture_desc.height = height;
		texture_desc.mip_levels = 1;
		texture_desc.type = kTextureType2D;
		texture_desc.format = kTextureFormatRGBA8;
		add_texture(&texture_desc, &diffuse_map_tex);

		ResourceUpdateDesc resource_update_desc;
		TextureUpdateDesc tex_update_desc{};
		resource_update_desc = &tex_update_desc;
		tex_update_desc.size = width * height * channels;
		tex_update_desc.texture = diffuse_map_tex;
		tex_update_desc.data = diffuse_map;
		begin_update_resource(resource_update_desc);
		std::memcpy(tex_update_desc.mapped_data, diffuse_map, tex_update_desc.size);
		end_update_resource(resource_update_desc);
	}
	else
	{
		std::cerr << "Failed to load diffuse_map_tex: " << name << std::endl;
	}

	name = "assets/container2_specular.png";
	uint8_t* specular_map = stbi_load(name.c_str(), &width, &height, &channels, 0);
	if (specular_map)
	{
		TextureDesc texture_desc{};
		texture_desc.width = width;
		texture_desc.height = height;
		texture_desc.mip_levels = 1;
		texture_desc.type = kTextureType2D;
		texture_desc.format = kTextureFormatRGBA8;
		add_texture(&texture_desc, &specular_map_tex);

		ResourceUpdateDesc resource_update_desc;
		TextureUpdateDesc tex_update_desc{};
		resource_update_desc = &tex_update_desc;
		tex_update_desc.size = width * height * channels;
		tex_update_desc.texture = specular_map_tex;
		tex_update_desc.data = specular_map;
		begin_update_resource(resource_update_desc);
		std::memcpy(tex_update_desc.mapped_data, specular_map, tex_update_desc.size);
		end_update_resource(resource_update_desc);
	}
	else
	{
		std::cerr << "Failed to load specular_map_tex: " << name << std::endl;
	}

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

	buffer_desc.size = sizeof(ubo);
	buffer_desc.flags = kBufferFlagMapWrite;
	Buffer* ubo_buf[image_count] = { nullptr, nullptr };
	for (auto& buf : ubo_buf)
		add_buffer(&buffer_desc, &buf);

	buffer_desc.size = sizeof(material);
	Buffer* mat_buf;
	add_buffer(&buffer_desc, &mat_buf);

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

		// emerald
		material[0].shinines = 128u * 0.6f;
		
		// emerald
		material[1].shinines = 128u * 0.6f;;
		
		// green plastic
		material[2].shinines = 128u * 0.25f;

		// red rubber
		material[3].shinines = 128u * 0.078125f;

		// silver
		material[4].shinines = 128u * 0.4f;

		// gold 
		material[5].shinines = 128u * 0.4f;

		// ruby
		material[6].shinines = 128u * 0.6f;

		// pearl
		material[7].shinines = 128u * 0.088;

		// jade 
		material[8].shinines = 128u * 0.1f;

		// turquoise
		material[9].shinines = 128u * 0.1f;
		update_desc.buffer = mat_buf;
		update_desc.size = sizeof(material);
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, &material, sizeof(material));
		end_update_resource(resource_update_desc);		
	}

	VertexLayout layout = {0};
	layout.attrib_count = 3;
	layout.attribs[0].size = 3;
	layout.attribs[0].format = GL_FLOAT;
	layout.attribs[0].offset = 0;
	layout.attribs[1].size = 2;
	layout.attribs[1].format = GL_FLOAT;
	layout.attribs[1].offset = 3 * sizeof(float);
	layout.attribs[2].size = 3;
	layout.attribs[2].format = GL_FLOAT;
	layout.attribs[2].offset = 5 * sizeof(float);


	DescriptorSetDesc set_desc;
	set_desc.max_sets = image_count;
	set_desc.shader = shader;
	set_desc.update_freq = kUpdateFreqPerFrame;
	DescriptorSet* ubo_desc;
	add_descriptor_set(&set_desc, &ubo_desc);

	UpdateDescriptorSetDesc update_set_desc{};
	for (uint32_t i = 0; i < image_count; ++i)
	{
		std::vector<DescriptorInfo> infos{
			{
				.name = "ubo",
				.descriptor = ubo_buf[i]
			}
		};
		update_set_desc.index = i;
		update_set_desc.infos = std::move(infos);
		update_descriptor_set(&update_set_desc, ubo_desc);
	}

	set_desc.max_sets = 1;
	set_desc.update_freq = kUpdateFreqNone;
	DescriptorSet* texture_set;
	add_descriptor_set(&set_desc, &texture_set);

	std::vector<DescriptorInfo> infos{
		{
			.name = "diffuse_map",
			.descriptor =
			DescriptorInfo::CombinedTextureSample{
				diffuse_map_tex,
				"samplerState",
				sampler
			}
		},
		{
			.name = "specular_map",
			.descriptor =
			DescriptorInfo::CombinedTextureSample{
				specular_map_tex,
				"samplerState",
				sampler
			}
		},
		{
			.name = "mat",
			.descriptor = mat_buf
		}
	};


	update_set_desc = {};
	update_set_desc.index = 0;
	update_set_desc.infos = std::move(infos);
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
	pc_desc.size = sizeof(uint32_t) * 4;
	CmdBufferDesc cmd_desc;
	cmd_desc.current_queue = queue;
	cmd_desc.use_push_constant = true;
	cmd_desc.pc_desc = &pc_desc;
	CmdBuffer* cmd;
	add_cmd(&cmd_desc, &cmd);

	glm::mat4 model{};
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	while(update_window())
	{
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		process_input((GLFWwindow*)get_window());

		imgui_begin_frame();

		glClearColor(gBackGroundColor[0], gBackGroundColor[1], gBackGroundColor[2], 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ubo.point_light.position[0] = *light_pos; // update point light pos
		ubo.view_pos = glm::vec4(camera.pos, 0.0f);
		ubo.view = glm::mat4(1.0f);
		ubo.view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);
		ubo.proj = glm::perspective(glm::radians(fov), 1920.0f / 1080.0f, 0.1f, 100.0f);

		for (uint32_t i = 0; i < 10; ++i)
		{
			model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(cube_positions[i]));
			if (i != 0) // i == 0 light position
			{
				float angle = 20.0f;// *(i + 1);
				model = glm::rotate(model, (float)glfwGetTime() * glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
			}
			else
			{
				model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
			}
			ubo.model[i] = model;
		}

		BufferUpdateDesc update;
		update.buffer = ubo_buf[frame_index];
		update.size = sizeof(ubo);
		resource_update_desc = &update;
		begin_update_resource(resource_update_desc);
		std::memcpy(update.mapped_data, &ubo, sizeof(ubo));
		end_update_resource(resource_update_desc);

		for (uint32_t i = 0; i < 10; ++i)
		{
			cmd_bind_pipeline(cmd, graphics_pipeline);
			// looks not good
			cmd_bind_vertex_buffer(cmd, vbo, layout.attrib_count, 0, sizeof(float) * 8);
			cmd_bind_index_buffer(cmd, ebo);
			cmd_bind_descriptor_set(cmd, ubo_desc, frame_index);
			cmd_bind_descriptor_set(cmd, texture_set, 0);
			cmd_bind_push_constant(cmd, &i);
			cmd_draw_indexed(cmd, 36, 0, 0);
		}
		
		queue_submit(queue);

        imgui_render();
        imgui_end_frame();

        swapchain->swap_buffers(swapchain->window);
        glfwPollEvents();
		
		frame_index = (frame_index + 1) % image_count;
	}

	terminate_window();
}

void process_input(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	float speed = static_cast<float>(2.5 * deltaTime);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.pos += speed * camera.front;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.pos -= speed * camera.front;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.pos -= glm::normalize(glm::cross(camera.front, camera.up)) * speed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.pos += glm::normalize(glm::cross(camera.front, camera.up)) * speed;
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		camera.pos += camera.up * speed;
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		camera.pos -= camera.up * speed;


	// Thing to move light source
	if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) != GLFW_PRESS)
	{
		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
			light_pos->y += speed;
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
			light_pos->y -= speed;
	}
	else
	{
		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
			light_pos->z -= speed;
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
			light_pos->z += speed;
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		light_pos->x -= speed;
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		light_pos->x += speed;
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
		glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS)
	{
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
