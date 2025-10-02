#pragma once
#include <cstdint>

enum yar_builtin_asset
{
    yar_builtin_white,
    yar_builtin_black,
    yar_builtin_checker,
    yar_builtin_count
};

enum yar_asset_type
{
    yar_asset_type_texture = 0,
    yar_asset_type_cubemap,
    yar_asset_type_model
};

struct yar_asset_texture
{
	uint32_t width;
	uint32_t height;
	uint32_t channels;
	uint8_t* pixels;
};

struct yar_asset_cubemap
{
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    uint8_t* pixels; // data for 6 faces: size = width * height * channels * 6
};

struct yar_asset_request {
    yar_asset_type type;
    uint64_t hash;

    union {
        char path[256];
        char cubemap_faces[6][256];
    };
};

void yar_request_asset(yar_asset_type type, const char* path);
void yar_request_asset(yar_asset_type type, const char* faces[6]);
void* yar_find_loaded_asset(yar_asset_type type, const char* path);
yar_asset_texture* yar_get_builtin(yar_builtin_asset id);
void init_asset_manager();
void terminate_asset_manager();

