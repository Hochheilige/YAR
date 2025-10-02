#include "asset_manager.h"
#include "asset_manager_internal.h"

#include <stb_image.h>

#include <memory>
#include <array>

static yar_asset_manager* asset_manager{ nullptr };

static uint64_t yar_hash(const char* str)
{
	uint64_t hash = 1469598103934665603ull;
	while (*str)
	{
		hash ^= (unsigned char)(*str++);
		hash *= 1099511628211ull;
	}
	return hash;
}

static yar_asset_texture* yar_create_solid_texture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    yar_asset_texture* tex = (yar_asset_texture*)std::malloc(sizeof(yar_asset_texture));
    tex->width = 1;
    tex->height = 1;
    tex->channels = 4;
    tex->pixels = (uint8_t*)std::malloc(4);
    tex->pixels[0] = r;
    tex->pixels[1] = g;
    tex->pixels[2] = b;
    tex->pixels[3] = a;
    return tex;
}

static yar_asset_texture* yar_create_checker_texture(int size = 8)
{
    int w = size, h = size;
    yar_asset_texture* tex = (yar_asset_texture*)std::malloc(sizeof(yar_asset_texture));
    tex->width = w;
    tex->height = h;
    tex->channels = 4;
    tex->pixels = (uint8_t*)std::malloc(w * h * 4);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int checker = ((x / 2) + (y / 2)) % 2;
            uint8_t c = checker ? 255 : 0;
            int idx = (y * w + x) * 4;
            tex->pixels[idx + 0] = c;
            tex->pixels[idx + 1] = c;
            tex->pixels[idx + 2] = c;
            tex->pixels[idx + 3] = 255;
        }
    }
    return tex;
}


static yar_asset_texture* yar_load_asset_texture(const char* path)
{
    if (!asset_manager || asset_manager->count >= yar_asset_manager::MaxAssetsCount)
        return nullptr;

    // it can be more optimal
    uint64_t h = yar_hash(path);
    for (uint32_t i = 0; i < asset_manager->count; i++)
    {
        yar_asset_entry* e = &asset_manager->assets[i];
        if (e->hash == h && e->type == yar_asset_type_texture)
        {
            return (yar_asset_texture*)e->asset;
        }
    }

    int32_t width, height, channels;
    uint8_t* pixels = stbi_load(path, &width, &height, &channels, 0);
    if (!pixels)
    {
        printf("Failed to load texture: %s\n", path);
        return nullptr;
    }

    yar_asset_texture* tex = static_cast<yar_asset_texture*>(
        std::malloc(sizeof(yar_asset_texture))
    );
    tex->width = width;
    tex->height = height;
    tex->channels = channels;
    tex->pixels = pixels; 

    yar_asset_entry* entry = &asset_manager->assets[asset_manager->count++];
    entry->hash = h;
    entry->type = yar_asset_type_texture;
    entry->asset = tex;

    return tex;
}

static yar_asset_cubemap* yar_load_asset_cubemap(const char faces[6][256])
{
    std::string combined;
    for (int i = 0; i < 6; i++)
    {
        combined += faces[i];
        combined += ";";
    }
    uint64_t h = yar_hash(combined.c_str());
    for (uint32_t i = 0; i < asset_manager->count; i++)
    {
        yar_asset_entry* e = &asset_manager->assets[i];
        if (e->hash == h && e->type == yar_asset_type_cubemap)
        {
            return (yar_asset_cubemap*)e->asset;
        }
    }

    int width = 0, height = 0, channels = 0;
    uint8_t* all_pixels = nullptr;

    for (size_t i = 0; i < 6; i++)
    {
        int w, h, c;
        stbi_set_flip_vertically_on_load(false);
        uint8_t* buf = stbi_load(faces[i], &w, &h, &c, 0);
        if (!buf)
        {
            printf("Failed to load cubemap face: %s\n", faces[i]);
            if (all_pixels) std::free(all_pixels);
            return nullptr;
        }

        if (i == 0)
        {
            width = w;
            height = h;
            channels = c;
            all_pixels = (uint8_t*)std::malloc(width * height * channels * 6);
        }
        else
        {
            if (w != width || h != height || c != channels)
            {
                printf("Cubemap face size mismatch: %s\n", faces[i]);
                stbi_image_free(buf);
                if (all_pixels) std::free(all_pixels);
                return nullptr;
            }
        }

        std::memcpy(
            all_pixels + i * width * height * channels,
            buf,
            width * height * channels
        );
        stbi_image_free(buf);
    }

    yar_asset_cubemap* cube = (yar_asset_cubemap*)std::malloc(sizeof(yar_asset_cubemap));
    cube->width = (uint32_t)width;
    cube->height = (uint32_t)height;
    cube->channels = (uint32_t)channels;
    cube->pixels = all_pixels;

    yar_asset_entry* entry = &asset_manager->assets[asset_manager->count++];
    entry->hash = h;
    entry->type = yar_asset_type_cubemap;
    entry->asset = cube;

    return cube;
}

void yar_request_asset(yar_asset_type type, const char* path)
{
    yar_asset_request req{};
    req.type = type;
    strncpy(req.path, path, sizeof(req.path));
    req.hash = yar_hash(path);

    {
        std::lock_guard<std::mutex> lock(asset_manager->req_mutex);
        asset_manager->requests.push(req);
    }
    asset_manager->req_cv.notify_one();
}

void yar_request_asset(yar_asset_type type, const char* faces[6])
{
    yar_asset_request req{};
    req.type = type;

    for (int i = 0; i < 6; i++)
    {
        strncpy(req.cubemap_faces[i], faces[i], sizeof(req.cubemap_faces[i]));
        req.cubemap_faces[i][sizeof(req.cubemap_faces[i]) - 1] = '\0';
    }

    std::string combined;
    for (int i = 0; i < 6; i++)
    {
        combined += faces[i];
        combined += ";";
    }
    req.hash = yar_hash(combined.c_str());

    {
        std::lock_guard<std::mutex> lock(asset_manager->req_mutex);
        asset_manager->requests.push(req);
    }
    asset_manager->req_cv.notify_one();
}

void* yar_find_loaded_asset(yar_asset_type type, const char* path)
{
    uint64_t hash = yar_hash(path);

    for (uint32_t i = 0; i < asset_manager->count; ++i)
    {
        auto& entry = asset_manager->assets[i];
        if (entry.hash == hash && entry.type == type)
        {
            return entry.asset; 
        }
    }

    return nullptr;
}

yar_asset_texture* yar_get_builtin(yar_builtin_asset id)
{
    if (!asset_manager) return nullptr;
    if (id < 0 || id >= yar_builtin_count) return nullptr;
    return (yar_asset_texture*)asset_manager->builtin[id].asset;
}

void init_asset_manager()
{
	if (asset_manager == nullptr)
	{
		asset_manager = static_cast<yar_asset_manager*>(
			std::malloc(sizeof(yar_asset_manager))
		);
        new (asset_manager) yar_asset_manager;
        asset_manager->count = 0;
        asset_manager->running = true;

        asset_manager->builtin[yar_builtin_white] = {
            0, yar_asset_type_texture,
            yar_create_solid_texture(255, 255, 255, 255)
        };

        asset_manager->builtin[yar_builtin_black] = {
            0, yar_asset_type_texture,
            yar_create_solid_texture(0, 0, 0, 255)
        };

        asset_manager->builtin[yar_builtin_checker] = {
            0, yar_asset_type_texture,
            yar_create_checker_texture()
        };

        uint32_t worker_count = std::thread::hardware_concurrency();
        for (int i = 0; i < worker_count; i++) {
            asset_manager->workers.emplace_back([am = asset_manager]()
                {
                    while (am->running) 
                    {
                        yar_asset_request req;
                        {
                            std::unique_lock<std::mutex> lock(am->req_mutex);
                            am->req_cv.wait(lock, [&]
                                {
                                    return !am->requests.empty() || !am->running;
                                }
                            );
                            if (!am->running) return;

                            req = am->requests.front();
                            am->requests.pop();
                        }


                        yar_asset_ready r{};
                        r.hash = req.hash;
                        r.type = req.type;

                        if (req.type == yar_asset_type_texture) 
                        {
                            r.asset = yar_load_asset_texture(req.path);
                        }
                        else if (req.type == yar_asset_type_cubemap)
                        {
                            r.asset = yar_load_asset_cubemap(req.cubemap_faces);
                        }
                        //else if (req.type == yar_asset_type_model) {
                        //    r.asset = yar_load_asset_model(req.path);
                        //}

                        {
                            std::lock_guard<std::mutex> lock(am->ready_mutex);
                            am->ready.push(r);
                        }
                    }
                }
            );
        }
	}
}

void terminate_asset_manager()
{
    asset_manager->running = false;
    asset_manager->req_cv.notify_all();
    for (auto& w : asset_manager->workers)
    {
        if (w.joinable()) w.join();
    }
}
