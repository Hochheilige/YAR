#include "asset_manager.h"
#include "asset_manager_internal.h"

#include <memory>
#include <future>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static std::unique_ptr<AssetManager> asset_manager{ nullptr };

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
	if (!pixels)
		return load_debug_white_texture();

	texture->width = width;
	texture->height = height;
	texture->channels = channels;
	texture->pixels = pixels;

	return texture;
}

yar_texture* get_gpu_texture(std::shared_future<std::shared_ptr<TextureAsset>>& texture_asset, yar_texture_type type, uint32_t channels)
{
	yar_texture* tex;

	auto& asset = texture_asset.get();
	if (asset->gpu_texture)
		return asset->gpu_texture;

	const uint32_t width = asset->width;
	const uint32_t height = asset->height;
	uint32_t cur_channels = channels;
	if (channels == 0)
		cur_channels = asset->channels;
	uint8_t* pixels = asset->pixels;

	yar_texture_format format;
	if (cur_channels == 1)
		format = yar_texture_format_r8;
	if (cur_channels == 3)
		format = yar_texture_format_rgb8;
	if (cur_channels == 4)
		format = yar_texture_format_rgba8;

	if (pixels)
	{
		yar_texture_desc texture_desc{};
		texture_desc.width = width;
		texture_desc.height = height;
		texture_desc.mip_levels = 1 + (uint32_t)std::floor(std::log2(std::max(width, height)));;
		texture_desc.type = type;
		texture_desc.format = format;
		texture_desc.usage = yar_texture_usage_shader_resource;
		texture_desc.name = asset->path.c_str();
		add_texture(&texture_desc, &tex);

		yar_resource_update_desc resource_update_desc;
		yar_texture_update_desc tex_update_desc{};
		resource_update_desc = &tex_update_desc;
		tex_update_desc.size = width * height * cur_channels;
		tex_update_desc.texture = tex;
		tex_update_desc.data = pixels;
		begin_update_resource(resource_update_desc);
		std::memcpy(tex_update_desc.mapped_data, pixels, tex_update_desc.size);
		end_update_resource(resource_update_desc);

		asset->gpu_texture = tex;

		stbi_image_free(asset->pixels);
		asset->pixels = nullptr;
	}
	else
	{
		std::cout << "error loading texture: " << asset->path;
		return nullptr;
	}

	return tex;
}

auto load_texture(std::string_view path) 
-> std::shared_future<std::shared_ptr<TextureAsset>>
{
	auto it = asset_manager->textures.find(std::string(path));
	if (it != asset_manager->textures.end())
		return it->second;

	// TODO: add thread pool or job system for this
	auto result = std::async(std::launch::async, 
		[path = std::string(path)]() { return load_texture_async(path); }
	).share();

	asset_manager->textures.emplace(path, result);
	return result;
}

static std::string make_cubemap_key(const std::array<std::string_view, 6>& paths) {
	std::string key;
	for (const auto& p : paths) {
		key += p;
		key += '|';
	}
	return key;
}

static auto load_cubemap_async(const std::array<std::string_view, 6>& paths) -> std::shared_ptr<TextureAsset>
{
	std::string key = make_cubemap_key(paths);
	auto texture = std::make_shared<TextureAsset>();
	texture->path = key;

	int32_t width = 0, height = 0, channels = 0;
	std::vector<uint8_t*> faces_pixels(6);

	for (int i = 0; i < 6; ++i) {
		int w, h, c;
		stbi_set_flip_vertically_on_load(false);
		uint8_t* pixels = stbi_load(paths[i].data(), &w, &h, &c, 0);
		if (!pixels) {
			std::cerr << "Failed to load cubemap face: " << paths[i] << "\n";
			for (int j = 0; j < i; ++j) stbi_image_free(faces_pixels[j]);
			return nullptr;
		}

		if (i == 0) {
			width = w;
			height = h;
			channels = c;
		}
		else {
			if (w != width || h != height || c != channels) {
				std::cerr << "Cubemap face size mismatch: " << paths[i] << "\n";
				for (int j = 0; j <= i; ++j) stbi_image_free(faces_pixels[j]);
				return nullptr;
			}
		}

		faces_pixels[i] = pixels;
	}

	size_t face_size = width * height * channels;
	texture->pixels = new uint8_t[face_size * 6];
	texture->width = width;
	texture->height = height;
	texture->channels = channels;

	for (int i = 0; i < 6; ++i) {
		std::memcpy(texture->pixels + i * face_size, faces_pixels[i], face_size);
		stbi_image_free(faces_pixels[i]);
	}

	return texture;
}

auto load_cubemap(const std::array<std::string_view, 6>& paths) -> std::shared_future<std::shared_ptr<TextureAsset>>
{
	auto key = make_cubemap_key(paths);
	auto it = asset_manager->textures.find(key);
	if (it != asset_manager->textures.end())
		return it->second;

	auto result = std::async(std::launch::async,
		[=]() { return load_cubemap_async(paths); }
	).share();

	asset_manager->textures.emplace(key, result);
	return result;
}