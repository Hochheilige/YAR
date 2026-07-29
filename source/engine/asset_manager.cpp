#include "asset_manager.h"
#include "asset_manager_internal.h"
#include "thread_pool.h"
#include "model_loader.h"

#include <memory>
#include <future>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define NOMINMAX
#include <DirectXTex.h>

static std::unique_ptr<AssetManager> asset_manager{ nullptr };
static std::unique_ptr<ThreadPool> asset_thread_pool{ nullptr };

std::wstring to_wstring(std::string_view sv)
{
	if (sv.empty())
		return L"";

	int size_needed = MultiByteToWideChar(
		CP_UTF8,
		0,
		sv.data(),
		(int)sv.size(),
		nullptr,
		0
	);

	std::wstring result(size_needed, 0);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		sv.data(),
		(int)sv.size(),
		result.data(),
		size_needed
	);

	return result;
}

inline yar_texture_format dxgi_to_yar_format(DXGI_FORMAT format)
{
	switch (format)
	{
		// =========================
		// UNORM / SRGB 8-bit
		// =========================
	case DXGI_FORMAT_R8_UNORM: return yar_texture_format_r8;
	case DXGI_FORMAT_R8G8B8A8_UNORM: return yar_texture_format_rgba8;
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return yar_texture_format_srgba8;

	case DXGI_FORMAT_B8G8R8A8_UNORM: return yar_texture_format_rgba8;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return yar_texture_format_srgba8;

		// =========================
		// FLOAT formats
		// =========================
	case DXGI_FORMAT_R16G16B16A16_FLOAT: return yar_texture_format_rgba16f;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:  return yar_texture_format_rgba32f;

		// =========================
		// Depth formats
		// =========================
	case DXGI_FORMAT_D16_UNORM: return yar_texture_format_depth16;
	case DXGI_FORMAT_D24_UNORM_S8_UINT: return yar_texture_format_depth24_stencil8;
	case DXGI_FORMAT_D32_FLOAT: return yar_texture_format_depth32f;

		// =========================
		// BC1 / DXT1
		// =========================
	case DXGI_FORMAT_BC1_UNORM: return yar_texture_format_bc1;
	case DXGI_FORMAT_BC1_UNORM_SRGB: return yar_texture_format_bc1_srgb;

		// =========================
		// BC2 / DXT3
		// =========================
	case DXGI_FORMAT_BC2_UNORM: return yar_texture_format_bc2;
	case DXGI_FORMAT_BC2_UNORM_SRGB: return yar_texture_format_bc2;

		// =========================
		// BC3 / DXT5
		// =========================
	case DXGI_FORMAT_BC3_UNORM: return yar_texture_format_bc3;
	case DXGI_FORMAT_BC3_UNORM_SRGB: return yar_texture_format_bc3_srgb;

		// =========================
		// BC4 (single channel)
		// =========================
	case DXGI_FORMAT_BC4_UNORM: return yar_texture_format_bc4;
	case DXGI_FORMAT_BC4_SNORM: return yar_texture_format_bc4_snorm;

		// =========================
		// BC5 (normal maps)
		// =========================
	case DXGI_FORMAT_BC5_UNORM: return yar_texture_format_bc5;
	case DXGI_FORMAT_BC5_SNORM: return yar_texture_format_bc5_snorm;

		// =========================
		// BC6H (HDR)
		// =========================
	case DXGI_FORMAT_BC6H_UF16: return yar_texture_format_bc6h;
	case DXGI_FORMAT_BC6H_SF16: return yar_texture_format_bc6h_sfloat;

		// =========================
		// BC7 (best quality color)
		// =========================
	case DXGI_FORMAT_BC7_UNORM: return yar_texture_format_bc7;
	case DXGI_FORMAT_BC7_UNORM_SRGB: return yar_texture_format_bc7_srgb;

	default:
		return yar_texture_format_none;
	}
}

void init_asset_manager()
{
	if (asset_manager == nullptr)
	{
		asset_manager = std::make_unique<AssetManager>();
		asset_thread_pool = std::make_unique<ThreadPool>();
	}
}

void shutdown_asset_manager()
{
	asset_thread_pool.reset();
	asset_manager.reset();
}

// It must be just one white texture for every needs
static auto load_debug_white_texture() -> std::shared_ptr<TextureAsset>
{
	auto texture = std::make_shared<TextureAsset>();
	texture->path = WHITE_TEXTURE;

	texture->width = 1;
	texture->height = 1;
	texture->channels = 4;
	uint8_t* pixels = new uint8_t[4]{ 255, 0, 255, 255 };
	texture->pixels = pixels;
	texture->format = yar_texture_format_rgba8;
	texture->source_format = TEX_SRC_PNG;

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

	if (path.contains(".dds"))
	{
		TexMetadata meta;
		ScratchImage image;
		auto hres = LoadFromDDSFile(to_wstring(path).c_str(), DDS_FLAGS_NONE, &meta, image);
		if (hres != S_OK)
			return load_debug_white_texture();

		const Image* imgs = image.GetImages();
		uint32_t mip_count = image.GetImageCount();

		texture->source_format = TEX_SRC_DDS;
		texture->width = meta.width;
		texture->height = meta.height;
		texture->format = dxgi_to_yar_format(meta.format);

		texture->mips.resize(mip_count);
		for (uint32_t i = 0; i < mip_count; ++i)
		{
			auto* data = new uint8_t[imgs[i].slicePitch];
			std::memcpy(data, imgs[i].pixels, imgs[i].slicePitch);
			texture->mips[i] = {
				imgs[i].width,
				imgs[i].height,
				(uint32_t)imgs[i].slicePitch,
				data
			};
		}
	}
	else
	{
		int32_t width, height, channels;
		stbi_set_flip_vertically_on_load(false);
		uint8_t* pixels = stbi_load(path.data(), &width, &height, &channels, 0);
		
		yar_texture_format format = yar_texture_format_none;
		if (channels == 1)
			format = yar_texture_format_r8;
		if (channels == 3)
			format = yar_texture_format_rgb8;
		if (channels == 4)
			format = yar_texture_format_rgba8;

		if (!pixels || format == yar_texture_format_none)
			return load_debug_white_texture();

		texture->source_format = TEX_SRC_PNG;
		texture->width = width;
		texture->height = height;
		texture->channels = channels;
		texture->pixels = pixels;
		texture->format = format;
	}

	return texture;
}

yar_texture* get_gpu_texture(AssetHandle<TextureAsset>& texture_asset, yar_texture_type type, uint32_t channels)
{
	if (!texture_asset.wait())
		return nullptr;

	yar_texture* tex = nullptr;

	auto asset = texture_asset.get_shared();
	if (!asset)
		return nullptr;

	if (asset->gpu_texture)
		return asset->gpu_texture;

	const uint32_t width = asset->width;
	const uint32_t height = asset->height;

	if (asset->source_format == TEX_SRC_PNG)
	{
		uint32_t cur_channels = channels;
		if (channels == 0)
			cur_channels = asset->channels;
		uint8_t* pixels = asset->pixels;
		yar_texture_format format = asset->format;

		if (pixels)
		{
			yar_texture_desc texture_desc{};
			texture_desc.width = width;
			texture_desc.height = height;
			texture_desc.mip_levels = 1 + (uint32_t)std::floor(std::log2(std::max(width, height)));
			texture_desc.type = type;
			if (type == yar_texture_type_cube_map)
				texture_desc.depth = 6;
			texture_desc.format = format;
			texture_desc.usage = yar_texture_usage_shader_resource;
			texture_desc.name = asset->path.c_str();
			add_texture(&texture_desc, &tex);

			yar_resource_update_desc resource_update_desc;
			yar_texture_update_desc tex_update_desc{};
			resource_update_desc = &tex_update_desc;
			if (type == yar_texture_type_cube_map)
				tex_update_desc.size = width * height * cur_channels * 6;
			else
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
	}
	else if (asset->source_format == TEX_SRC_DDS)
	{
		if (asset->mips.empty())
		{
			std::cout << "DDS has no mip data: " << asset->path;
			return nullptr;
		}

		yar_texture_desc desc{};
		desc.width = width;
		desc.height = height;
		desc.mip_levels = (uint32_t)asset->mips.size();
		desc.type = type;
		desc.format = asset->format;
		desc.usage = yar_texture_usage_shader_resource;
		desc.name = asset->path.c_str();

		if (type == yar_texture_type_cube_map)
			desc.depth = 6;

		add_texture(&desc, &tex);

		// Upload each mip level
		for (uint32_t i = 0; i < asset->mips.size(); i++)
		{
			const auto& mip = asset->mips[i];

			yar_resource_update_desc resource_update_desc;
			yar_texture_update_desc tex_update_desc{};
			resource_update_desc = &tex_update_desc;

			tex_update_desc.texture = tex;
			tex_update_desc.size = mip.size;
			tex_update_desc.mip_level = i; // IMPORTANT (you need this in your system)

			begin_update_resource(resource_update_desc);
			std::memcpy(tex_update_desc.mapped_data, mip.data, mip.size);
			end_update_resource(resource_update_desc);
		}

		for (auto& mip : asset->mips)
		{
			delete[] mip.data;
			mip.data = nullptr;
		}
		asset->mips.clear();

		asset->gpu_texture = tex;
	}

	return tex;
}

auto load_texture(std::string_view path) -> AssetHandle<TextureAsset>
{
	std::lock_guard<std::mutex> lock(asset_manager->textures_mutex);

	auto it = asset_manager->textures.find(std::string(path));
	if (it != asset_manager->textures.end())
		return AssetHandle(it->second);

	auto result = asset_thread_pool->submit(
		[path = std::string(path)]() { return load_texture_async(path); }
	);

	asset_manager->textures.emplace(path, result);
	return AssetHandle(result);
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
	texture->source_format = TEX_SRC_PNG;

	yar_texture_format format = yar_texture_format_none;
	if (channels == 1)
		format = yar_texture_format_r8;
	if (channels == 3)
		format = yar_texture_format_rgb8;
	if (channels == 4)
		format = yar_texture_format_rgba8;
	texture->format = format;

	for (int i = 0; i < 6; ++i) {
		std::memcpy(texture->pixels + i * face_size, faces_pixels[i], face_size);
		stbi_image_free(faces_pixels[i]);
	}

	return texture;
}

auto load_cubemap(const std::array<std::string_view, 6>& paths) -> AssetHandle<TextureAsset>
{
	std::lock_guard<std::mutex> lock(asset_manager->textures_mutex);

	auto key = make_cubemap_key(paths);
	auto it = asset_manager->textures.find(key);
	if (it != asset_manager->textures.end())
		return AssetHandle(it->second);

	auto result = asset_thread_pool->submit([=]() { return load_cubemap_async(paths); });

	asset_manager->textures.emplace(key, result);
	return AssetHandle(result);
}

static auto load_model_async(std::string_view path) -> std::shared_ptr<ModelData>
{
	ModelData data = load_model(path);
	return std::make_shared<ModelData>(std::move(data));
}

auto load_model_asset(std::string_view path) -> AssetHandle<ModelData>
{
	std::lock_guard<std::mutex> lock(asset_manager->models_mutex);

	std::string key(path);
	auto it = asset_manager->models.find(key);
	if (it != asset_manager->models.end())
		return AssetHandle(it->second);

	auto result = asset_thread_pool->submit(
		[path = std::string(path)]() { return load_model_async(path); }
	);

	asset_manager->models.emplace(key, result);
	return AssetHandle(result);
}