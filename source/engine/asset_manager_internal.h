#pragma once

#include "asset_manager.h"

#include <thread>
#include <mutex>
#include <queue>

struct yar_asset_entry
{
	uint64_t hash;
	yar_asset_type type;
	void* asset;
};

struct yar_asset_ready {
	yar_asset_type type;
	void* asset;
	uint64_t hash;
};

struct yar_asset_manager
{
	static constexpr uint32_t MaxAssetsCount = 4096u;

	yar_asset_entry assets[MaxAssetsCount];
	uint32_t count;

	yar_asset_entry builtin[yar_builtin_count];

	std::queue<yar_asset_request> requests;
	std::mutex req_mutex;
	std::condition_variable req_cv;

	std::queue<yar_asset_ready> ready;
	std::mutex ready_mutex;

	std::vector<std::thread> workers;
	std::atomic<bool> running;
};
