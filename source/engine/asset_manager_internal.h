#pragma once

#include "asset_manager.h"

#include <unordered_map>
#include <memory>
#include <string_view>
#include <future>
#include <mutex>

struct ModelData;

constexpr uint64_t hash_fnv1a(std::string_view str)
{
	uint64_t hash = 1469598103934665603ull; // offset basis
	for (char c : str)
	{
		hash ^= static_cast<unsigned char>(c);
		hash *= 1099511628211ull; // FNV prime
	}
	return hash;
}

struct BasicStringHash
{
	size_t operator()(std::string_view str) const noexcept
	{
		return static_cast<size_t>(hash_fnv1a(str));
	}
};

struct AssetManager
{
	AssetManager()
	{
		textures.reserve(MaxTextureCount);
		models.reserve(MaxModelCount);
	}

	std::mutex textures_mutex;
	std::unordered_map<
		std::string,
		std::shared_future<std::shared_ptr<TextureAsset>>,
		BasicStringHash> textures;

	std::mutex models_mutex;
	std::unordered_map<
		std::string,
		std::shared_ptr<ModelData>,
		BasicStringHash> models;

private:
	static constexpr size_t MaxTextureCount = 2048ull;
	static constexpr size_t MaxModelCount = 256ull;
};