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
#include <iostream>
#include <stddef.h>

struct ImGuiVertex {
	glm::vec2 position;
	glm::vec2 uv;
	uint32_t color;
};

inline uint32_t random_uint() 
{
	static std::uniform_int_distribution<uint32_t> distribution(0, UINT32_MAX);
	static std::mt19937 generator(std::random_device{}());
	return distribution(generator);
}

yar_texture* get_imgui_fonts()
{
	auto& font_atlas = ImGui::GetIO().Fonts;
	uint8_t* pixels;
	int32_t width, height, bpp;
	font_atlas->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);

	yar_texture_desc desc{};
	desc.width = width;
	desc.height = height;
	desc.mip_levels = 1;
	desc.format = yar_texture_format_rgba8;
	desc.name = "ImGui Fonts";
	desc.type = yar_texture_type_2d;
	desc.usage = yar_texture_usage_shader_resource;

	yar_texture* font;
	add_texture(&desc, &font);

	yar_resource_update_desc upd{};
	yar_texture_update_desc tex_upd{};
	upd = &tex_upd;
	tex_upd.data = pixels;
	tex_upd.size = width * height * bpp;
	tex_upd.texture = font;
	begin_update_resource(upd);
	std::memcpy(tex_upd.mapped_data, pixels, tex_upd.size);
	end_update_resource(upd);

	return font;
}

void process_input(GLFWwindow* window);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

enum MaterialType : uint32_t
{
	kLambertian = 0,
	kMetal, 
	kDielectric
};

struct Material
{
	glm::vec3 albedo;
	float fuzz; // only for metals
	float refraction_index; // for dielectircs
	MaterialType type;
	uint32_t pad[2] = {};
};

struct Lambertian
{
	Lambertian(glm::vec3 albedo)
		: mat({ albedo, 0.0f, 0.0f, kLambertian }) {
	}

	Material mat;
};

struct Metal
{
	Metal(glm::vec3 albedo, float fuzz)
		: mat({ albedo, fuzz, 0.0f, kMetal }) {
	}

	Material mat;
};

struct Dielectric
{
	Dielectric(float refraction_index)
		: mat({ glm::vec3{}, 0.0f, refraction_index, kDielectric }) {
	}

	Material mat;
};

struct Sphere
{
	glm::vec3 center;
	float radius;
};

constexpr uint32_t kSpheresCount = 5u;

struct UBO
{
	glm::mat4 invViewProj;
	glm::mat4 ui_ortho;
	glm::vec4 cameraPos;
	Sphere spheres[kSpheresCount];
	Material mats[kSpheresCount];
	int32_t samples_per_pixel;
	int32_t max_ray_depth;
	uint32_t seed;
	uint32_t pad;
}ubo;

struct Camera
{
	glm::vec3 pos = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
} camera;

float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 1920.0f / 2.0;
float lastY = 1080.0 / 2.0;
float fov = 45.0f;

yar_texture* create_texture(const uint32_t width, const uint32_t height)
{
	yar_texture* tex;
	yar_texture_desc texture_desc{};
	texture_desc.width = width;
	texture_desc.height = height;
	texture_desc.mip_levels = 1;
	texture_desc.type = yar_texture_type_2d;
	texture_desc.format = yar_texture_format_rgba32f;
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

	int32_t w, h;
	glfwGetFramebufferSize((GLFWwindow*)get_window(), &w, &h);

	yar_swapchain_desc swapchain_desc{};
	swapchain_desc.buffer_count = 2;
	swapchain_desc.format = yar_texture_format_srgba8;
	swapchain_desc.height = h;
	swapchain_desc.width = w;
	swapchain_desc.vsync = false;
	swapchain_desc.window_handle = get_window();
	yar_swapchain* swapchain;
	add_swapchain(&swapchain_desc, &swapchain);

	auto dims = get_window_dims();
	yar_texture* quad = create_texture(dims.width, dims.height);

	float quad_vertices[] = {
		-1.0f, -1.0f,   0.0f, 0.0f,
		 1.0f, -1.0f,   1.0f, 0.0f,
		-1.0f,  1.0f,   0.0f, 1.0f,
		 1.0f,  1.0f,   1.0f, 1.0f
	};


	yar_sampler* sampler;
	yar_sampler_desc sampler_desc{};
	sampler_desc.min_filter = yar_filter_type_nearest;
	sampler_desc.mag_filter = yar_filter_type_nearest;
	sampler_desc.wrap_u = yar_wrap_mode_repeat;
	sampler_desc.wrap_v = yar_wrap_mode_repeat;
	add_sampler(&sampler_desc, &sampler);

	yar_buffer_desc buffer_desc;
	buffer_desc.size = sizeof(quad_vertices);
	buffer_desc.flags = yar_buffer_flag_gpu_only;
	buffer_desc.name = "quad_vertex_buffer";
	yar_buffer* vbo = nullptr;
	add_buffer(&buffer_desc, &vbo);

	yar_texture* imgui_fonts = get_imgui_fonts();

	constexpr uint32_t image_count = 2;
	uint32_t frame_index = 0;

	buffer_desc.size = sizeof(ubo);
	buffer_desc.flags = yar_buffer_flag_map_write;
	buffer_desc.name = "UBO";
	yar_buffer* ubo_buf[image_count] = { nullptr, nullptr };
	for (auto& buf : ubo_buf)
		add_buffer(&buffer_desc, &buf);

	yar_resource_update_desc resource_update_desc;
	{ // update buffers data
		yar_buffer_update_desc update_desc{};
		resource_update_desc = &update_desc;
		update_desc.buffer = vbo;
		update_desc.size = sizeof(quad_vertices);
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, quad_vertices, sizeof(quad_vertices));
		end_update_resource(resource_update_desc);
	}

	yar_shader_load_desc shader_load_desc = {};
	shader_load_desc.stages[0] = { "shaders/quad_vert.hlsl", "main", yar_shader_stage::yar_shader_stage_vert };
	shader_load_desc.stages[1] = { "shaders/quad_frag.hlsl", "main", yar_shader_stage::yar_shader_stage_pixel };
	yar_shader_desc* shader_desc = nullptr;
	load_shader(&shader_load_desc, &shader_desc);
	yar_shader* shader;
	add_shader(shader_desc, &shader);
	std::free(shader_desc);
	shader_desc = nullptr;
	
	shader_load_desc = {};
	std::free(shader_desc); shader_desc = nullptr;
	shader_load_desc.stages[0] = { "shaders/raytracing_comp.hlsl", "main", yar_shader_stage::yar_shader_stage_comp };
	load_shader(&shader_load_desc, &shader_desc);
	yar_shader* compute_shader;
	add_shader(shader_desc, &compute_shader);
	std::free(shader_desc);
	shader_desc = nullptr;

	shader_load_desc.stages[0] = { "shaders/imgui_vert_rt.hlsl", "main", yar_shader_stage::yar_shader_stage_vert };
	shader_load_desc.stages[1] = { "shaders/imgui_pix.hlsl", "main", yar_shader_stage::yar_shader_stage_pixel };
	load_shader(&shader_load_desc, &shader_desc);
	yar_shader* imgui_shader;
	add_shader(shader_desc, &imgui_shader);
	std::free(shader_desc);
	shader_desc = nullptr;


	yar_descriptor_set_desc set_desc;
	set_desc.max_sets = 1;
	set_desc.update_freq = yar_update_freq_none;
	set_desc.shader = compute_shader;
	yar_descriptor_set* uav_set;
	add_descriptor_set(&set_desc, &uav_set);

	yar_descriptor_set* srv_set;
	set_desc.shader = shader;
	add_descriptor_set(&set_desc, &srv_set);

	std::vector<yar_descriptor_info> infos{
		{
			.name = "quad_tex",
			.descriptor = quad
		},
	};

	yar_update_descriptor_set_desc update_set_desc{};
	update_set_desc = {};
	update_set_desc.index = 0;
	update_set_desc.infos = std::move(infos);
	update_descriptor_set(&update_set_desc, uav_set);

	infos = std::vector<yar_descriptor_info>{
		{
			.name = "quad_tex",
			.descriptor =
				yar_descriptor_info::yar_combined_texture_sample{
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
	set_desc.update_freq = yar_update_freq_per_frame;
	yar_descriptor_set* ubo_desc;
	add_descriptor_set(&set_desc, &ubo_desc);

	update_set_desc = {};
	for (uint32_t i = 0; i < image_count; ++i)
	{
		std::vector<yar_descriptor_info> infos{
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
	set_desc.update_freq = yar_update_freq_none;
	set_desc.shader = imgui_shader;
	yar_descriptor_set* imgui_set;
	add_descriptor_set(&set_desc, &imgui_set);
	std::vector<yar_descriptor_info> imgui_font_info{
		{
			.name = "font",
			.descriptor =
			yar_descriptor_info::yar_combined_texture_sample{
				imgui_fonts,
				"samplerState",
			}
		},
		{
			.name = "samplerState",
			.descriptor = sampler
		}
	};
	update_set_desc.infos = std::move(imgui_font_info);
	update_set_desc.index = 0;
	update_descriptor_set(&update_set_desc, imgui_set);

	yar_vertex_layout layout = { 0 };
	layout.attrib_count = 2;
	layout.attribs[0].size = 2;
	layout.attribs[0].format = yar_attrib_format_float;
	layout.attribs[0].offset = 0u;
	layout.attribs[1].size = 2;
	layout.attribs[1].format = yar_attrib_format_float;
	layout.attribs[1].offset = 2 * sizeof(float);

	yar_pipeline_desc pipeline_desc = { };
	pipeline_desc.type = yar_pipeline_type_graphics;
	pipeline_desc.shader = shader;
	pipeline_desc.vertex_layout = layout;
	pipeline_desc.topology = yar_primitive_topology_triangle_strip;

	yar_pipeline* graphics_pipeline;
	add_pipeline(&pipeline_desc, &graphics_pipeline);

	pipeline_desc.type = yar_pipeline_type_compute;
	pipeline_desc.shader = compute_shader;
	yar_pipeline* compute_pipeline;
	add_pipeline(&pipeline_desc, &compute_pipeline);

	yar_vertex_layout imgui_layout{};
	imgui_layout.attrib_count = 3;
	imgui_layout.attribs[0] = { .size = 2, .format = yar_attrib_format_float, .offset = offsetof(ImGuiVertex, position) };
	imgui_layout.attribs[1] = { .size = 2, .format = yar_attrib_format_float, .offset = offsetof(ImGuiVertex, uv) };
	imgui_layout.attribs[2] = { .size = 4, .format = yar_attrib_format_ubyte, .offset = offsetof(ImGuiVertex, color) };

	pipeline_desc.shader = imgui_shader;
	pipeline_desc.vertex_layout = imgui_layout;
	pipeline_desc.topology = yar_primitive_topology_triangle_list;
	pipeline_desc.type = yar_pipeline_type_graphics;

	yar_depth_stencil_state imgui_depth{};
	imgui_depth.depth_enable = false;
	imgui_depth.depth_write = false;
	imgui_depth.stencil_enable = false;
	imgui_depth.depth_func = yar_depth_stencil_func_never;
	pipeline_desc.depth_stencil_state = imgui_depth;

	yar_blend_state imgui_blend{};
	imgui_blend.blend_enable = true;
	imgui_blend.src_factor = yar_blend_factor_src_alpha;
	imgui_blend.dst_factor = yar_blend_factor_one_minus_src_alpha;
	imgui_blend.op = yar_blend_op_add;
	imgui_blend.src_alpha_factor = yar_blend_factor_one;
	imgui_blend.dst_alpha_factor = yar_blend_factor_one_minus_src_alpha;
	imgui_blend.alpha_op = yar_blend_op_add;
	pipeline_desc.blend_state = imgui_blend;

	yar_rasterizer_state imgui_raster{};
	imgui_raster.fill_mode = yar_fill_mode_solid;
	imgui_raster.cull_mode = yar_cull_mode_none;
	imgui_raster.front_counter_clockwise = false;
	pipeline_desc.rasterizer_state = imgui_raster;

	yar_pipeline* imgui_pipeline;
	add_pipeline(&pipeline_desc, &imgui_pipeline);

	yar_buffer* imgui_vb;
	yar_buffer* imgui_ib;
	yar_buffer_desc imgui_buffer_desc{};

	imgui_buffer_desc.size = 5000 * sizeof(ImDrawVert);
	imgui_buffer_desc.usage = yar_buffer_usage_vertex_buffer;
	imgui_buffer_desc.name = "imgui_vertex_buffer";
	imgui_buffer_desc.flags = yar_buffer_flag_dynamic;
	add_buffer(&imgui_buffer_desc, &imgui_vb);

	imgui_buffer_desc.size = 10000 * sizeof(ImDrawIdx);
	imgui_buffer_desc.usage = yar_buffer_usage_index_buffer;
	imgui_buffer_desc.name = "imgui_index_buffer";
	add_buffer(&imgui_buffer_desc, &imgui_ib);

	imgui_get_new_frame_data();

	yar_cmd_queue_desc queue_desc;
	yar_cmd_queue* queue;
	add_queue(&queue_desc, &queue);

	yar_cmd_buffer_desc cmd_desc;
	cmd_desc.current_queue = queue;
	cmd_desc.use_push_constant = false;
	yar_cmd_buffer* cmd;
	add_cmd(&cmd_desc, &cmd);

	camera.pos = glm::vec3(0.0f, 0.0f, 0.0f);

	Lambertian ground(glm::vec3(0.8f, 0.8f, 0.0f));
	Lambertian center(glm::vec3(0.1f, 0.2f, 0.5f));
	Metal right(glm::vec3(0.8f, 0.6f, 0.2f), 0.7f);

	float left_refraction_index = 1.5f;
	Dielectric left(left_refraction_index);
	Dielectric inner_bubble(1.0f / left_refraction_index);

	ubo.spheres[0] = Sphere(glm::vec3(0.0f, 0.0f, -1.0f), 0.5f);
	ubo.spheres[1] = Sphere(glm::vec3(0.0f, -100.5f, -1.0f), 100.0f);
	ubo.spheres[2] = Sphere(glm::vec3(-1.0f, 0.0f, -1.0f), 0.5f);
	ubo.spheres[3] = Sphere(glm::vec3(1.0f, 0.0f, -1.0f), 0.5f);
	ubo.spheres[4] = Sphere(glm::vec3(-1.0f, 0.0f, -1.0f), 0.4f);
	ubo.mats[0] = center.mat;
	ubo.mats[1] = ground.mat;
	ubo.mats[2] = left.mat;
	ubo.mats[3] = right.mat;
	ubo.mats[4] = inner_bubble.mat;

	uint32_t group_x = (dims.width + 15) / 16;
	uint32_t group_y = (dims.height + 15) / 16;

	while (update_window())
	{
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		process_input((GLFWwindow*)get_window());

		auto* draw_data = imgui_get_new_frame_data();
		int32_t fb_width = static_cast<int32_t>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
		int32_t fb_height = static_cast<int32_t>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
		ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
		ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		glm::mat4 ortho = glm::ortho(
			draw_data->DisplayPos.x,
			draw_data->DisplayPos.x + draw_data->DisplaySize.x,
			draw_data->DisplayPos.y + draw_data->DisplaySize.y,
			draw_data->DisplayPos.y,
			-1.0f,
			1.0f
		);

		ubo.cameraPos = glm::vec4(camera.pos, 0.0f);
		glm::mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), dims.width / (float)dims.height, 0.1f, 100.0f);
		glm::mat4 viewMatrix = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);
		ubo.invViewProj = glm::inverse(projectionMatrix * viewMatrix);
		ubo.samples_per_pixel = samples_per_pixel;
		ubo.max_ray_depth = max_ray_depth;
		// Not sure that I really need to pass random number every frame here
		ubo.seed = 42u;// random_uint();
		ubo.ui_ortho = ortho;


		yar_buffer_update_desc update;
		update.buffer = ubo_buf[frame_index];
		update.size = sizeof(ubo);
		resource_update_desc = &update;
		begin_update_resource(resource_update_desc);
		std::memcpy(update.mapped_data, &ubo, sizeof(ubo));
		end_update_resource(resource_update_desc);

		uint32_t sc_image;
		acquire_next_image(swapchain, sc_image);

		cmd_bind_pipeline(cmd, compute_pipeline);
		cmd_bind_descriptor_set(cmd, uav_set, 0);
		cmd_bind_descriptor_set(cmd, ubo_desc, frame_index);
		cmd_dispatch(cmd, group_x, group_y, 1);

		yar_render_pass_desc fullscreen_quad_pass{};
		fullscreen_quad_pass.color_attachment_count = 1;
		fullscreen_quad_pass.color_attachments[0].target = swapchain->render_targets[sc_image];

		cmd_begin_render_pass(cmd, &fullscreen_quad_pass);

		cmd_bind_pipeline(cmd, graphics_pipeline);
		cmd_bind_vertex_buffer(cmd, vbo, layout.attrib_count, 0, sizeof(float) * 4);
		cmd_bind_descriptor_set(cmd, srv_set, 0);
		cmd_draw(cmd, 0, 4);

		// render imgui to swapchain rt tmp solution
		cmd_bind_pipeline(cmd, imgui_pipeline);
		cmd_bind_descriptor_set(cmd, imgui_set, 0);
		cmd_bind_vertex_buffer(cmd, imgui_vb, imgui_layout.attrib_count, 0, sizeof(ImDrawVert));
		cmd_bind_index_buffer(cmd, imgui_ib);
		cmd_set_viewport(cmd, fb_width, fb_height);
		for (int i = 0; i < draw_data->CmdListsCount; ++i)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[i];
			cmd_update_buffer(cmd, imgui_vb, 0, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), cmd_list->VtxBuffer.Data);
			cmd_update_buffer(cmd, imgui_ib, 0, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), cmd_list->IdxBuffer.Data);
			for (uint32_t j = 0; j < cmd_list->CmdBuffer.Size; ++j)
			{
				const ImDrawCmd* draw_cmd = &cmd_list->CmdBuffer[j];

				// Project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_min((draw_cmd->ClipRect.x - clip_off.x) * clip_scale.x, (draw_cmd->ClipRect.y - clip_off.y) * clip_scale.y);
				ImVec2 clip_max((draw_cmd->ClipRect.z - clip_off.x) * clip_scale.x, (draw_cmd->ClipRect.w - clip_off.y) * clip_scale.y);
				if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
					continue;

				cmd_set_scissor(cmd, (int)clip_min.x, (int)((float)fb_height - clip_max.y), (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y));
				cmd_draw_indexed(cmd, draw_cmd->ElemCount, yar_index_type_ushort, draw_cmd->IdxOffset, draw_cmd->VtxOffset);
			}
		}

		cmd_end_render_pass(cmd);

		queue_submit(queue);

		yar_queue_present_desc present_desc{};
		present_desc.swapchain = swapchain;
		queue_present(queue, &present_desc);

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

bool isRightMouseButtonPressed = false;

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
	if (ImGui::GetIO().WantCaptureMouse || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS)
	{
		isRightMouseButtonPressed = false;
		return;
	}

	if (!isRightMouseButtonPressed)
	{
		lastX = static_cast<float>(xposIn);
		lastY = static_cast<float>(yposIn);
		isRightMouseButtonPressed = true; 
	}

	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

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

