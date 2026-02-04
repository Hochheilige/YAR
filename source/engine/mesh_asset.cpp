#include "mesh_asset.h"

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
