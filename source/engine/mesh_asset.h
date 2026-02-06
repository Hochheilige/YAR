#pragma once

#include "render.h"
#include "vertex.h"

#include <vector>
#include <cstdint>
#include <cstring>

struct MeshAsset
{
	std::vector<uint8_t> cpu_vertices;
	std::vector<uint32_t> cpu_indices;
	uint32_t vertex_stride = 0;

	yar_buffer* vertex_buffer = nullptr;
	yar_buffer* index_buffer = nullptr;
	uint32_t index_count = 0;
	uint32_t vertex_count = 0;
	yar_vertex_layout layout{};

	bool has_gpu_resources() const { return vertex_buffer != nullptr; }
	void ensure_gpu_resources();
	void bind(yar_cmd_buffer* cmd, uint32_t vertex_stride) const;
	void draw(yar_cmd_buffer* cmd) const;
	void bind_and_draw(yar_cmd_buffer* cmd, uint32_t vertex_stride) const;
};

template<typename VertexType>
MeshAsset create_mesh_asset(
	const std::vector<VertexType>& vertices,
	const std::vector<uint32_t>& indices,
	const yar_vertex_layout& layout)
{
	MeshAsset asset;
	asset.layout = layout;
	asset.index_count = static_cast<uint32_t>(indices.size());
	asset.vertex_count = static_cast<uint32_t>(vertices.size());
	asset.vertex_stride = sizeof(VertexType);

	asset.cpu_vertices.resize(vertices.size() * sizeof(VertexType));
	std::memcpy(asset.cpu_vertices.data(), vertices.data(), asset.cpu_vertices.size());

	asset.cpu_indices = indices;

	return asset;
}
