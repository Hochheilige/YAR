#pragma once

#include <future>
#include <memory>
#include <chrono>

template<typename T>
class AssetHandle
{
public:
	AssetHandle() = default;

	explicit AssetHandle(std::shared_future<std::shared_ptr<T>> future)
		: asset_future(std::move(future))
	{
	}

	AssetHandle(const AssetHandle&) = default;
	AssetHandle(AssetHandle&&) noexcept = default;
	AssetHandle& operator=(const AssetHandle&) = default;
	AssetHandle& operator=(AssetHandle&&) noexcept = default;

	// Non-blocking: returns nullptr if asset still loading
	T* get() const
	{
		if (!is_ready())
			return nullptr;

		auto& asset = asset_future.get();
		return asset ? asset.get() : nullptr;
	}

	std::shared_ptr<T> get_shared() const
	{
		if (!is_ready())
			return nullptr;

		return asset_future.get();
	}

	// Check if loading complete (doesn't mean success, just done)
	bool is_ready() const
	{
		if (!is_valid())
			return false;

		return asset_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	}

	bool is_valid() const
	{
		return asset_future.valid();
	}

	explicit operator bool() const
	{
		return get() != nullptr;
	}

	T* operator->() const
	{
		return get();
	}

	T& operator*() const
	{
		return *get();
	}

	// Blocking wait - use sparingly
	T* wait() const
	{
		if (!is_valid())
			return nullptr;

		auto& asset = asset_future.get();
		return asset ? asset.get() : nullptr;
	}

private:
	std::shared_future<std::shared_ptr<T>> asset_future;
};
