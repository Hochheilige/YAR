#include "model_loader.h"
#include "asset_manager.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <meshoptimizer.h>

#include <iostream>

namespace
{

void optimize_mesh(std::vector<VertexStatic>& vertices, std::vector<uint32_t>& indices)
{
	uint32_t vertex_size = sizeof(VertexStatic);
	uint32_t index_count = static_cast<uint32_t>(indices.size());
	uint32_t vertex_count = static_cast<uint32_t>(vertices.size());

	std::vector<uint32_t> remap(vertex_count);
	size_t opt_vertex_count = meshopt_generateVertexRemap(
		remap.data(), indices.data(), index_count,
		vertices.data(), vertex_count, vertex_size);

	std::vector<uint32_t> opt_indices(index_count);
	std::vector<VertexStatic> opt_vertices(opt_vertex_count);

	meshopt_remapIndexBuffer(opt_indices.data(), indices.data(), index_count, remap.data());
	meshopt_remapVertexBuffer(opt_vertices.data(), vertices.data(), vertex_count, vertex_size, remap.data());

	meshopt_optimizeVertexCache(opt_indices.data(), opt_indices.data(), index_count, opt_vertex_count);

	meshopt_optimizeOverdraw(opt_indices.data(), opt_indices.data(), index_count,
		&(opt_vertices[0].position[0]), opt_vertex_count, vertex_size, 1.05f);

	meshopt_optimizeVertexFetch(opt_vertices.data(), opt_indices.data(), index_count,
		opt_vertices.data(), opt_vertex_count, vertex_size);

	vertices = std::move(opt_vertices);
	indices = std::move(opt_indices);
}

AssetHandle<TextureAsset> load_material_texture(aiMaterial* mat, aiTextureType type, const std::string& directory)
{
	if (mat->GetTextureCount(type) > 0)
	{
		aiString str;
		mat->GetTexture(type, 0, &str);
		std::string path = directory + '/' + str.C_Str();
		return load_texture(path);
	}
	return load_texture(WHITE_TEXTURE);
}

struct ProcessedMesh
{
	std::vector<VertexStatic> vertices;
	std::vector<uint32_t> indices;
	int32_t material_index = -1;
};

ProcessedMesh process_mesh(aiMesh* mesh, const aiScene* scene, const Matrix4x4& transform)
{
	ProcessedMesh result;

	for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
	{
		VertexStatic vertex{};

		Vector4 pos = transform * Vector4(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
		vertex.position = Vector3(pos);

		if (mesh->HasNormals())
		{
			Matrix4x4 normal_mat = transform.inverse().transpose();
			Vector3 norm(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			vertex.normal = normal_mat.transform_direction(norm).normalized();
		}

		if (mesh->HasTextureCoords(0))
		{
			vertex.uv0.x() = mesh->mTextureCoords[0][i].x;
			vertex.uv0.y() = mesh->mTextureCoords[0][i].y;
		}

		if (mesh->HasTextureCoords(1))
		{
			vertex.uv1.x() = mesh->mTextureCoords[1][i].x;
			vertex.uv1.y() = mesh->mTextureCoords[1][i].y;
		}

		if (mesh->HasTangentsAndBitangents())
		{
			vertex.tangent.x() = mesh->mTangents[i].x;
			vertex.tangent.y() = mesh->mTangents[i].y;
			vertex.tangent.z() = mesh->mTangents[i].z;
			vertex.bitangent.x() = mesh->mBitangents[i].x;
			vertex.bitangent.y() = mesh->mBitangents[i].y;
			vertex.bitangent.z() = mesh->mBitangents[i].z;
		}

		result.vertices.push_back(vertex);
	}

	for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
	{
		aiFace& face = mesh->mFaces[i];
		for (uint32_t j = 0; j < face.mNumIndices; ++j)
			result.indices.push_back(face.mIndices[j]);
	}

	result.material_index = mesh->mMaterialIndex;

	return result;
}

void process_node(
	aiNode* node,
	const aiScene* scene,
	const Matrix4x4& parent_transform,
	std::vector<ProcessedMesh>& out_meshes)
{
	Matrix4x4 local_transform = Matrix4x4(&node->mTransformation.a1).transpose();
	Matrix4x4 global_transform = local_transform * parent_transform;

	for (uint32_t i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		out_meshes.push_back(process_mesh(mesh, scene, global_transform));
	}

	for (uint32_t i = 0; i < node->mNumChildren; ++i)
	{
		process_node(node->mChildren[i], scene, global_transform, out_meshes);
	}
}

} // namespace

void StaticMesh::bind_and_draw(yar_cmd_buffer* cmd) const
{
	if (mesh_asset)
		mesh_asset->bind_and_draw(cmd, sizeof(VertexStatic));
}

void ModelData::setup_descriptor_set(yar_shader* shader, yar_sampler* sampler)
{
	for (auto& material : materials)
	{
		material->create_descriptor_set(shader, sampler);
	}
}

void ModelData::draw(yar_cmd_buffer* cmd, bool bind_descriptor)
{
	for (auto& mesh : meshes)
	{
		if (bind_descriptor && mesh.material && mesh.material->descriptor_set)
			cmd_bind_descriptor_set(cmd, mesh.material->descriptor_set, 0);

		mesh.bind_and_draw(cmd);
	}
}

ModelData load_model(const std::string_view& path)
{
	ModelData model_data;

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "Error [assimp]: " << importer.GetErrorString() << std::endl;
		return model_data;
	}

	std::string directory(path.substr(0, path.find_last_of('/')));

	// Load all materials
	for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
	{
		aiMaterial* ai_mat = scene->mMaterials[i];

		auto material = std::make_unique<Material>();
		material->shading_model = ShadingModel::Lit;
		material->albedo = load_material_texture(ai_mat, aiTextureType_DIFFUSE, directory);
		material->roughness = load_material_texture(ai_mat, aiTextureType_DIFFUSE_ROUGHNESS, directory);
		material->metalness = load_material_texture(ai_mat, aiTextureType_METALNESS, directory);
		material->normal = load_material_texture(ai_mat, aiTextureType_NORMALS, directory);

		model_data.materials.push_back(std::move(material));
	}

	// Process all nodes and collect meshes
	std::vector<ProcessedMesh> processed_meshes;
	process_node(scene->mRootNode, scene, Matrix4x4::identity(), processed_meshes);

	// Create MeshAssets and StaticMeshes
	yar_vertex_layout layout{};
	VertexStatic::setup_layout(layout);

	for (auto& processed : processed_meshes)
	{
		if (processed.vertices.empty() || processed.indices.empty())
			continue;

		optimize_mesh(processed.vertices, processed.indices);

		if (processed.vertices.empty() || processed.indices.empty())
			continue;

		auto mesh_asset = std::make_unique<MeshAsset>(
			create_mesh_asset(processed.vertices, processed.indices, layout));

		StaticMesh static_mesh;
		static_mesh.mesh_asset = mesh_asset.get();

		if (processed.material_index >= 0 &&
			processed.material_index < static_cast<int32_t>(model_data.materials.size()))
		{
			static_mesh.material = model_data.materials[processed.material_index].get();
		}

		model_data.mesh_assets.push_back(std::move(mesh_asset));
		model_data.meshes.push_back(static_mesh);
	}

	return model_data;
}
