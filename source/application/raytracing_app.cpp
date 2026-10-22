#include <window.h>
#include <imgui.h>
#include <imgui_layer.h>
#include <render.h>
#include <gpu_resource.h>

#include <profiler.h>
#include <math/yar_math.h>

#define NOMINMAX
#include <Windows.h>

#include <random>
#include <iostream>
#include <stddef.h>
#include <cmath>
#include <algorithm>

#include "../shaders/rt_common.h"


static LARGE_INTEGER g_perf_frequency;
static LARGE_INTEGER g_perf_start;

static double get_time()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (double)(now.QuadPart - g_perf_start.QuadPart) / (double)g_perf_frequency.QuadPart;
}

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
	desc.format = yar_format_r8g8b8a8_unorm;
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

void mouse_callback(double xpos, double ypos);
void scroll_callback(double xoffset, double yoffset);
void process_input();

float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct Lambertian
{
	Lambertian(Vector3 albedo)
		: mat({ albedo, 0.0f, 0.0f, MaterialType::Lambertian }) {
	}

	MaterialData mat;
};

struct Metal
{
	Metal(Vector3 albedo, float fuzz)
		: mat({ albedo, fuzz, 0.0f, MaterialType::Metal }) {
	}

	MaterialData mat;
};

struct Dielectric
{
	Dielectric(float refraction_index)
		: mat({ Vector3{}, 0.0f, refraction_index, MaterialType::Dielectric }) {
	}

	MaterialData mat;
};

struct PrimitiveRecord
{
	Vector3 aabb_min;
	Vector3 aabb_max;
	PrimitiveRef ref;
};

std::vector<PrimitiveRecord> primitive_records;
std::vector<PrimitiveRef> primitive_refs;
std::vector<Sphere> spheres;
std::vector<uint32_t> spheres_mat_indexes;
std::vector<Quad> quads;
std::vector<uint32_t> quads_mat_indexes;
std::vector<MaterialData> mats;
UBO ubo;

// Fixed seed so the random scene is identical on every run.
std::mt19937 scene_rng(1337u);

float random_float(float min = 0.0f, float max = 1.0f)
{
	return std::uniform_real_distribution<float>(min, max)(scene_rng);
}

Vector3 random_color(float min = 0.0f, float max = 1.0f)
{
	return Vector3(random_float(min, max), random_float(min, max), random_float(min, max));
}

void add_sphere(const Vector3& center, float radius, const MaterialData& mat)
{
	PrimitiveRecord rec{};
	Vector3 radius_vec = Vector3(radius);
	rec.aabb_min = center - radius_vec;
	rec.aabb_max = center + radius_vec;
	rec.ref = {
		.type = PrimitiveType::Sphere,
		.index = static_cast<uint32_t>(spheres.size())
	};

	primitive_records.push_back(rec);

	spheres.push_back(Sphere(center, radius));
	spheres_mat_indexes.push_back(mats.size());
	mats.push_back(mat);
}

void add_quad(Vector3 Q, Vector3 u, Vector3 v, const MaterialData& mat)
{
	Quad quad{};
	Vector3 normal = u.cross(v);
	quad.corner = Q;
	quad.normal = normal.normalized();
	quad.plane_d = quad.normal.dot(Q);

	auto w = normal / normal.dot(normal);
	quad.dual_u = v.cross(w);
	quad.dual_v = w.cross(u);

	PrimitiveRecord rec{};
	rec.ref = {
		.type = PrimitiveType::Quad,
		.index = static_cast<uint32_t>(quads.size())
	};

	const Vector3 Qu = Q + u;
	const Vector3 Qv = Q + v;
	const Vector3 Quv = Q + u + v;
	Vector3 aabb_min = min(min(Q, Qu), min(Qv, Quv));
	Vector3 aabb_max = max(max(Q, Qu), max(Qv, Quv));
	static constexpr float delta = 0.0001f;
	static constexpr float half_delta = 0.5f * delta;
	for (uint32_t i = 0; i < 3; ++i)
	{
		if (aabb_max[i] - aabb_min[i] < delta)
		{
			aabb_max[i] += half_delta;
			aabb_min[i] -= half_delta;
		}
	}
	rec.aabb_min = aabb_min;
	rec.aabb_max = aabb_max;

	primitive_records.push_back(rec);

	quads.push_back(quad);
	quads_mat_indexes.push_back(mats.size());
	mats.push_back(mat);
}

struct Camera
{
	Vector3 pos = Vector3(0.0f, 0.0f, 3.0f);
	Vector3 front = Vector3(0.0f, 0.0f, -1.0f);
	Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
} camera;

float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 1920.0f / 2.0;
float lastY = 1080.0 / 2.0;
float fov = 20.0f;

void set_camera(const Vector3& look_from, const Vector3& look_at, float vfov)
{
	camera.pos = look_from;
	camera.front = (look_at - look_from).normalized();
	yaw = degrees(std::atan2(camera.front.z(), camera.front.x()));
	pitch = degrees(std::asin(camera.front.y()));
	fov = vfov;
}

enum scene_id : int32_t
{
	scene_spheres,
	scene_quads,
	scene_count
};

const char* scene_names[scene_count] = { "Spheres", "Quads" };

// Spheres from the Next Week book
void build_spheres_scene()
{
	scene_rng.seed(1337u);
	set_camera(Vector3(13.0f, 2.0f, 3.0f), Vector3(0.0f), 20.0f);

	add_sphere(Vector3(0.0f, -1000.0f, 0.0f), 1000.0f, Lambertian(Vector3(0.5f)).mat);

	for (int a = -11; a < 11; ++a)
	{
		for (int b = -11; b < 11; ++b)
		{
			const float choose_mat = random_float();
			const Vector3 center(a + 0.9f * random_float(), 0.2f, b + 0.9f * random_float());

			// keep clear of the big metal sphere
			if ((center - Vector3(4.0f, 0.2f, 0.0f)).length() <= 0.9f)
				continue;

			if (choose_mat < 0.8f)
				add_sphere(center, 0.2f, Lambertian(random_color() * random_color()).mat);
			else if (choose_mat < 0.95f)
				add_sphere(center, 0.2f, Metal(random_color(0.5f, 1.0f), random_float(0.0f, 0.5f)).mat);
			else
				add_sphere(center, 0.2f, Dielectric(1.5f).mat);
		}
	}

	add_sphere(Vector3(0.0f, 1.0f, 0.0f), 1.0f, Dielectric(1.5f).mat);
	add_sphere(Vector3(-4.0f, 1.0f, 0.0f), 1.0f, Lambertian(Vector3(0.4f, 0.2f, 0.1f)).mat);
	add_sphere(Vector3(4.0f, 1.0f, 0.0f), 1.0f, Metal(Vector3(0.7f, 0.6f, 0.5f), 0.0f).mat);
}

// Quads from the Next Week book
void build_quads_scene()
{
	set_camera(Vector3(0.0f, 0.0f, 9.0f), Vector3(0.0f), 80.0f);

	add_quad(Vector3(-3.0f, -2.0f, 5.0f), Vector3(0.0f, 0.0f, -4.0f), Vector3(0.0f, 4.0f, 0.0f), Lambertian(Vector3(1.0f, 0.2f, 0.2f)).mat);
	add_quad(Vector3(-2.0f, -2.0f, 0.0f), Vector3(4.0f, 0.0f, 0.0f), Vector3(0.0f, 4.0f, 0.0f), Lambertian(Vector3(0.2f, 1.0f, 0.2f)).mat);
	add_quad(Vector3(3.0f, -2.0f, 1.0f), Vector3(0.0f, 0.0f, 4.0f), Vector3(0.0f, 4.0f, 0.0f), Lambertian(Vector3(0.2f, 0.2f, 1.0f)).mat);
	add_quad(Vector3(-2.0f, 3.0f, 1.0f), Vector3(4.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 4.0f), Lambertian(Vector3(1.0f, 0.5f, 0.0f)).mat);
	add_quad(Vector3(-2.0f, -3.0f, 5.0f), Vector3(4.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -4.0f), Lambertian(Vector3(0.2f, 0.8f, 0.8f)).mat);
}

void clear_scene()
{
	spheres.clear();
	spheres_mat_indexes.clear();
	quads.clear();
	quads_mat_indexes.clear();
	mats.clear();
	primitive_records.clear();
	primitive_refs.clear();
}

yar_texture* create_texture(const uint32_t width, const uint32_t height)
{
	yar_texture* tex;
	yar_texture_desc texture_desc{};
	texture_desc.width = width;
	texture_desc.height = height;
	texture_desc.mip_levels = 1;
	texture_desc.type = yar_texture_type_2d;
	texture_desc.format = yar_format_r32g32b32a32_sfloat;
	// Written by the compute pass as a UAV, then sampled by the fullscreen quad.
	texture_desc.usage = yar_texture_usage_unordered_access | yar_texture_usage_shader_resource;
	texture_desc.name = "raytracer_output";
	add_texture(&texture_desc, &tex);

	return tex;
}

std::vector<BVHNode> bvh;
uint32_t root_node_index = 0;
uint32_t nodes_used = 1;

auto update_node_bounds(const uint32_t node_index)
{
	BVHNode& node = bvh[node_index];
	node.aabb_min = Vector3(std::numeric_limits<float>::max());
	node.aabb_max = Vector3(std::numeric_limits<float>::lowest());
	for (uint32_t first = node.left_first, i = 0; i < node.prim_count; ++i)
	{
		const PrimitiveRecord& rec = primitive_records[first + i];
		node.aabb_min = min(node.aabb_min, rec.aabb_min);
		node.aabb_max = max(node.aabb_max, rec.aabb_max);
	}
}

auto subdivide(const uint32_t node_index)
{
	BVHNode& node = bvh[node_index];
	if (node.prim_count <= 2)
		return;

	// determine split axis and position
	Vector3 extent = node.aabb_max - node.aabb_min;
	uint32_t axis = 0;
	if (extent.y() > extent.x())
		axis = 1;
	if (extent.z() > extent[axis])
		axis = 2;

	const float split_pos = node.aabb_min[axis] + extent[axis] * 0.5f;

	// in-place partition
	int32_t i = node.left_first;
	int32_t j = i + node.prim_count - 1;
	while (i <= j)
	{
		const PrimitiveRecord& rec = primitive_records[i];
		const Vector3 centroid = (rec.aabb_min + rec.aabb_max) / 2;
			if (centroid[axis] < split_pos)
				i++;
			else
				std::swap(primitive_records[i], primitive_records[j--]);
	}

	// abort if one of the sides is empty
	int left_count = i - node.left_first;
	if (left_count == 0 || left_count == node.prim_count)
		return;

	uint32_t left_child_index = nodes_used++;
	uint32_t right_child_index = nodes_used++;
	bvh[left_child_index].prim_count = left_count;
	bvh[left_child_index].left_first = node.left_first;
	bvh[right_child_index].prim_count = node.prim_count - left_count;
	bvh[right_child_index].left_first = i;

	node.left_first = left_child_index;
	node.prim_count = 0; // it's node - not leaf

	update_node_bounds(left_child_index);
	update_node_bounds(right_child_index);

	subdivide(left_child_index);
	subdivide(right_child_index);
}

auto build_bvh()
{
	const uint32_t prim_count = static_cast<uint32_t>(primitive_records.size());

	bvh.clear();
	nodes_used = 0;
	if (prim_count == 0)
		return;

	// A binary tree over N primitives never needs more than 2N - 1 nodes.
	bvh.assign(prim_count * 2 - 1, BVHNode{});
	nodes_used = 1;

	BVHNode& root = bvh[root_node_index];
	root.left_first = 0;
	root.prim_count = prim_count;

	update_node_bounds(root_node_index);
	subdivide(root_node_index);

	// Drop the slots the build didn't use so only real nodes are uploaded.
	bvh.resize(nodes_used);

	for (uint32_t i = 0; i < prim_count; ++i)
	{
		primitive_refs.push_back(primitive_records[i].ref);
	}
}

std::vector<gpu_buffer> scene_buffers;

template <typename T>
yar_buffer* add_scene_buffer(const char* name, const std::vector<T>& data)
{
	// GL rejects zero-sized storage, so an empty list is uploaded as one zeroed element.
	const T zero{};
	const T* src = data.empty() ? &zero : data.data();
	const uint32_t bytes = static_cast<uint32_t>(std::max<size_t>(data.size(), 1) * sizeof(T));

	yar_buffer_desc desc{
		.size = bytes,
		.usage = yar_buffer_usage_storage_buffer,
		.flags = yar_buffer_flag_gpu_only,
		.name = name
	};
	gpu_buffer& buffer = scene_buffers.emplace_back(&desc);

	yar_buffer_update_desc update_desc{};
	update_desc.buffer = buffer.get();
	update_desc.size = bytes;
	yar_resource_update_desc resource_update_desc{};
	resource_update_desc = &update_desc;
	begin_update_resource(resource_update_desc);
	std::memcpy(update_desc.mapped_data, src, bytes);
	end_update_resource(resource_update_desc);

	return buffer.get();
}

void load_scene(scene_id id, yar_texture* output, yar_descriptor_set* set)
{
	clear_scene();
	switch (id)
	{
	case scene_spheres: build_spheres_scene(); break;
	case scene_quads:   build_quads_scene();   break;
	default: break;
	}

	build_bvh();
	std::cout << scene_names[id] << ": " << nodes_used << " BVH nodes for " << spheres.size()
		<< " spheres, " << quads.size() << " quads" << std::endl;

	scene_buffers.clear();

	// update_descriptor_set replaces the whole vector, so every set-0
	// resource has to be listed in one call.
	std::vector<yar_descriptor_info> infos{
		{ .name = "quad_tex",            .descriptor = output },
		{ .name = "spheres",             .descriptor = add_scene_buffer("spheres", spheres) },
		{ .name = "quads",               .descriptor = add_scene_buffer("quads", quads) },
		{ .name = "mats",                .descriptor = add_scene_buffer("mats", mats) },
		{ .name = "bvh",                 .descriptor = add_scene_buffer("bvh", bvh) },
		{ .name = "primitives",          .descriptor = add_scene_buffer("primitives", primitive_refs) },
		{ .name = "spheres_mat_indexes", .descriptor = add_scene_buffer("spheres_mat_indexes", spheres_mat_indexes) },
		{ .name = "quads_mat_indexes",   .descriptor = add_scene_buffer("quads_mat_indexes", quads_mat_indexes) },
	};

	yar_update_descriptor_set_desc update_set_desc{};
	update_set_desc.index = 0;
	update_set_desc.infos = std::move(infos);
	update_descriptor_set(&update_set_desc, set);
}

auto main() -> int
{
	QueryPerformanceFrequency(&g_perf_frequency);
	QueryPerformanceCounter(&g_perf_start);

	int32_t samples_per_pixel = 1u;
	int32_t max_ray_depth = 10;
	bool use_bvh = true;
	int32_t selected_scene = scene_spheres;

	const auto imgui_layer = [&]()
		{
			ImGui::Begin("Raytracer settings");
			ImGui::Combo("Scene", &selected_scene, scene_names, scene_count);
			ImGui::SliderInt("Samples Per Pixel", &samples_per_pixel, 1, 100);
			ImGui::SliderInt("Max Ray Depth", &max_ray_depth, 1, 100);
			ImGui::Checkbox("Use BVH", &use_bvh);
			ImGui::End();

			// No-op unless built in the Profile configuration.
			profiler_draw_imgui();
		};


	init_window(imgui_layer);
	register_mouse_callback(mouse_callback);
	register_scroll_callback(scroll_callback);
	init_render();

	const auto& win_dims = get_window_dims();
	int32_t w = win_dims.width;
	int32_t h = win_dims.height;

	yar_swapchain_desc swapchain_desc{};
	swapchain_desc.buffer_count = 2;
	swapchain_desc.format = yar_format_r8g8b8a8_srgb;
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

	yar_buffer_desc buffer_desc{
		.size = sizeof(quad_vertices),
		.usage = yar_buffer_usage_vertex_buffer,
		.flags = yar_buffer_flag_gpu_only,
		.name = "quad_vertex_buffer"
	};
	gpu_buffer vbo(&buffer_desc);

	yar_texture* imgui_fonts = get_imgui_fonts();

	constexpr uint32_t frames_in_flight = 2;
	uint32_t frame_index = 0;

	buffer_desc.size = sizeof(ubo);
	buffer_desc.usage = yar_buffer_usage_uniform_buffer;
	buffer_desc.flags = yar_buffer_flag_map_write;
	buffer_desc.name = "UBO";
	gpu_buffer ubo_buf[frames_in_flight] = {gpu_buffer(&buffer_desc), gpu_buffer(&buffer_desc)};

	yar_resource_update_desc resource_update_desc{};
	{ // update buffers data
		yar_buffer_update_desc update_desc{};
		resource_update_desc = &update_desc;
		update_desc.buffer = vbo.get();
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
	shader_load_desc.stages[0] = { "shaders/raytracing_comp.slang", "main", yar_shader_stage::yar_shader_stage_comp };
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


	yar_descriptor_set_desc set_desc{};
	set_desc.max_sets = 1;
	set_desc.update_freq = yar_update_freq_none;
	set_desc.shader = compute_shader;
	yar_descriptor_set* uav_set;
	add_descriptor_set(&set_desc, &uav_set);

	yar_descriptor_set* srv_set;
	set_desc.shader = shader;
	add_descriptor_set(&set_desc, &srv_set);

	int32_t loaded_scene = selected_scene;
	load_scene(static_cast<scene_id>(loaded_scene), quad, uav_set);

	yar_update_descriptor_set_desc update_set_desc{};
	update_set_desc.index = 0;
	std::vector<yar_descriptor_info> infos{
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

	set_desc.max_sets = frames_in_flight;
	set_desc.shader = compute_shader;
	set_desc.update_freq = yar_update_freq_per_frame;
	yar_descriptor_set* ubo_desc;
	add_descriptor_set(&set_desc, &ubo_desc);

	update_set_desc = {};
	for (uint32_t i = 0; i < frames_in_flight; ++i)
	{
		std::vector<yar_descriptor_info> infos{
			{
				.name = "ubo",
				.descriptor = ubo_buf[i].get()
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

	yar_vertex_layout layout{};
	layout.attrib_count = 2;
	layout.attribs[0] = { .format = yar_format_r32g32_sfloat, .offset = 0u };
	layout.attribs[1] = { .format = yar_format_r32g32_sfloat, .offset = 2 * sizeof(float) };

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
	imgui_layout.attribs[0] = { .format = yar_format_r32g32_sfloat, .offset = offsetof(ImDrawVert, pos) };
	imgui_layout.attribs[1] = { .format = yar_format_r32g32_sfloat, .offset = offsetof(ImDrawVert, uv) };
	imgui_layout.attribs[2] = { .format = yar_format_r8g8b8a8_unorm, .offset = offsetof(ImDrawVert, col) };

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

	yar_cmd_queue_desc queue_desc{};
	yar_cmd_queue* queue;
	add_queue(&queue_desc, &queue);

	yar_cmd_buffer_desc cmd_desc{};
	cmd_desc.current_queue = queue;
	cmd_desc.use_push_constant = false;
	yar_cmd_buffer* cmd;
	add_cmd(&cmd_desc, &cmd);

	uint32_t group_x = (dims.width + 15) / 16;
	uint32_t group_y = (dims.height + 15) / 16;

	while (update_window())
	{
		profiler_begin_frame();
		YAR_CPU_SCOPE("Frame");

		float currentFrame = static_cast<float>(get_time());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		process_input();

		auto* draw_data = imgui_get_new_frame_data();

		// The previous frame's commands already ran in queue_submit, so the old buffers are free to go.
		if (selected_scene != loaded_scene)
		{
			loaded_scene = selected_scene;
			load_scene(static_cast<scene_id>(loaded_scene), quad, uav_set);
		}
		int32_t fb_width = static_cast<int32_t>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
		int32_t fb_height = static_cast<int32_t>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
		ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
		ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		Matrix4x4 ortho = Matrix4x4::ortho_off_center_rh_gl(
			draw_data->DisplayPos.x,
			draw_data->DisplayPos.x + draw_data->DisplaySize.x,
			draw_data->DisplayPos.y + draw_data->DisplaySize.y,
			draw_data->DisplayPos.y,
			-1.0f,
			1.0f
		);

		ubo.camera_pos = Vector4(camera.pos, 0.0f);
		Matrix4x4 projectionMatrix = Matrix4x4::perspective_fov_rh_gl(radians(fov), dims.width / (float)dims.height, 0.1f, 100.0f);
		Matrix4x4 viewMatrix = Matrix4x4::look_at_rh(camera.pos, camera.pos + camera.front, camera.up);
		ubo.inv_view_proj = (viewMatrix * projectionMatrix).inverse();
		ubo.samples_per_pixel = samples_per_pixel;
		ubo.max_ray_depth = max_ray_depth;
		ubo.seed = random_uint();
		ubo.use_bvh = use_bvh ? 1u : 0u;
		ubo.ui_ortho = ortho;

		yar_buffer_update_desc update{};
		update.buffer = ubo_buf[frame_index].get();
		update.size = sizeof(ubo);
		resource_update_desc = &update;
		begin_update_resource(resource_update_desc);
		std::memcpy(update.mapped_data, &ubo, sizeof(ubo));
		end_update_resource(resource_update_desc);

		uint32_t sc_image;
		acquire_next_image(swapchain, sc_image);

		YAR_GPU_SCOPE_BEGIN(cmd, "GPU Frame");

		YAR_GPU_SCOPE_BEGIN(cmd, "Raytrace Dispatch");
		cmd_bind_pipeline(cmd, compute_pipeline);
		cmd_bind_descriptor_set(cmd, uav_set, 0);
		cmd_bind_descriptor_set(cmd, ubo_desc, frame_index);
		cmd_dispatch(cmd, group_x, group_y, 1);
		YAR_GPU_SCOPE_END(cmd);

		yar_render_pass_desc fullscreen_quad_pass{};
		fullscreen_quad_pass.color_attachment_count = 1;
		fullscreen_quad_pass.color_attachments[0].target = swapchain->render_targets[sc_image];

		cmd_begin_render_pass(cmd, &fullscreen_quad_pass);

		YAR_GPU_SCOPE_BEGIN(cmd, "Fullscreen Quad");
		cmd_bind_pipeline(cmd, graphics_pipeline);
		cmd_bind_vertex_buffer(cmd, vbo.get(), layout.attrib_count, 0, sizeof(float) * 4);
		cmd_bind_descriptor_set(cmd, srv_set, 0);
		cmd_draw(cmd, 0, 4);
		YAR_GPU_SCOPE_END(cmd);

		// render imgui to swapchain rt tmp solution
		YAR_GPU_SCOPE_BEGIN(cmd, "ImGui");
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
			for (int j = 0; j < cmd_list->CmdBuffer.Size; ++j)
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

		YAR_GPU_SCOPE_END(cmd); // ImGui

		cmd_end_render_pass(cmd);

		YAR_GPU_SCOPE_END(cmd); // GPU Frame

		queue_submit(queue);

		yar_queue_present_desc present_desc{};
		present_desc.swapchain = swapchain;
		queue_present(queue, &present_desc);


		frame_index = (frame_index + 1) % frames_in_flight;
	}

	// Scene buffers are globals; release them while the GL context is still alive.
	scene_buffers.clear();
	terminate_window();
}

bool isRightMouseButtonPressed = false;

void mouse_callback(double xposIn, double yposIn)
{
	if (ImGui::GetIO().WantCaptureMouse || !(GetAsyncKeyState(VK_RBUTTON) & 0x8000))
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

	Vector3 new_front(
		std::cos(radians(yaw)) * std::cos(radians(pitch)),
		std::sin(radians(pitch)),
		std::sin(radians(yaw)) * std::cos(radians(pitch))
	);
	camera.front = new_front.normalized();
}

void process_input()
{
	if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
		PostMessage((HWND)get_window(), WM_CLOSE, 0, 0);

	float speed = static_cast<float>(2.5 * deltaTime);
	if (GetAsyncKeyState('W') & 0x8000)
		camera.pos += speed * camera.front;
	if (GetAsyncKeyState('S') & 0x8000)
		camera.pos -= speed * camera.front;
	if (GetAsyncKeyState('A') & 0x8000)
		camera.pos -= camera.front.cross(camera.up).normalized() * speed;
	if (GetAsyncKeyState('D') & 0x8000)
		camera.pos += camera.front.cross(camera.up).normalized() * speed;
	if (GetAsyncKeyState('E') & 0x8000)
		camera.pos += camera.up * speed;
	if (GetAsyncKeyState('Q') & 0x8000)
		camera.pos -= camera.up * speed;
}

void scroll_callback(double xoffset, double yoffset)
{
}
