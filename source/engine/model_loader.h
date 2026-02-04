#pragma once

#include "mesh_asset.h"
#include "material.h"
#include "vertex.h"
#include "math/yar_math.h"

#include <vector>
#include <string>
#include <string_view>
#include <memory>

struct StaticMesh
{
	MeshAsset* mesh_asset = nullptr;
	Material* material = nullptr;
	std::string name;

	void bind_and_draw(yar_cmd_buffer* cmd) const;
};

struct ModelData
{
	std::vector<std::unique_ptr<MeshAsset>> mesh_assets;
	std::vector<std::unique_ptr<Material>> materials;
	std::vector<StaticMesh> meshes;

	yar_descriptor_set* descriptor_set = nullptr;

	void setup_descriptor_set(yar_shader* shader, yar_sampler* sampler);
	void draw(yar_cmd_buffer* cmd, bool bind_descriptor = true);
};

ModelData load_model(const std::string_view& path);
