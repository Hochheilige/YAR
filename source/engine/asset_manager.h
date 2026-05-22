#pragma once

#include <memory>
#include <string_view>
#include <array>

#include "asset_handle.h"
#include "render.h"

#define WHITE_TEXTURE "DEBUG_WHITE_TEXTURE"

struct TextureMip
{
	size_t width;
	size_t height;
	uint32_t size;
	uint8_t* data;
};

enum TextureSourceFormat
{
	TEX_SRC_PNG,
	TEX_SRC_DDS
};

struct TextureAsset
{
	TextureSourceFormat source_format;

	uint32_t width;
	uint32_t height;
	yar_texture_format format;

	uint32_t channels;
	uint8_t* pixels;

	std::vector<TextureMip> mips;

	yar_texture* gpu_texture;
	std::string path;
};

struct ModelData;

void init_asset_manager();
void shutdown_asset_manager();

auto load_texture(std::string_view path) -> AssetHandle<TextureAsset>;
auto load_cubemap(const std::array<std::string_view, 6>& paths) -> AssetHandle<TextureAsset>;
auto get_gpu_texture(AssetHandle<TextureAsset>& texture_asset, yar_texture_type type, uint32_t channels = 0) -> yar_texture*;

auto load_model_asset(std::string_view path) -> AssetHandle<ModelData>;