#pragma once

#include <memory>
#include <string_view>
#include <array>

#include "asset_handle.h"
#include "render.h"

#define WHITE_TEXTURE "DEBUG_WHITE_TEXTURE"

struct TextureAsset
{
	uint32_t width;
	uint32_t height;
	uint32_t channels;
	uint8_t* pixels;
	yar_texture* gpu_texture;
	std::string path;
};

struct ModelData;

void init_asset_manager();
void shutdown_asset_manager();

auto load_texture(std::string_view path) -> AssetHandle<TextureAsset>;
auto load_cubemap(const std::array<std::string_view, 6>& paths) -> AssetHandle<TextureAsset>;
auto get_gpu_texture(AssetHandle<TextureAsset>& texture_asset, yar_texture_type type, uint32_t channels = 0) -> yar_texture*;

// Model loading is synchronous because it creates GPU resources
// (OpenGL context is thread-local)
auto load_model_asset(std::string_view path) -> std::shared_ptr<ModelData>;