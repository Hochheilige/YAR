#include "mesh_asset.h"

void MeshAsset::ensure_gpu_resources()
{
	if (has_gpu_resources())
		return;

	if (cpu_vertices.empty() || cpu_indices.empty())
		return;

	yar_buffer_desc buffer_desc{};
	buffer_desc.size = cpu_vertices.size();
	buffer_desc.flags = yar_buffer_flag_gpu_only;
	buffer_desc.name = "mesh_vertex_buffer";
	add_buffer(&buffer_desc, &vertex_buffer);

	buffer_desc.size = cpu_indices.size() * sizeof(uint32_t);
	buffer_desc.name = "mesh_index_buffer";
	add_buffer(&buffer_desc, &index_buffer);

	yar_resource_update_desc resource_update_desc;
	yar_buffer_update_desc buf_update_desc{};
	resource_update_desc = &buf_update_desc;

	buf_update_desc.buffer = vertex_buffer;
	buf_update_desc.size = cpu_vertices.size();
	begin_update_resource(resource_update_desc);
	std::memcpy(buf_update_desc.mapped_data, cpu_vertices.data(), buf_update_desc.size);
	end_update_resource(resource_update_desc);

	buf_update_desc.buffer = index_buffer;
	buf_update_desc.size = cpu_indices.size() * sizeof(uint32_t);
	begin_update_resource(resource_update_desc);
	std::memcpy(buf_update_desc.mapped_data, cpu_indices.data(), buf_update_desc.size);
	end_update_resource(resource_update_desc);

	cpu_vertices.clear();
	cpu_vertices.shrink_to_fit();
	cpu_indices.clear();
	cpu_indices.shrink_to_fit();
}

void MeshAsset::bind(yar_cmd_buffer* cmd, uint32_t vertex_stride) const
{
	cmd_bind_vertex_buffer(cmd, vertex_buffer, layout.attrib_count, 0, vertex_stride);
	cmd_bind_index_buffer(cmd, index_buffer);
}

void MeshAsset::draw(yar_cmd_buffer* cmd) const
{
	cmd_draw_indexed(cmd, index_count, yar_index_type_uint, 0, 0);
}

void MeshAsset::bind_and_draw(yar_cmd_buffer* cmd, uint32_t vertex_stride) const
{
	if (!vertex_buffer || !index_buffer || index_count == 0)
		return;

	bind(cmd, vertex_stride);
	draw(cmd);
}
