#pragma once

#include <future>
#include <memory>
#include <string_view>
#include <vector>

// TODO: move idea with creating gpu resources to render thread
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

void init_asset_manager();
auto load_texture(std::string_view path) -> std::shared_future<std::shared_ptr<TextureAsset>>;
auto load_cubemap(const std::array<std::string_view, 6>& paths) -> std::shared_future<std::shared_ptr<TextureAsset>>;
auto get_gpu_texture(std::shared_future<std::shared_ptr<TextureAsset>>& texture_asset, yar_texture_type type) -> yar_texture*;