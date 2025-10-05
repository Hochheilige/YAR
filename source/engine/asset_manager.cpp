#include "asset_manager.h"
#include "asset_manager_internal.h"

#include <memory>
#include <future>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static std::unique_ptr<AssetManager> asset_manager{ nullptr };

TextureAsset::~TextureAsset()
{
	stbi_image_free(pixels);
}

void init_asset_manager()
{
	if (asset_manager == nullptr)
	{
		asset_manager = std::make_unique<AssetManager>();
	}
}

// It must be just one white texture for every needs
static auto load_debug_white_texture() -> std::shared_ptr<TextureAsset>
{
	auto texture = std::make_shared<TextureAsset>();
	texture->path = WHITE_TEXTURE;

	texture->width = 1;
	texture->height = 1;
	texture->channels = 4;
	uint8_t* pixels = new uint8_t[4]{ 255, 255, 255, 255 };
	texture->pixels = pixels;

	return texture;
}


static auto load_texture_async(std::string_view path) -> std::shared_ptr<TextureAsset>
{
	// I thinkg that it possible to check the type of a file
	// and change loading process somehow, for example load
	// specific engine assets without stb

	if (path == WHITE_TEXTURE)
		return load_debug_white_texture();

	auto texture = std::make_shared<TextureAsset>();
	texture->path = path;

	int32_t width, height, channels;
	stbi_set_flip_vertically_on_load(false);
	uint8_t* pixels = stbi_load(path.data(), &width, &height, &channels, 0);

	texture->width = width;
	texture->height = height;
	texture->channels = channels;
	texture->pixels = pixels;

	return texture;
}

auto load_texture(std::string_view path) 
-> std::shared_future<std::shared_ptr<TextureAsset>>
{
	// TODO: add thread pool or job system for this
	auto result = std::async(std::launch::async, 
		[path = std::string(path)]() { return load_texture_async(path); }
	).share();

	asset_manager->textures.emplace(path, result);
	return result;
}