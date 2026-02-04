#pragma once

#include "render.h"
#include "vertex.h"

#include <vector>
#include <cstdint>
#include <cstring>

struct MeshAsset
{
	yar_buffer* vertex_buffer = nullptr;
	yar_buffer* index_buffer = nullptr;
	uint32_t index_count = 0;
	uint32_t vertex_count = 0;
	yar_vertex_layout layout{};

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

	yar_buffer_desc buffer_desc{};
	buffer_desc.size = vertices.size() * sizeof(VertexType);
	buffer_desc.flags = yar_buffer_flag_gpu_only;
	buffer_desc.name = "mesh_vertex_buffer";
	add_buffer(&buffer_desc, &asset.vertex_buffer);

	buffer_desc.size = indices.size() * sizeof(uint32_t);
	buffer_desc.name = "mesh_index_buffer";
	add_buffer(&buffer_desc, &asset.index_buffer);

	yar_resource_update_desc resource_update_desc;
	yar_buffer_update_desc buf_update_desc{};
	resource_update_desc = &buf_update_desc;

	buf_update_desc.buffer = asset.vertex_buffer;
	buf_update_desc.size = vertices.size() * sizeof(VertexType);
	begin_update_resource(resource_update_desc);
	std::memcpy(buf_update_desc.mapped_data, vertices.data(), buf_update_desc.size);
	end_update_resource(resource_update_desc);

	buf_update_desc.buffer = asset.index_buffer;
	buf_update_desc.size = indices.size() * sizeof(uint32_t);
	begin_update_resource(resource_update_desc);
	std::memcpy(buf_update_desc.mapped_data, indices.data(), buf_update_desc.size);
	end_update_resource(resource_update_desc);

	return asset;
}
