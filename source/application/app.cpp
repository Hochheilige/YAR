#include <window.h>
#include <imgui.h>
#include <imgui_layer.h>
#include <render.h>
#include <asset_manager.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <meshoptimizer.h>

#include <iostream>
#include <optional>

#include <cstddef>

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

struct Texture 
{
	std::shared_future<std::shared_ptr<TextureAsset>> texture_asset;
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec3 bitangent;
	glm::vec2 tex_coord;
	glm::vec2 tex_coord1;
};

struct SkyBoxVertex
{
	glm::vec3 position;
};

struct ImGuiVertex {
	glm::vec2 position;
	glm::vec2 uv; 
	uint32_t color;   
};


class Mesh
{
public:
	Mesh(std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices, std::vector<Texture*>&& textures, std::string name) :
		vertices(vertices), indices(indices), textures(textures),
		gpu_vertex_buffer(nullptr), gpu_index_buffer(nullptr), name(name)
	{
		optimize_mesh();
		upload_buffers();
	}

	void setup_vertex_layout(yar_vertex_layout& layout)
	{
		layout.attrib_count = 6u;
		layout.attribs[0].size = 3u;
		layout.attribs[0].format = yar_attrib_format_float;
		layout.attribs[0].offset = offsetof(Vertex, position);
		layout.attribs[1].size = 3u;
		layout.attribs[1].format = yar_attrib_format_float;
		layout.attribs[1].offset = offsetof(Vertex, normal);
		layout.attribs[2].size = 3u;
		layout.attribs[2].format = yar_attrib_format_float;
		layout.attribs[2].offset = offsetof(Vertex, tangent);
		layout.attribs[3].size = 3u;
		layout.attribs[3].format = yar_attrib_format_float;
		layout.attribs[3].offset = offsetof(Vertex, bitangent);
		layout.attribs[4].size = 2u;
		layout.attribs[4].format = yar_attrib_format_float;
		layout.attribs[4].offset = offsetof(Vertex, tex_coord);
		layout.attribs[5].size = 2u;
		layout.attribs[5].format = yar_attrib_format_float;
		layout.attribs[5].offset = offsetof(Vertex, tex_coord1);
	}

	void draw(yar_cmd_buffer* cmd)
	{
		cmd_bind_vertex_buffer(cmd, gpu_vertex_buffer, attrib_count, 0, sizeof(Vertex));
		cmd_bind_index_buffer(cmd, gpu_index_buffer);
		cmd_draw_indexed(cmd, indices.size(), yar_index_type_uint, 0, 0);
	}

	yar_texture* get_texture(uint32_t index)
	{
		if (index >= textures.size())
			return nullptr;
		// HACK
		if (name == "Skybox")
			return get_gpu_texture(static_cast<Texture*>(textures[index])->texture_asset, yar_texture_type_cube_map);
		if (index == 1 || index == 2)
			return get_gpu_texture(static_cast<Texture*>(textures[index])->texture_asset, yar_texture_type_2d, 1);
		else if (index == 3)
			return get_gpu_texture(static_cast<Texture*>(textures[index])->texture_asset, yar_texture_type_2d, 3);

		return get_gpu_texture(static_cast<Texture*>(textures[index])->texture_asset, yar_texture_type_2d);
	}

private:
	void optimize_mesh()
	{
		uint32_t vertex_size = sizeof(Vertex);
		uint32_t index_count = indices.size();
		uint32_t vertex_count = vertices.size();

		// 1. Indexing (generate remap table from vertex and index data)
		std::vector<uint32_t> remap(vertex_count); // temporary remap table
		size_t opt_vertex_count = meshopt_generateVertexRemap(&remap[0], indices.data(), index_count,
			vertices.data(), vertex_count, vertex_size);

		std::vector<uint32_t> opt_indices(index_count);
		std::vector<Vertex> opt_vertices(opt_vertex_count);

		// 2. Remove duplicate vertices
		meshopt_remapIndexBuffer(opt_indices.data(), indices.data(), index_count, remap.data());
		meshopt_remapVertexBuffer(opt_vertices.data(), vertices.data(), vertex_count, vertex_size, remap.data());

		// 3. Optimize vertex cache
		meshopt_optimizeVertexCache(opt_indices.data(), opt_indices.data(), index_count, opt_vertex_count);

		// 4. Overdraw optimization
		meshopt_optimizeOverdraw(opt_indices.data(), opt_indices.data(), index_count, &(opt_vertices[0].position.x),
			opt_vertex_count, vertex_size, 1.05f);

		// 5. Vertex fetch optimization
		meshopt_optimizeVertexFetch(opt_vertices.data(), opt_indices.data(), index_count, opt_vertices.data(),
			opt_vertex_count, vertex_size);

		vertices = opt_vertices;
		indices = opt_indices;
	}

	void upload_buffers()
	{
		yar_buffer_desc buffer_desc;
		uint32_t vertexes_size = vertices.size() * sizeof(Vertex);
		buffer_desc.size = vertexes_size;
		buffer_desc.flags = yar_buffer_flag_gpu_only;
		buffer_desc.name = "vertex_buffer";
		add_buffer(&buffer_desc, &gpu_vertex_buffer);

		uint64_t indexes_size = indices.size() * sizeof(uint32_t);
		buffer_desc.size = indexes_size;
		buffer_desc.name = "index_buffer";
		add_buffer(&buffer_desc, &gpu_index_buffer);

		yar_resource_update_desc resource_update_desc;
		{ // update buffers data
			yar_buffer_update_desc update_desc{};
			resource_update_desc = &update_desc;
			update_desc.buffer = gpu_vertex_buffer;
			update_desc.size = vertexes_size;
			begin_update_resource(resource_update_desc);
			std::memcpy(update_desc.mapped_data, vertices.data(), vertexes_size);
			end_update_resource(resource_update_desc);

			update_desc.buffer = gpu_index_buffer;
			update_desc.size = indexes_size;
			begin_update_resource(resource_update_desc);
			std::memcpy(update_desc.mapped_data, indices.data(), indexes_size);
			end_update_resource(resource_update_desc);
		}
	}

private:
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Texture*> textures;

	yar_buffer* gpu_vertex_buffer;
	yar_buffer* gpu_index_buffer;

	std::string name;

	const uint32_t attrib_count = 6u;
};

class Model
{
public:
	Model(const std::string_view& path)
	{
		Assimp::Importer imp;
		const aiScene * scene = imp.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_FlipUVs);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cout << "Error [assimp]:" << imp.GetErrorString() << std::endl;
			return;
		}
		directory = path.substr(0, path.find_last_of('/'));

		process_node(scene->mRootNode, scene, glm::mat4(1.0f));
	}

	void setup_descriptor_set(yar_shader* shader, yar_descriptor_set_update_frequency update_freq, yar_sampler* sampler)
	{
		yar_descriptor_set_desc set_desc;
		set_desc.max_sets = meshes.size();
		set_desc.shader = shader;
		set_desc.update_freq = update_freq;
		add_descriptor_set(&set_desc, &set);

		uint32_t index = 0;
		for (auto& mesh : meshes)
		{
			// It is incorrect we can't garauntee that 
			// 0th texture is diffuse_map,
			// 1st texture is specular_map, etc
			// TODO: store info about textures inside Mesh
			std::vector<yar_descriptor_info> infos{
				{
					.name = "diffuse_map",
					.descriptor =
					yar_descriptor_info::yar_combined_texture_sample{
						mesh.get_texture(0),
						"samplerState",
					}
				},
				{
					.name = "roughness_map",
					.descriptor =
					yar_descriptor_info::yar_combined_texture_sample{
						mesh.get_texture(1),
						"samplerState",
					}
				},
				{
					.name = "metalness_map",
					.descriptor =
					yar_descriptor_info::yar_combined_texture_sample{
						mesh.get_texture(2),
						"samplerState",
					}
				},
				{
					.name = "normal_map",
					.descriptor =
					yar_descriptor_info::yar_combined_texture_sample{
						mesh.get_texture(3),
						"samplerState",
					}
				},
				{
					.name = "samplerState",
					.descriptor = sampler
				},
			};

			yar_update_descriptor_set_desc update_set_desc = {};
			update_set_desc.index = index;
			update_set_desc.infos = std::move(infos);
			update_descriptor_set(&update_set_desc, set);
			++index;
		}
	}

	void setup_vertex_layout(yar_vertex_layout& layout)
	{
		meshes[0].setup_vertex_layout(layout);
	}

	void draw(yar_cmd_buffer* cmd, bool shadow_map = false)
	{
		for (uint32_t i = 0; i < meshes.size(); ++i)
		{
			if (!shadow_map)
				cmd_bind_descriptor_set(cmd, set, i);
			meshes[i].draw(cmd);
		}
	}

private:
	void process_node(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform)
	{
		glm::mat4 localTransform = glm::transpose(glm::make_mat4(&node->mTransformation.a1));
		glm::mat4 globalTransform = parentTransform * localTransform;

		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.push_back(process_mesh(mesh, scene, globalTransform));
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			process_node(node->mChildren[i], scene, globalTransform);
		}
	}

	Mesh process_mesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<Texture*> textures;

		for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
		{
			Vertex vertex{};

			glm::vec4 pos = transform * glm::vec4(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
			vertex.position = glm::vec3(pos);

			if (mesh->HasNormals())
			{
				glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(transform)));
				glm::vec3 norm = normalMat * glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
				vertex.normal = glm::normalize(norm);
			}

			if (mesh->HasTextureCoords(0))
			{
				vertex.tex_coord.x = mesh->mTextureCoords[0][i].x;
				vertex.tex_coord.y = mesh->mTextureCoords[0][i].y;
			}

			if (mesh->HasTextureCoords(1))
			{
				vertex.tex_coord1.x = mesh->mTextureCoords[1][i].x;
				vertex.tex_coord1.y = mesh->mTextureCoords[1][i].y;
			}

			if (mesh->HasTangentsAndBitangents())
			{
				vertex.tangent.x = mesh->mTangents[i].x;
				vertex.tangent.y = mesh->mTangents[i].y;
				vertex.tangent.z = mesh->mTangents[i].z;
				vertex.bitangent.x = mesh->mBitangents[i].x;
				vertex.bitangent.y = mesh->mBitangents[i].y;
				vertex.bitangent.z = mesh->mBitangents[i].z;
			}

			vertices.push_back(vertex);
		}

		for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
		{
			aiFace face = mesh->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; ++j)
				indices.push_back(face.mIndices[j]);
		}
		
		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			std::vector<Texture*> diffuse_maps = load_material_textures(material,
				aiTextureType_DIFFUSE);
			textures.insert(textures.end(), diffuse_maps.begin(), diffuse_maps.end());
			std::vector<Texture*> roughness_maps = load_material_textures(material,
				aiTextureType_DIFFUSE_ROUGHNESS);
			textures.insert(textures.end(), roughness_maps.begin(), roughness_maps.end());
			std::vector<Texture*> metalness_maps = load_material_textures(material,
				aiTextureType_METALNESS);
			textures.insert(textures.end(), metalness_maps.begin(), metalness_maps.end());
			std::vector<Texture*> normal_maps = load_material_textures(material,
				aiTextureType_NORMALS);
			textures.insert(textures.end(), normal_maps.begin(), normal_maps.end());
		}

		return Mesh(std::move(vertices), std::move(indices), std::move(textures), mesh->mName.C_Str());
	}


	std::vector<Texture*> load_material_textures(aiMaterial* mat, aiTextureType type)
	{
		std::vector<Texture*> textures;
		for (uint32_t i = 0; i < mat->GetTextureCount(type); ++i)
		{
			aiString str;
			mat->GetTexture(type, i, &str);
			std::string path = directory + '/' + str.C_Str();
			
			Texture* tex = new Texture();
			tex->texture_asset = load_texture(path);
			textures.push_back(std::move(tex));
		}

		if (textures.empty())
		{
			Texture* tex = new Texture();
			tex->texture_asset = load_texture(WHITE_TEXTURE);
			textures.push_back(std::move(tex));
		}

		return textures;
	}

private:
	std::string directory;
	std::vector<Mesh> meshes;

	yar_descriptor_set* set;

	std::unordered_map<std::string, yar_texture*> loaded_textures;
};

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
} material[11];

static constexpr uint32_t kDirLightCount = 1;
static constexpr uint32_t kPointLightCount = 1;
static constexpr uint32_t kSpotLightCount = 1;
static constexpr uint64_t kLightSourcesCount = kDirLightCount + kPointLightCount +kSpotLightCount;

struct DirectLight
{
	glm::vec4 direction[kDirLightCount];
};

struct PointLight
{
	glm::vec4 position[kPointLightCount];
};

struct SpotLight
{
	glm::vec4 position[kSpotLightCount];
	glm::vec4 direction[kSpotLightCount];
	// not cool to use vec4 here
	glm::vec4 cutoff[kSpotLightCount];
	glm::vec4 attenuation[kPointLightCount];
};

struct LightParams
{
	glm::vec4 color[kLightSourcesCount];
};

struct UBO
{
	glm::mat4 view;
	glm::mat4 view_sb;
	glm::mat4 proj;
	glm::mat4 light_space;
	glm::mat4 ui_ortho;
	glm::mat4 model[11];
	DirectLight dir_light;
	PointLight point_light;
	SpotLight spot_light;
	LightParams light_params;
	glm::vec4 view_pos;
}ubo;

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

static float dir_light_distance = 15.0f; // base good value for current scene

static std::function<void()> app_layer = []()
	{

		ImGui::Begin("Light Settings");

		static float dir[3] = {
			ubo.dir_light.direction[0].x,
			ubo.dir_light.direction[0].y,
			ubo.dir_light.direction[0].z
		};
		if (ImGui::SliderFloat3("Direction Light", dir, -1.0f, 1.0f))
		{
			glm::vec3 lightDir = glm::vec3(dir[0], dir[1], dir[2]);
			ubo.dir_light.direction[0] = glm::vec4(lightDir, 0.0f);
		}

		ImGui::SliderFloat("Direction Light Distance", &dir_light_distance, 0.0f, 100.0f);

		static bool spot_light_enabled = true;
		ImGui::Checkbox("Enable Spot Light", &spot_light_enabled);
		{
			if (spot_light_enabled)
				ubo.light_params.color[2].a = 1.0f;
			else
				ubo.light_params.color[2].a = 0.0f;
		}

		ImGui::End();
	};

auto main() -> int {
	init_window(app_layer);
	
	init_asset_manager();
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

	yar_render_target_desc depth_buffer_desc{};
	depth_buffer_desc.format = yar_texture_format_depth32f;
	depth_buffer_desc.height = h;
	depth_buffer_desc.width = w;
	depth_buffer_desc.type = yar_texture_type_2d;
	depth_buffer_desc.usage = yar_texture_usage_depth_stencil;
	yar_render_target* depth_buffer;
	add_render_target(&depth_buffer_desc, &depth_buffer);

	uint32_t shadow_map_dims = 1024;
	depth_buffer_desc.height = shadow_map_dims;
	depth_buffer_desc.width = shadow_map_dims;
	depth_buffer_desc.usage = yar_texture_usage_shader_resource;
	depth_buffer_desc.format = yar_texture_format_depth32f;
	depth_buffer_desc.type = yar_texture_type_2d;
	depth_buffer_desc.mip_levels = 1;
	yar_render_target* shadow_map_target;
	add_render_target(&depth_buffer_desc, &shadow_map_target);

	yar_render_target_desc imgui_rt_desc{};
	imgui_rt_desc.format = yar_texture_format_srgb8;
	imgui_rt_desc.height = h;
	imgui_rt_desc.width = w;
	imgui_rt_desc.type = yar_texture_type_2d;
	imgui_rt_desc.usage = yar_texture_usage_render_target;
	imgui_rt_desc.mip_levels = 1;
	yar_render_target* imgui_rt;
	add_render_target(&imgui_rt_desc, &imgui_rt);

	auto cube_vertexes = std::vector<Vertex>{
		// Front face
		Vertex{{-0.5f, -0.5f,  0.5f}, {0,0,1}, {1,0,0}, {0,1,0}, {0,0},   {0,0}},
		Vertex{{ 0.5f, -0.5f,  0.5f}, {0,0,1}, {1,0,0}, {0,1,0}, { 1,0 }, {1,0}},
		Vertex{{ 0.5f,  0.5f,  0.5f}, {0,0,1}, {1,0,0}, {0,1,0}, { 1,1 }, {1,1}},
		Vertex{{-0.5f,  0.5f,  0.5f}, {0,0,1}, {1,0,0}, {0,1,0}, { 0,1 }, {0,1}},

		// Back face
		Vertex{{ 0.5f, -0.5f, -0.5f}, {0,0,-1}, {-1,0,0}, {0,1,0}, {1,0}, {1,0}},
		Vertex{{-0.5f, -0.5f, -0.5f}, {0,0,-1}, {-1,0,0}, {0,1,0}, {0,0}, {0,0}},
		Vertex{{-0.5f,  0.5f, -0.5f}, {0,0,-1}, {-1,0,0}, {0,1,0}, {0,1}, {0,1}},
		Vertex{{ 0.5f,  0.5f, -0.5f}, {0,0,-1}, {-1,0,0}, {0,1,0}, {1,1}, {1,1}},

		// Left face
		Vertex{{-0.5f, -0.5f, -0.5f}, {-1,0,0}, {0,0,-1}, {0,1,0}, {0,0}, {0,0}},
		Vertex{{-0.5f, -0.5f,  0.5f}, {-1,0,0}, {0,0,-1}, {0,1,0}, {1,0}, {1,0}},
		Vertex{{-0.5f,  0.5f,  0.5f}, {-1,0,0}, {0,0,-1}, {0,1,0}, {1,1}, {1,1}},
		Vertex{{-0.5f,  0.5f, -0.5f}, {-1,0,0}, {0,0,-1}, {0,1,0}, {0,1}, {0,1}},

		// Right face
		Vertex{{ 0.5f, -0.5f,  0.5f}, {1,0,0}, {0,0,1}, {0,1,0}, {0,0}, {0,0}},
		Vertex{{ 0.5f, -0.5f, -0.5f}, {1,0,0}, {0,0,1}, {0,1,0}, {1,0}, {1,0}},
		Vertex{{ 0.5f,  0.5f, -0.5f}, {1,0,0}, {0,0,1}, {0,1,0}, {1,1}, {1,1}},
		Vertex{{ 0.5f,  0.5f,  0.5f}, {1,0,0}, {0,0,1}, {0,1,0}, {0,1}, {0,1}},

		// Bottom face
		Vertex{{-0.5f, -0.5f, -0.5f}, {0,-1,0}, {1,0,0}, {0,0,1}, {0,1}, {0,1}},
		Vertex{{ 0.5f, -0.5f, -0.5f}, {0,-1,0}, {1,0,0}, {0,0,1}, {1,1}, {1,1}},
		Vertex{{ 0.5f, -0.5f,  0.5f}, {0,-1,0}, {1,0,0}, {0,0,1}, {1,0}, {1,0}},
		Vertex{{-0.5f, -0.5f,  0.5f}, {0,-1,0}, {1,0,0}, {0,0,1}, {0,0}, {0,0}},

		// Top face
		Vertex{{-0.5f,  0.5f,  0.5f}, {0,1,0}, {1,0,0}, {0,0,-1}, {0,1}, {0,1}},
		Vertex{{ 0.5f,  0.5f,  0.5f}, {0,1,0}, {1,0,0}, {0,0,-1}, {1,1}, {1,1}},
		Vertex{{ 0.5f,  0.5f, -0.5f}, {0,1,0}, {1,0,0}, {0,0,-1}, {1,0}, {1,0}},
		Vertex{{-0.5f,  0.5f, -0.5f}, {0,1,0}, {1,0,0}, {0,0,-1}, {0,0}, {0,0}},
	};


	std::vector<uint32_t> indexes = {
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
		glm::vec4(0.0f,  0.0f,  -5.0f  , 1.0f),
		glm::vec4(2.0f,  5.0f, 0.0f , 1.0f),
		glm::vec4(-1.5f, 10.2f, 0.0f , 1.0f),
		glm::vec4(-3.8f, 7.0f, 0.3f, 1.0f),
		glm::vec4(2.4f, 9.4f, 0.0f  , 1.0f),
		glm::vec4(-1.7f,  3.0f, -7.5f , 1.0f),
		glm::vec4(1.3f, 3.0f, -2.5f  , 1.0f),
		glm::vec4(1.5f,  2.0f, -2.5f  , 1.0f),
		glm::vec4(1.5f,  3.2f, -1.5f  , 1.0f),
		glm::vec4(-1.3f,  3.0f, -1.5f , 1.0f)
	};

	glm::vec4 backpack_pos(0.0f);

	
	auto skybox_vertexes = std::vector{
		// Front face
		Vertex{{glm::vec3(-1.0f, -1.0f,  1.0f)}},
		Vertex{{glm::vec3(1.0f, -1.0f,  1.0f)}},
		Vertex{{glm::vec3(1.0f,  1.0f,  1.0f)}},
		Vertex{{glm::vec3(-1.0f,  1.0f,  1.0f)}},

		// Back face
		Vertex{{glm::vec3(1.0f, -1.0f, -1.0f)}},
		Vertex{{glm::vec3(-1.0f, -1.0f, -1.0f)}},
		Vertex{{glm::vec3(-1.0f,  1.0f, -1.0f)}},
		Vertex{{glm::vec3(1.0f,  1.0f, -1.0f)}},

		// Left face
		Vertex{{glm::vec3(-1.0f, -1.0f, -1.0f)}},
		Vertex{{glm::vec3(-1.0f, -1.0f,  1.0f)}},
		Vertex{{glm::vec3(-1.0f,  1.0f,  1.0f)}},
		Vertex{{glm::vec3(-1.0f,  1.0f, -1.0f)}},

		// Right face
		Vertex{{glm::vec3(1.0f, -1.0f,  1.0f)}},
		Vertex{{glm::vec3(1.0f, -1.0f, -1.0f)}},
		Vertex{{glm::vec3(1.0f,  1.0f, -1.0f)}},
		Vertex{{glm::vec3(1.0f,  1.0f,  1.0f)}},

		// Bottom face
		Vertex{{glm::vec3(-1.0f, -1.0f, -1.0f)}},
		Vertex{{glm::vec3(1.0f, -1.0f, -1.0f)}},
		Vertex{{glm::vec3(1.0f, -1.0f,  1.0f)}},
		Vertex{{glm::vec3(-1.0f, -1.0f,  1.0f)}},

		// Top face
		Vertex{{glm::vec3(-1.0f,  1.0f,  1.0f)}},
		Vertex{{glm::vec3(1.0f,  1.0f,  1.0f)}},
		Vertex{{glm::vec3(1.0f,  1.0f, -1.0f)}},
		Vertex{{glm::vec3(-1.0f,  1.0f, -1.0f)}},
	};

	std::vector<uint32_t> skybox_indices = {
		// Front face (z+)
		0, 2, 1,
		2, 0, 3,

		// Back face (z-)
		4, 6, 5,
		6, 4, 7,

		// Left face (x-)
		8, 10, 9,
		10, 8, 11,

		// Right face (x+)
		12, 14, 13,
		14, 12, 15,

		// Bottom face (y-)
		16, 18, 17,
		18, 16, 19,

		// Top face (y+)
		20, 22, 21,
		22, 20, 23
	};

	light_pos = &cube_positions[0];

	// Point Light
	memset(&ubo, 0x00, sizeof(ubo));

	// Dir Light
	ubo.dir_light.direction[0] = glm::vec4(-0.01f, -1.0f, 0.0f, 0.0f);
	ubo.light_params.color[0] = glm::vec4(1.0f, 0.95f, 0.9f, 0.0f);
	ubo.light_params.color[0].a = 1.5f;  // intensity

	// Point Light
	ubo.point_light.position[0] = *light_pos; 
	ubo.light_params.color[1] = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
	ubo.light_params.color[1].a = 1.0f; // intensity

	// Spotlight
	ubo.spot_light.cutoff[0] = glm::vec4(
		glm::cos(glm::radians(12.5f)), 
		glm::cos(glm::radians(17.5f)),
		0.0f, 0.0f
	);
	ubo.spot_light.attenuation[0] = glm::vec4(1.0f, 0.007f, 0.0002f, 0.0f);
	ubo.light_params.color[2] = glm::vec4(1.0f, 0.0f, 1.0f, 0.0f);
	ubo.light_params.color[2].a = 1.0f; // intensity

	Texture diffuse_map_tex{
		.texture_asset = load_texture("assets/cube_albedo.png")
	};
	Texture roughness_tex{ 
		.texture_asset = load_texture("assets/cube_roughness.png")
	};
	Texture metalness_tex{
		.texture_asset = load_texture("assets/cube_metallic.png")
	};
	Texture normal_map_tex{
		.texture_asset = load_texture("assets/cube_normal.png")
	};

	// TODO: remake this stupid thing
	std::array<std::string, 6> paths = {
		"assets/pz.png",
		"assets/nz.png",
		"assets/py.png",
		"assets/ny.png",
		"assets/px.png",
		"assets/nx.png",
	};
	std::array<std::string_view, 6> cubemap_paths;
	for (int i = 0; i < 6; ++i)
		cubemap_paths[i] = paths[i];

	Texture skybox {
		.texture_asset = load_cubemap(cubemap_paths)
	};
	
	std::vector<Texture*> textures = {
		&diffuse_map_tex,
		&roughness_tex,
		&metalness_tex,
		&normal_map_tex
	};

	std::vector<Texture*> skybox_textures = {
		&skybox
	};

	yar_texture* imgui_fonts = get_imgui_fonts();

	Mesh test_mesh(std::move(cube_vertexes), std::move(indexes), std::move(textures), "Cube");
	Mesh skybox_mesh(std::move(skybox_vertexes), std::move(skybox_indices), std::move(skybox_textures), "Skybox");
	Model mdl("assets/sponza/sponza.gltf");

	yar_sampler* sampler;
	yar_sampler_desc sampler_desc{};
	sampler_desc.min_filter = yar_filter_type_linear;
	sampler_desc.mag_filter = yar_filter_type_linear;
	sampler_desc.wrap_u = yar_wrap_mode_repeat;
	sampler_desc.wrap_v = yar_wrap_mode_repeat;
	sampler_desc.mip_map_filter = yar_filter_type_linear;
	add_sampler(&sampler_desc, &sampler);

	yar_sampler* sm_sampler;
	sampler_desc.min_filter = yar_filter_type_nearest;
	sampler_desc.mag_filter = yar_filter_type_nearest;
	sampler_desc.wrap_u = yar_wrap_mode_clamp_border;
	sampler_desc.wrap_v = yar_wrap_mode_clamp_border;
	add_sampler(&sampler_desc, &sm_sampler);

	yar_sampler* skybox_sampler;
	sampler_desc.min_filter = yar_filter_type_linear;
	sampler_desc.mag_filter = yar_filter_type_linear;
	sampler_desc.wrap_u = yar_wrap_mode_clamp_edge;
	sampler_desc.wrap_v = yar_wrap_mode_clamp_edge;
	sampler_desc.wrap_w = yar_wrap_mode_clamp_edge;
	sampler_desc.mip_map_filter = yar_filter_type_linear;
	add_sampler(&sampler_desc, &skybox_sampler);

	constexpr uint32_t image_count = 2;
	uint32_t frame_index = 0; 

	yar_buffer_desc buffer_desc;
	buffer_desc.size = sizeof(ubo);
	buffer_desc.flags = yar_buffer_flag_map_write;
	buffer_desc.name = "UBO";
	yar_buffer* ubo_buf[image_count] = { nullptr, nullptr };
	for (auto& buf : ubo_buf)
		add_buffer(&buffer_desc, &buf);

	// Need to make something with shaders path 
	yar_shader_load_desc shader_load_desc = {};
	shader_load_desc.stages[0] = { "shaders/base_vert.hlsl", "main", yar_shader_stage::yar_shader_stage_vert };
	shader_load_desc.stages[1] = { "shaders/base_frag.hlsl", "main", yar_shader_stage::yar_shader_stage_pixel };
	yar_shader_desc* shader_desc = nullptr;
	load_shader(&shader_load_desc, &shader_desc);
	yar_shader* shader;
	add_shader(shader_desc, &shader);

	// Fix memory leak but need to change logic here on load_shader
	std::free(shader_desc);
	shader_desc = nullptr;

	shader_load_desc.stages[0] = { "shaders/skybox_vert.hlsl", "main", yar_shader_stage::yar_shader_stage_vert };
	shader_load_desc.stages[1] = { "shaders/skybox_pix.hlsl", "main", yar_shader_stage::yar_shader_stage_pixel };
	load_shader(&shader_load_desc, &shader_desc);
	yar_shader* skybox_shader;
	add_shader(shader_desc, &skybox_shader);
	std::free(shader_desc);
	shader_desc = nullptr;

	shader_load_desc.stages[0] = { "shaders/shadow_map_vert.hlsl", "main", yar_shader_stage::yar_shader_stage_vert };
	shader_load_desc.stages[1] = {  };
	load_shader(&shader_load_desc, &shader_desc);
	yar_shader* shadow_map_shader;
	add_shader(shader_desc, &shadow_map_shader);
	std::free(shader_desc);
	shader_desc = nullptr;

	shader_load_desc.stages[0] = { "shaders/imgui_vert.hlsl", "main", yar_shader_stage::yar_shader_stage_vert };
	shader_load_desc.stages[1] = { "shaders/imgui_pix.hlsl", "main", yar_shader_stage::yar_shader_stage_pixel };
	load_shader(&shader_load_desc, &shader_desc);
	yar_shader* imgui_shader;
	add_shader(shader_desc, &imgui_shader);
	std::free(shader_desc);
	shader_desc = nullptr;
	 
	yar_vertex_layout layout{};
	yar_depth_stencil_state depth_stencil{};
	mdl.setup_vertex_layout(layout);
	depth_stencil.depth_enable = true;
	depth_stencil.depth_write = true;
	depth_stencil.depth_func = yar_depth_stencil_func_less;

	yar_rasterizer_state raster{};
	raster.fill_mode = yar_fill_mode_solid;
	raster.cull_mode = yar_cull_mode_back;
	raster.front_counter_clockwise = true;

	yar_descriptor_set_desc set_desc;
	set_desc.max_sets = image_count;
	set_desc.shader = shader;
	set_desc.update_freq = yar_update_freq_per_frame;
	yar_descriptor_set* ubo_desc;
	add_descriptor_set(&set_desc, &ubo_desc);

	set_desc.max_sets = 1;
	yar_descriptor_set* shadow_map_ds_desc;
	add_descriptor_set(&set_desc, &shadow_map_ds_desc);

	yar_update_descriptor_set_desc update_set_desc{};
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
	yar_descriptor_set* texture_set;
	add_descriptor_set(&set_desc, &texture_set);

	std::vector<yar_descriptor_info> infos{
		{
			.name = "diffuse_map",
			.descriptor =
			yar_descriptor_info::yar_combined_texture_sample{
				test_mesh.get_texture(0),
				"samplerState",
			}
		},
		{
			.name = "roughness_map",
			.descriptor =
			yar_descriptor_info::yar_combined_texture_sample{
				test_mesh.get_texture(1),
				"samplerState",
			}
		},
		{
			.name = "metalness_map",
			.descriptor =
			yar_descriptor_info::yar_combined_texture_sample{
				test_mesh.get_texture(2),
				"samplerState",
			}
		},
		{
			.name = "normal_map",
			.descriptor =
			yar_descriptor_info::yar_combined_texture_sample{
				test_mesh.get_texture(3),
				"samplerState",
			}
		},
		{
			.name = "samplerState",
			.descriptor = sampler
		}
	};

	update_set_desc = {};
	update_set_desc.index = 0;
	update_set_desc.infos = std::move(infos);
	update_descriptor_set(&update_set_desc, texture_set);


	infos = {
		{
			.name = "shadow_map", 
			.descriptor =
			yar_descriptor_info::yar_combined_texture_sample{
				shadow_map_target->texture,
				"smSampler",
			}
		},
		{
			.name = "smSampler",
			.descriptor = sm_sampler
		}
	};
	update_set_desc.infos = std::move(infos);
	update_descriptor_set(&update_set_desc, shadow_map_ds_desc);

	set_desc.max_sets = 1;
	set_desc.update_freq = yar_update_freq_none;
	set_desc.shader = skybox_shader;
	yar_descriptor_set* skybox_texture_set;
	add_descriptor_set(&set_desc, &skybox_texture_set);

	std::vector<yar_descriptor_info> skybox_infos{
		{
			.name = "skybox",
			.descriptor =
			yar_descriptor_info::yar_combined_texture_sample{
				skybox_mesh.get_texture(0),
				"samplerState",
			}
		},
		{
			.name = "samplerState",
			.descriptor = skybox_sampler
		}
	};

	update_set_desc.infos = std::move(skybox_infos);
	update_descriptor_set(&update_set_desc, skybox_texture_set);

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
			.descriptor = skybox_sampler
		}
	};
	update_set_desc.infos = std::move(imgui_font_info);
	update_descriptor_set(&update_set_desc, imgui_set);

	mdl.setup_descriptor_set(shader, yar_update_freq_none, sampler);

	yar_pipeline_desc pipeline_desc{};
	pipeline_desc.shader = shader;
	pipeline_desc.vertex_layout = layout;
	pipeline_desc.depth_stencil_state = depth_stencil;
	pipeline_desc.rasterizer_state = raster;
	pipeline_desc.topology = yar_primitive_topology_triangle_list;

	yar_pipeline* graphics_pipeline;
	add_pipeline(&pipeline_desc, &graphics_pipeline);

	yar_vertex_layout skybox_layout{};
	skybox_layout.attrib_count = 1u;
	skybox_layout.attribs[0].size = 3u;
	skybox_layout.attribs[0].format = yar_attrib_format_float;
	skybox_layout.attribs[0].offset = offsetof(SkyBoxVertex, position);
	pipeline_desc.shader = skybox_shader;
	pipeline_desc.vertex_layout = skybox_layout;
	pipeline_desc.depth_stencil_state.depth_enable = true;
	pipeline_desc.depth_stencil_state.depth_write = false;
	pipeline_desc.depth_stencil_state.depth_func = yar_depth_stencil_func_less_equal;
	pipeline_desc.depth_stencil_state.stencil_enable = false;
	pipeline_desc.blend_state.blend_enable = false;
	pipeline_desc.rasterizer_state.cull_mode = yar_cull_mode_back;
	yar_pipeline* skybox_pipeline;
	add_pipeline(&pipeline_desc, &skybox_pipeline);

	yar_vertex_layout shadow_map_layout{};
	shadow_map_layout.attrib_count = 1u;
	shadow_map_layout.attribs[0].size = 3u;
	shadow_map_layout.attribs[0].format = yar_attrib_format_float;
	shadow_map_layout.attribs[0].offset = offsetof(Vertex, position);
	pipeline_desc.shader = shadow_map_shader;
	pipeline_desc.vertex_layout = shadow_map_layout;
	pipeline_desc.depth_stencil_state.depth_enable = true;
	pipeline_desc.depth_stencil_state.depth_write = true;
	pipeline_desc.depth_stencil_state.depth_func = yar_depth_stencil_func_less_equal;
	pipeline_desc.depth_stencil_state.stencil_enable = false;
	pipeline_desc.rasterizer_state.cull_mode = yar_cull_mode_front;
	yar_pipeline* shadow_map_pipeline;
	add_pipeline(&pipeline_desc, &shadow_map_pipeline);

	yar_vertex_layout imgui_layout{};
	imgui_layout.attrib_count = 3;
	imgui_layout.attribs[0] = { .size = 2, .format = yar_attrib_format_float, .offset = offsetof(ImGuiVertex, position) };
	imgui_layout.attribs[1] = { .size = 2, .format = yar_attrib_format_float, .offset = offsetof(ImGuiVertex, uv) };
	imgui_layout.attribs[2] = { .size = 4, .format = yar_attrib_format_ubyte, .offset = offsetof(ImGuiVertex, color) };

	pipeline_desc.shader = imgui_shader;
	pipeline_desc.vertex_layout = imgui_layout;
	pipeline_desc.topology = yar_primitive_topology_triangle_list;

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

	yar_cmd_queue_desc queue_desc;
	yar_cmd_queue* queue;
	add_queue(&queue_desc, &queue);

	yar_push_constant_desc pc_desc{};
	pc_desc.name = "push_constant";
	pc_desc.shader = shader;
	pc_desc.size = sizeof(uint32_t) * 4;
	yar_cmd_buffer_desc cmd_desc;
	cmd_desc.current_queue = queue;
	cmd_desc.use_push_constant = true;
	cmd_desc.pc_desc = &pc_desc;
	yar_cmd_buffer* cmd;
	add_cmd(&cmd_desc, &cmd);

	glm::mat4 model = glm::mat4(1.0f);
	
	float max_cube_scale = 0.5f;
	glm::vec3 cube_scales[] = {
		glm::vec3(glm::linearRand(0.08f, max_cube_scale)),
		glm::vec3(glm::linearRand(0.08f, max_cube_scale)),
		glm::vec3(glm::linearRand(0.08f, max_cube_scale)),
		glm::vec3(glm::linearRand(0.08f, max_cube_scale)),
		glm::vec3(glm::linearRand(0.08f, max_cube_scale)),
		glm::vec3(glm::linearRand(0.08f, max_cube_scale)),
		glm::vec3(glm::linearRand(0.08f, max_cube_scale)),
		glm::vec3(glm::linearRand(0.08f, max_cube_scale)),
		glm::vec3(glm::linearRand(0.08f, max_cube_scale))
	};

	float near_plane = 0.1f;
	float far_plane = 100.0f;
	glm::mat4 light_projection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);

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
	
	while(update_window())
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

		glm::vec3 light_dir = glm::vec3(ubo.dir_light.direction->x, ubo.dir_light.direction->y, ubo.dir_light.direction->z);
		glm::mat4 dir_light_view = glm::lookAt(
			-light_dir * dir_light_distance,
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)
		); 
		glm::mat4 light_space_mat = light_projection * dir_light_view;
		ubo.light_space = light_space_mat;

		for (uint32_t i = 0; i < 10; ++i)
		{
			model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(cube_positions[i]));
			if (i != 0) // i == 0 light position
			{
				float angle = 20.0f;// *(i + 1);
				model = glm::rotate(model, (float)glfwGetTime() * glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
				model = glm::scale(model, cube_scales[i - 1]);
			}
			else
			{
				model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
			}
			ubo.model[i] = model;
		}

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(backpack_pos));
		//model = glm::scale(model, glm::vec3(0.01f, 0.01f, 0.01f));
		ubo.model[10] = model;

		ubo.point_light.position[0] = *light_pos; // update point light pos
		ubo.view_pos = glm::vec4(camera.pos, 0.0f);
		ubo.view = glm::mat4(1.0f);
		ubo.view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);
		ubo.view_sb = glm::mat4(glm::mat3(ubo.view));
		ubo.proj = glm::perspective(glm::radians(fov), 1920.0f / 1080.0f, 0.1f, 100.0f);
		ubo.spot_light.position[0] = ubo.view_pos;
		ubo.spot_light.direction[0] = glm::vec4(camera.front, 0.0f);
		ubo.ui_ortho = ortho;

		yar_resource_update_desc resource_update_desc;
		yar_buffer_update_desc update;
		update.buffer = ubo_buf[frame_index];
		update.size = sizeof(ubo);
		resource_update_desc = &update;
		begin_update_resource(resource_update_desc);
		std::memcpy(update.mapped_data, &ubo, sizeof(ubo));
		end_update_resource(resource_update_desc);

		uint32_t sc_image;
		acquire_next_image(swapchain, sc_image);

		yar_render_pass_desc shadow_map_pass_desc{};
		shadow_map_pass_desc.color_attachment_count = 0;
		shadow_map_pass_desc.depth_stencil_attachment.target = shadow_map_target;
		cmd_begin_render_pass(cmd, &shadow_map_pass_desc);
		{
			cmd_set_viewport(cmd, shadow_map_dims, shadow_map_dims);
			cmd_bind_descriptor_set(cmd, ubo_desc, frame_index);
			cmd_bind_pipeline(cmd, shadow_map_pipeline);
			for (uint32_t i = 0; i < 10; ++i)
			{
				cmd_bind_push_constant(cmd, &i);
				test_mesh.draw(cmd);
			}

			uint32_t index = 10;
			cmd_bind_push_constant(cmd, &index);
			mdl.draw(cmd, true);
		}
		cmd_end_render_pass(cmd);

		yar_render_pass_desc pass_desc{};
		pass_desc.color_attachment_count = 1;
		pass_desc.color_attachments[0].target = swapchain->render_targets[sc_image];
		pass_desc.depth_stencil_attachment.target = depth_buffer;

		cmd_begin_render_pass(cmd, &pass_desc);
		
		cmd_set_viewport(cmd, 1920, 1080);
		cmd_bind_pipeline(cmd, graphics_pipeline);
		cmd_bind_descriptor_set(cmd, ubo_desc, frame_index);
		cmd_bind_descriptor_set(cmd, shadow_map_ds_desc, 0);

		for (uint32_t i = 0; i < 10; ++i)
		{
			cmd_bind_descriptor_set(cmd, texture_set, 0);
			cmd_bind_push_constant(cmd, &i);
			test_mesh.draw(cmd);
		}

		uint32_t index = 10;
		cmd_bind_push_constant(cmd, &index);
		mdl.draw(cmd);

		cmd_bind_pipeline(cmd, skybox_pipeline);
		cmd_bind_descriptor_set(cmd, skybox_texture_set, 0);
		skybox_mesh.draw(cmd);

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
				ImVec2 clip_min((draw_cmd->ClipRect.x - clip_off.x)* clip_scale.x, (draw_cmd->ClipRect.y - clip_off.y)* clip_scale.y);
				ImVec2 clip_max((draw_cmd->ClipRect.z - clip_off.x)* clip_scale.x, (draw_cmd->ClipRect.w - clip_off.y)* clip_scale.y);
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

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	fov -= (float)yoffset;
	if (fov < 1.0f)
		fov = 1.0f;
	if (fov > 45.0f)
		fov = 45.0f;
}
