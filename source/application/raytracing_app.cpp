#include <window.h>
#include <imgui.h>
#include <imgui_layer.h>
#include <render.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <random>

inline uint32_t random_uint() 
{
	static std::uniform_int_distribution<uint32_t> distribution(0, UINT32_MAX);
	static std::mt19937 generator(std::random_device{}());
	return distribution(generator);
}

void process_input(GLFWwindow* window);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct Sphere
{
	glm::vec3 center;
	float radius;
};

constexpr uint32_t kSpheresCount = 2u;

struct UBO
{
	glm::mat4 invViewProj;
	glm::vec4 cameraPos;
	Sphere spheres[kSpheresCount];
	int32_t samples_per_pixel;
	int32_t max_ray_depth;
	uint32_t seed[2];
}ubo;

struct Camera
{
	glm::vec3 pos = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
} camera;

bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 1920.0f / 2.0;
float lastY = 1080.0 / 2.0;
float fov = 45.0f;

Texture* create_texture(const uint32_t width, const uint32_t height)
{
	Texture* tex;
	TextureDesc texture_desc{};
	texture_desc.width = width;
	texture_desc.height = height;
	texture_desc.mip_levels = 1;
	texture_desc.type = kTextureType2D;
	texture_desc.format = kTextureFormatRGBA32F;
	add_texture(&texture_desc, &tex);

	return tex;
}

auto main() -> int
{
	int32_t samples_per_pixel = 1u;
	int32_t max_ray_depth = 1u;
	
	const auto imgui_layer = [&]()
		{
			ImGui::Begin("Raytracer settings");
			ImGui::SliderInt("Samples Per Pixel", &samples_per_pixel, 1, 100);
			ImGui::SliderInt("Max Ray Depth", &max_ray_depth, 1, 100);
			ImGui::End();
		};


	init_window(imgui_layer);
	init_render();

	SwapChain* swapchain;
	add_swapchain(true, &swapchain);

	auto dims = get_window_dims();
	Texture* quad = create_texture(dims.width, dims.height);

	float quad_vertices[] = {
		-1.0f, -1.0f,   0.0f, 0.0f,
		 1.0f, -1.0f,   1.0f, 0.0f,
		-1.0f,  1.0f,   0.0f, 1.0f,
		 1.0f,  1.0f,   1.0f, 1.0f
	};


	Sampler* sampler;
	SamplerDesc sampler_desc{};
	sampler_desc.min_filter = kFilterTypeNearest;
	sampler_desc.mag_filter = kFilterTypeNearest;
	sampler_desc.wrap_u = kWrapModeRepeat;
	sampler_desc.wrap_v = kWrapModeRepeat;
	add_sampler(&sampler_desc, &sampler);

	BufferDesc buffer_desc;
	buffer_desc.size = sizeof(quad_vertices);
	buffer_desc.flags = kBufferFlagGPUOnly;
	Buffer* vbo = nullptr;
	add_buffer(&buffer_desc, &vbo);

	constexpr uint32_t image_count = 2;
	uint32_t frame_index = 0;

	buffer_desc.size = sizeof(ubo);
	buffer_desc.flags = kBufferFlagMapWrite;
	Buffer* ubo_buf[image_count] = { nullptr, nullptr };
	for (auto& buf : ubo_buf)
		add_buffer(&buffer_desc, &buf);

	ResourceUpdateDesc resource_update_desc;
	{ // update buffers data
		BufferUpdateDesc update_desc{};
		resource_update_desc = &update_desc;
		update_desc.buffer = vbo;
		update_desc.size = sizeof(quad_vertices);
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, quad_vertices, sizeof(quad_vertices));
		end_update_resource(resource_update_desc);
	}

	ShaderLoadDesc shader_load_desc = {};
	shader_load_desc.stages[0] = { "shaders/quad_vert.hlsl", "main", ShaderStage::kShaderStageVert };
	shader_load_desc.stages[1] = { "shaders/quad_frag.hlsl", "main", ShaderStage::kShaderStageFrag };
	ShaderDesc* shader_desc = nullptr;
	load_shader(&shader_load_desc, &shader_desc);
	Shader* shader;
	add_shader(shader_desc, &shader);
	
	shader_load_desc = {};
	std::free(shader_desc); shader_desc = nullptr;
	shader_load_desc.stages[0] = { "shaders/raytracing_comp.hlsl", "main", ShaderStage::kShaderStageComp };
	load_shader(&shader_load_desc, &shader_desc);
	Shader* compute_shader;
	add_shader(shader_desc, &compute_shader);

	DescriptorSetDesc set_desc;
	set_desc.max_sets = 1;
	set_desc.update_freq = kUpdateFreqNone;
	set_desc.shader = compute_shader;
	DescriptorSet* uav_set;
	add_descriptor_set(&set_desc, &uav_set);

	DescriptorSet* srv_set;
	set_desc.shader = shader;
	add_descriptor_set(&set_desc, &srv_set);

	std::vector<DescriptorInfo> infos{
		{
			.name = "quad_tex",
			.descriptor = quad
		},
	};

	UpdateDescriptorSetDesc update_set_desc{};
	update_set_desc = {};
	update_set_desc.index = 0;
	update_set_desc.infos = std::move(infos);
	update_descriptor_set(&update_set_desc, uav_set);

	infos = std::vector<DescriptorInfo>{
		{
			.name = "quad_tex",
			.descriptor =
				DescriptorInfo::CombinedTextureSample{
					quad,
					"samplerState",
				}
		},

		{
			.name = "samplerState",
			.descriptor = sampler
		},
	};
	update_set_desc.infos = std::move(infos);
	update_descriptor_set(&update_set_desc, srv_set);

	set_desc.max_sets = image_count;
	set_desc.shader = compute_shader;
	set_desc.update_freq = kUpdateFreqPerFrame;
	DescriptorSet* ubo_desc;
	add_descriptor_set(&set_desc, &ubo_desc);


	update_set_desc = {};
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

	VertexLayout layout = { 0 };
	layout.attrib_count = 2;
	layout.attribs[0].size = 2;
	layout.attribs[0].format = kAttribFormatFloat;
	layout.attribs[0].offset = 0u;
	layout.attribs[1].size = 2;
	layout.attribs[1].format = kAttribFormatFloat;
	layout.attribs[1].offset = 2 * sizeof(float);

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
	cmd_desc.use_push_constant = false;
	CmdBuffer* cmd;
	add_cmd(&cmd_desc, &cmd);

	camera.pos = glm::vec3(0.0f, 0.0f, 0.0f);

	ubo.spheres[0] = { Sphere(glm::vec3(0.0f, 0.0f, -1.0f), 0.5f) };
	ubo.spheres[1] = { Sphere(glm::vec3(0.0f, -100.5f, -1.0f), 100.0f) };

	uint32_t group_x = (dims.width + 15) / 16;
	uint32_t group_y = (dims.height + 15) / 16;

	while (update_window())
	{
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		process_input((GLFWwindow*)get_window());

		imgui_begin_frame();

		ubo.cameraPos = glm::vec4(camera.pos, 0.0f);
		glm::mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), dims.width / (float)dims.height, 0.1f, 100.0f);
		glm::mat4 viewMatrix = glm::lookAt(glm::vec3(ubo.cameraPos), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.invViewProj = glm::inverse(projectionMatrix * viewMatrix);
		ubo.samples_per_pixel = samples_per_pixel;
		ubo.max_ray_depth = max_ray_depth;
		// Not sure that I really need to pass random number every frame here
		ubo.seed[0] = 42u;// random_uint();


		BufferUpdateDesc update;
		update.buffer = ubo_buf[frame_index];
		update.size = sizeof(ubo);
		resource_update_desc = &update;
		begin_update_resource(resource_update_desc);
		std::memcpy(update.mapped_data, &ubo, sizeof(ubo));
		end_update_resource(resource_update_desc);

		glUseProgram(compute_shader->program);
		cmd_bind_descriptor_set(cmd, uav_set, 0);
		cmd_bind_descriptor_set(cmd, ubo_desc, frame_index);
		cmd_dispatch(cmd, group_x, group_y, 1);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		cmd_bind_pipeline(cmd, graphics_pipeline);
		cmd_bind_vertex_buffer(cmd, vbo, layout.attrib_count, 0, sizeof(float) * 4);
		cmd_bind_descriptor_set(cmd, srv_set, 0);
		cmd_draw(cmd, 0, 4);

		queue_submit(queue);

		imgui_render();
		imgui_end_frame();

		swapchain->swap_buffers(swapchain->window);
		glfwPollEvents();

		frame_index = (frame_index + 1) % image_count;
	}

	terminate_window();
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);

	WindowDimensions* dims = static_cast<WindowDimensions*>(glfwGetWindowUserPointer(window));
	dims->width = width;
	dims->height = height;
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
}


void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
}

