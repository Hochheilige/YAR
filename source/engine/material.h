#pragma once

#include "asset_handle.h"
#include "asset_manager.h"
#include "render.h"
#include "math/vector4.h"

enum class ShadingModel
{
	Lit,
	Unlit,
	Skybox
};

struct Material
{
	ShadingModel shading_model = ShadingModel::Lit;

	AssetHandle<TextureAsset> albedo;
	AssetHandle<TextureAsset> normal;
	AssetHandle<TextureAsset> roughness;
	AssetHandle<TextureAsset> metalness;

	Vector4 base_color = { 1.0f, 1.0f, 1.0f, 1.0f };
	float roughness_value = 0.5f;
	float metalness_value = 0.0f;

	yar_descriptor_set* descriptor_set = nullptr;

	bool is_ready() const;
	void create_descriptor_set(yar_shader* shader, yar_sampler* sampler);
};
