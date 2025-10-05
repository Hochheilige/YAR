#pragma once

#include <future>
#include <memory>
#include <string_view>
#include <vector>

#define WHITE_TEXTURE "DEBUG_WHITE_TEXTURE"

struct TextureAsset
{
	uint32_t width;
	uint32_t height;
	uint32_t channels;
	uint8_t* pixels;
	std::string path;

	~TextureAsset();
};

void init_asset_manager();

auto load_texture(std::string_view path) -> std::shared_future<std::shared_ptr<TextureAsset>>;