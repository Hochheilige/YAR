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

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <optional>

Texture* load_white_texture()
{
	int32_t channels = 4;
	size_t size = channels;
	std::vector<uint8_t> white_data(size, 255);

	Texture* tex;
	TextureDesc texture_desc{};
	texture_desc.width = 1;
	texture_desc.height = 1;
	texture_desc.mip_levels = 1;
	texture_desc.type = kTextureType2D;
	texture_desc.format = kTextureFormatRGBA8;
	add_texture(&texture_desc, &tex);

	ResourceUpdateDesc resource_update_desc;
	TextureUpdateDesc tex_update_desc{};
	resource_update_desc = &tex_update_desc;
	tex_update_desc.size = size;
	tex_update_desc.texture = tex;
	tex_update_desc.data = white_data.data();
	begin_update_resource(resource_update_desc);
	std::memcpy(tex_update_desc.mapped_data, white_data.data(), tex_update_desc.size);
	end_update_resource(resource_update_desc);

	return tex;
}


Texture* load_texture(const std::string_view& name)
{
	Texture* tex;

	int32_t width, height, channels;
	stbi_set_flip_vertically_on_load(true);
	uint8_t* buf = stbi_load(name.data(), &width, &height, &channels, 0);

	if (buf)
	{
		TextureDesc texture_desc{};
		texture_desc.width = width;
		texture_desc.height = height;
		texture_desc.mip_levels = 1;
		texture_desc.type = kTextureType2D;
		texture_desc.format = channels == 3 ? kTextureFormatRGB8 : kTextureFormatRGBA8;
		add_texture(&texture_desc, &tex);

		ResourceUpdateDesc resource_update_desc;
		TextureUpdateDesc tex_update_desc{};
		resource_update_desc = &tex_update_desc;
		tex_update_desc.size = width * height * channels;
		tex_update_desc.texture = tex;
		tex_update_desc.data = buf;
		begin_update_resource(resource_update_desc);
		std::memcpy(tex_update_desc.mapped_data, buf, tex_update_desc.size);
		end_update_resource(resource_update_desc);
	}
	else
	{
		std::cout << "error loading texture: " << name;
	}

	return tex;
}

using VertexData = std::variant<glm::vec2, glm::vec3, glm::vec4>;

struct VertexAttributeLayout
{
	VertexData data;
	std::string name;
};

struct Vertex
{
	std::vector<VertexData> data;
};

class VertexBuffer
{
public:
	VertexBuffer(const std::vector<VertexAttributeLayout>&& attr, const std::vector<Vertex>&& vert) :
		attributes(attr), vertexes(vert), plain_data(), size(0) {}

	VertexBuffer(const VertexBuffer& other) :
		attributes(other.attributes),
		vertexes(other.vertexes),
		plain_data(other.plain_data),
		size(other.size)
	{}

	float* get_plain_data()
	{
		size_t data_size = get_vertex_buffer_size();
		if (data_size != size)
		{
			size = data_size;

			plain_data.reserve(data_size);

			for (const auto& vertex : vertexes)
			{
				for (const auto& attr : vertex.data)
				{
					std::visit([&](auto&& arg) {
						using T = std::decay_t<decltype(arg)>;
						if constexpr (std::is_same_v<T, glm::vec2>)
						{
							plain_data.push_back(arg.x);
							plain_data.push_back(arg.y);
						}
						else if constexpr (std::is_same_v<T, glm::vec3>)
						{
							plain_data.push_back(arg.x);
							plain_data.push_back(arg.y);
							plain_data.push_back(arg.z);
						}
						else if constexpr (std::is_same_v<T, glm::vec4>)
						{
							plain_data.push_back(arg.x);
							plain_data.push_back(arg.y);
							plain_data.push_back(arg.z);
							plain_data.push_back(arg.w);
						}
						}, attr);
				}
			}
		}

		return plain_data.data();
	}

	const size_t get_size() const
	{
		return vertex_size() * vertexes.size();
	}

	const uint32_t offsetof_by_name(std::string_view name) const
	{
		size_t offset = 0;
		for (const auto& attr : attributes) {
			if (attr.name == name) {
				return offset;
			}
			if (std::holds_alternative<glm::vec2>(attr.data)) {
				offset += sizeof(glm::vec2);
			}
			else if (std::holds_alternative<glm::vec3>(attr.data)) {
				offset += sizeof(glm::vec3);
			}
			else if (std::holds_alternative<glm::vec4>(attr.data)) {
				offset += sizeof(glm::vec4);
			}
		}
		return -1;
	}

	const size_t attribute_size(std::string_view name) const
	{
		for (const auto& attr : attributes) {
			if (attr.name == name) {
				if (std::holds_alternative<glm::vec2>(attr.data)) {
					return 2;
				}
				else if (std::holds_alternative<glm::vec3>(attr.data)) {
					return 3;
				}
				else if (std::holds_alternative<glm::vec4>(attr.data)) {
					return 4;
				}
			}
		}
		return -1;
	}

	const size_t vertex_size() const
	{
		size_t size = 0;
		for (const auto& attr : attributes) {
			if (std::holds_alternative<glm::vec2>(attr.data)) {
				size += sizeof(glm::vec2);
			}
			else if (std::holds_alternative<glm::vec3>(attr.data)) {
				size += sizeof(glm::vec3);
			}
			else if (std::holds_alternative<glm::vec4>(attr.data)) {
				size += sizeof(glm::vec4);
			}
		}
		return size;
	}

	const uint32_t attrib_count() const
	{
		return attributes.size();
	}

private:
	std::vector<VertexAttributeLayout> attributes;
	std::vector<Vertex> vertexes;
	std::vector<float> plain_data;
	size_t size;

	size_t get_vertex_buffer_size()
	{
		size_t data_size = 0;
		for (const auto& vertex : vertexes)
		{
			data_size += vertex.data.size();
		}
		return data_size * sizeof(float);
	}
};

class Mesh
{
public:
	Mesh(const VertexBuffer& buffer, const std::vector<uint32_t>& indices, const std::vector<Texture*> textures) :
		buffer(buffer), indices(indices), textures(textures),
		gpu_vertex_buffer(nullptr), gpu_index_buffer(nullptr)
	{
		upload_buffers();
	}

	void setup_vertex_layout(VertexLayout& layout)
	{
		layout.attrib_count = buffer.attrib_count();
		layout.attribs[0].size = buffer.attribute_size("position");
		layout.attribs[0].format = kAttribFormatFloat;
		layout.attribs[0].offset = buffer.offsetof_by_name("position");
		layout.attribs[1].size = buffer.attribute_size("normal");
		layout.attribs[1].format = kAttribFormatFloat;
		layout.attribs[1].offset = buffer.offsetof_by_name("normal");
		layout.attribs[2].size = buffer.attribute_size("tex_coords");
		layout.attribs[2].format = kAttribFormatFloat;
		layout.attribs[2].offset = buffer.offsetof_by_name("tex_coords");
	}

	void draw(CmdBuffer* cmd)
	{
		cmd_bind_vertex_buffer(cmd, gpu_vertex_buffer, buffer.attrib_count(), 0, buffer.vertex_size());
		cmd_bind_index_buffer(cmd, gpu_index_buffer);
		cmd_draw_indexed(cmd, indices.size(), 0, 0);
	}

	const VertexBuffer& get_vertex_buffer() const
	{
		return buffer;
	}

	Texture* get_texture(uint32_t index) const
	{
		if (index >= textures.size())
			return nullptr;
		return textures[index];
	}

private:
	void upload_buffers()
	{
		BufferDesc buffer_desc;
		buffer_desc.size = buffer.get_size();
		buffer_desc.flags = kBufferFlagGPUOnly;
		add_buffer(&buffer_desc, &gpu_vertex_buffer);

		uint64_t indexes_size = indices.size() * sizeof(uint32_t);
		buffer_desc.size = indexes_size;
		add_buffer(&buffer_desc, &gpu_index_buffer);

		ResourceUpdateDesc resource_update_desc;
		{ // update buffers data
			BufferUpdateDesc update_desc{};
			resource_update_desc = &update_desc;
			update_desc.buffer = gpu_vertex_buffer;
			update_desc.size = buffer.get_size();
			begin_update_resource(resource_update_desc);
			std::memcpy(update_desc.mapped_data, buffer.get_plain_data(), buffer.get_size());
			end_update_resource(resource_update_desc);

			update_desc.buffer = gpu_index_buffer;
			update_desc.size = indexes_size;
			begin_update_resource(resource_update_desc);
			std::memcpy(update_desc.mapped_data, indices.data(), indexes_size);
			end_update_resource(resource_update_desc);
		}
	}

private:
	VertexBuffer buffer;
	std::vector<uint32_t> indices;
	std::vector<Texture*> textures;

	Buffer* gpu_vertex_buffer;
	Buffer* gpu_index_buffer;
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

		process_node(scene->mRootNode, scene);
	}

	void setup_descriptor_set(Shader* shader, DescriptorSetUpdateFrequency update_freq, Sampler* sampler)
	{
		DescriptorSetDesc set_desc;
		set_desc.max_sets = meshes.size();
		set_desc.shader = shader;
		set_desc.update_freq = update_freq;
		add_descriptor_set(&set_desc, &set);

		uint32_t index = 0;
		for (const auto& mesh : meshes)
		{
			// It is incorrect we can't garauntee that 
			// 0th texture is diffuse_map,
			// 1st texture is specular_map, etc
			// TODO: store info about textures inside Mesh
			std::vector<DescriptorInfo> infos{
				{
					.name = "diffuse_map",
					.descriptor =
					DescriptorInfo::CombinedTextureSample{
						mesh.get_texture(0),
						"samplerState",
					}
				},
				{
					.name = "specular_map",
					.descriptor =
					DescriptorInfo::CombinedTextureSample{
						mesh.get_texture(1),
						"samplerState",
					}
				},
				{
					.name = "samplerState",
					.descriptor = sampler
				},
			};

			UpdateDescriptorSetDesc update_set_desc = {};
			update_set_desc.index = index;
			update_set_desc.infos = std::move(infos);
			update_descriptor_set(&update_set_desc, set);
			++index;
		}
	}

	void setup_vertex_layout(VertexLayout& layout)
	{
		meshes[0].setup_vertex_layout(layout);
	}

	void draw(CmdBuffer* cmd)
	{
		for (uint32_t i = 0; i < meshes.size(); ++i)
		{
			cmd_bind_descriptor_set(cmd, set, i);
			meshes[i].draw(cmd);
		}
	}

private:
	void process_node(aiNode* node, const aiScene* scene)
	{
		// process all the node's meshes (if any)
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.push_back(process_mesh(mesh, scene));
		}
		// then do the same for each of its children
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			process_node(node->mChildren[i], scene);
		}
	}
	
	Mesh process_mesh(aiMesh* mesh, const aiScene* scene)
	{
		std::vector<VertexAttributeLayout> layouts;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<Texture*> textures;

		bool has_pos = false;
		bool has_norm = false;
		bool has_tang = false;
		bool has_color = false;
		bool has_tex_coord = false;

		if (mesh->HasPositions())
		{
			has_pos = true;
			layouts.push_back({ glm::vec3{}, "position" });
		}

		if (mesh->HasNormals())
		{
			has_norm = true;
			layouts.push_back({ glm::vec3{}, "normal" });
		}

		if (mesh->HasTangentsAndBitangents())
		{
			has_tang = true;
			layouts.push_back({ glm::vec3{}, "tangent" });
			layouts.push_back({ glm::vec3{}, "bi_tangent" });
		}

		for (uint32_t i = 0; i < mesh->GetNumUVChannels(); ++i)
		{
			if (mesh->HasVertexColors(i))
			{
				has_color = true;
				layouts.push_back({ glm::vec4{}, "color" + i });
			}
		}

		for (uint32_t i = 0; i < mesh->GetNumUVChannels(); ++i)
		{
			if (mesh->HasTextureCoords(i))
			{
				has_tex_coord = true;
				if (mesh->mNumUVComponents[i] == 2)
					layouts.push_back({ glm::vec2{}, "tex_coords" + i });
				
				if (mesh->mNumUVComponents[i] == 3)
					layouts.push_back({ glm::vec3{}, "tex_coords" + i });
			}
		}

		for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
		{
			Vertex vertex;

			if (has_pos)
			{
				glm::vec3 pos;
				pos.x = mesh->mVertices[i].x;
				pos.y = mesh->mVertices[i].y;
				pos.z = mesh->mVertices[i].z;
				vertex.data.push_back(pos);
			}

			if (has_norm)
			{
				glm::vec3 norm;
				norm.x = mesh->mNormals[i].x;
				norm.y = mesh->mNormals[i].y;
				norm.z = mesh->mNormals[i].z;
				vertex.data.push_back(norm);
			}

			if (has_tang)
			{
				glm::vec3 tang;
				tang.x = mesh->mTangents[i].x;
				tang.y = mesh->mTangents[i].y;
				tang.z = mesh->mTangents[i].z;
				vertex.data.push_back(tang);

				tang.x = mesh->mBitangents[i].x;
				tang.y = mesh->mBitangents[i].y;
				tang.z = mesh->mBitangents[i].z;
				vertex.data.push_back(tang);
			}

			if (has_color)
			{
				for (uint32_t j = 0; j < mesh->GetNumColorChannels(); ++j)
				{
					glm::vec4 col;
					col.r = mesh->mColors[i][j].r;
					col.g = mesh->mColors[i][j].g;
					col.b = mesh->mColors[i][j].b;
					col.a = mesh->mColors[i][j].a;
					vertex.data.push_back(col);
				}
			}

			if (has_tex_coord)
			{
				for (uint32_t j = 0; j < mesh->GetNumUVChannels(); ++j)
				{
					if (mesh->mNumUVComponents[j] == 2)
					{
						glm::vec2 tex_coord;
						tex_coord.x = mesh->mTextureCoords[j][i].x;
						tex_coord.y = mesh->mTextureCoords[j][i].y;
						vertex.data.push_back(tex_coord);
					}

					if (mesh->mNumUVComponents[j] == 3)
					{
						glm::vec3 tex_coord;
						tex_coord.x = mesh->mTextureCoords[j][i].x;
						tex_coord.y = mesh->mTextureCoords[j][i].y;
						tex_coord.z = mesh->mTextureCoords[j][i].z;
						vertex.data.push_back(tex_coord);
					}
				}
			}

			vertices.push_back(vertex);
		}
		VertexBuffer buffer(std::move(layouts), std::move(vertices));

		for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
		{
			aiFace face = mesh->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; ++j)
				indices.push_back(face.mIndices[j]);
		}

			// process material
		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			std::vector<Texture*> diffuseMaps = load_material_textures(material,
				aiTextureType_DIFFUSE);
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
			std::vector<Texture*> specularMaps = load_material_textures(material,
				aiTextureType_SPECULAR);
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		}

		return Mesh(buffer, indices, textures);
	}

	std::vector<Texture*> load_material_textures(aiMaterial* mat, aiTextureType type)
	{
		std::vector<Texture*> textures;
		for (uint32_t i = 0; i < mat->GetTextureCount(type); ++i)
		{
			aiString str;
			mat->GetTexture(type, i, &str);
			std::string path = directory + '/' + str.C_Str();
			bool skip = false;
			auto tex = loaded_textures.find(path);
			if (tex != loaded_textures.end())
			{
				// TODO: optimize this
				textures.push_back(tex->second);
				skip = true;
			}

			if (!skip)
			{
				Texture* tex = load_texture(path);
				textures.push_back(tex);
				loaded_textures[path] = tex;
			}
		}

		if (textures.empty())
		{
			textures.push_back(load_white_texture());
		}

		return textures;
	}

private:
	std::string directory;
	std::vector<Mesh> meshes;

	DescriptorSet* set;

	std::unordered_map<std::string, Texture*> loaded_textures;
};

class GeometryRenderer
{
public:

private:
	std::vector<Model> models;
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
	glm::vec4 attenuation[kPointLightCount];
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
	glm::vec4 ambient[kLightSourcesCount];
	glm::vec4 diffuse[kLightSourcesCount];
	glm::vec4 specular[kLightSourcesCount];
};

struct UBO
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 model[11];
	DirectLight dir_light;
	PointLight point_light;
	SpotLight spot_light;
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

	VertexBuffer vertex_buffer{
		{
			VertexAttributeLayout{glm::vec3{}, "position"},
			VertexAttributeLayout{glm::vec2{}, "tex_coords"},
			VertexAttributeLayout{glm::vec3{}, "normal"},
		},
		{
			Vertex{{glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{0.0f, 0.0f}}},
			Vertex{{glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{1.0f, 0.0f}}},
			Vertex{{glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{1.0f, 1.0f}}},
			Vertex{{glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{0.0f, 1.0f}}},

			Vertex{{glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3{0.0f, 0.0f, -1.0f}, glm::vec2{0.0f, 0.0f}}},
			Vertex{{glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec3{0.0f, 0.0f, -1.0f}, glm::vec2{1.0f, 0.0f}}},
			Vertex{{glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec3{0.0f, 0.0f, -1.0f}, glm::vec2{1.0f, 1.0f}}},
			Vertex{{glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3{0.0f, 0.0f, -1.0f}, glm::vec2{0.0f, 1.0f}}},

			Vertex{{glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3{-1.0f, 0.0f, 0.0f}, glm::vec2{1.0f, 0.0f}}},
			Vertex{{glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3{-1.0f, 0.0f, 0.0f}, glm::vec2{0.0f, 0.0f}}},
			Vertex{{glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3{-1.0f, 0.0f, 0.0f}, glm::vec2{0.0f, 1.0f}}},
			Vertex{{glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3{-1.0f, 0.0f, 0.0f}, glm::vec2{1.0f, 1.0f}}},

			Vertex{{glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec2{0.0f, 0.0f}}},
			Vertex{{glm::vec3(0.5f, -0.5f,  0.5f), glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec2{1.0f, 0.0f}}},
			Vertex{{glm::vec3(0.5f,  0.5f,  0.5f), glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec2{1.0f, 1.0f}}},
			Vertex{{glm::vec3(0.5f,  0.5f, -0.5f), glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec2{0.0f, 1.0f}}},

			Vertex{{glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3{0.0f, -1.0f, 0.0f}, glm::vec2{0.0f, 1.0f}}},
			Vertex{{glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec3{0.0f, -1.0f, 0.0f}, glm::vec2{1.0f, 1.0f}}},
			Vertex{{glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec3{0.0f, -1.0f, 0.0f}, glm::vec2{1.0f, 0.0f}}},
			Vertex{{glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3{0.0f, -1.0f, 0.0f}, glm::vec2{0.0f, 0.0f}}},

			Vertex{{glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec2{0.0f, 0.0f}}},
			Vertex{{glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec2{1.0f, 0.0f}}},
			Vertex{{glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec2{1.0f, 1.0f}}},
			Vertex{{glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec2{0.0f, 1.0f}}},
		}
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

	glm::vec4 backpack_pos(0.0f);


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
	ubo.spot_light.cutoff[0] = glm::vec4(
		glm::cos(glm::radians(12.5f)), 
		glm::cos(glm::radians(17.5f)),
		0.0f, 0.0f
	);
	ubo.spot_light.attenuation[0] = glm::vec4(1.0f, 0.007f, 0.0002f, 0.0f);
	ubo.light_params.ambient[2] = glm::vec4(0.0f, 0.0f, 0.2f, 0.0f);
	ubo.light_params.diffuse[2] = glm::vec4(0.0f, 0.0f, 0.5f, 0.0f);
	ubo.light_params.specular[2] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);


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

	std::vector<Texture*> textures = {
		diffuse_map_tex,
		specular_map_tex
	};


	Mesh test_mesh(vertex_buffer, indexes, textures);
	Model mdl("assets/sponza/sponza.obj");

	Sampler* sampler;
	SamplerDesc sampler_desc{};
	sampler_desc.min_filter = kFilterTypeNearest;
	sampler_desc.mag_filter = kFilterTypeNearest;
	sampler_desc.wrap_u = kWrapModeClampToEdge;
	sampler_desc.wrap_v = kWrapModeClampToEdge;
	add_sampler(&sampler_desc, &sampler);

	BufferDesc buffer_desc;
	buffer_desc.size = vertex_buffer.get_size();
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
		update_desc.size = vertex_buffer.get_size();
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, vertex_buffer.get_plain_data(), vertex_buffer.get_size());
		end_update_resource(resource_update_desc);

		uint64_t indexes_size = indexes.size() * sizeof(uint32_t);
		update_desc.buffer = ebo;
		update_desc.size = indexes_size;
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, indexes.data(), indexes_size);
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

		material[10].shinines = 1;

		update_desc.buffer = mat_buf;
		update_desc.size = sizeof(material);
		begin_update_resource(resource_update_desc);
		std::memcpy(update_desc.mapped_data, &material, sizeof(material));
		end_update_resource(resource_update_desc);		
	}

	VertexLayout layout = {0};
	//test_mesh.setup_vertex_layout(layout);
	mdl.setup_vertex_layout(layout);

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
			}
		},
		{
			.name = "specular_map",
			.descriptor =
			DescriptorInfo::CombinedTextureSample{
				specular_map_tex,
				"samplerState",
			}
		},
		{
			.name = "samplerState",
			.descriptor = sampler
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

	mdl.setup_descriptor_set(shader, kUpdateFreqNone, sampler);

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
		ubo.spot_light.position[0] = ubo.view_pos;
		ubo.spot_light.direction[0] = glm::vec4(camera.front, 0.0f);

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

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(backpack_pos));
		model = glm::scale(model, glm::vec3(0.05f, 0.05f, 0.05f));
		ubo.model[10] = model;

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
			//cmd_bind_vertex_buffer(cmd, vbo, layout.attrib_count, 0, sizeof(float) * 8);
			//cmd_bind_index_buffer(cmd, ebo);
			cmd_bind_descriptor_set(cmd, ubo_desc, frame_index);
			cmd_bind_descriptor_set(cmd, texture_set, 0);
			cmd_bind_push_constant(cmd, &i);
			test_mesh.draw(cmd);
			//cmd_draw_indexed(cmd, 36, 0, 0);
		}

		cmd_bind_pipeline(cmd, graphics_pipeline);
		cmd_bind_descriptor_set(cmd, ubo_desc, frame_index);
		uint32_t index = 10;
		cmd_bind_push_constant(cmd, &index);
		mdl.draw(cmd);
		
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
