#include "vertex.h"

#include <cstddef>

void VertexStatic::setup_layout(yar_vertex_layout& layout)
{
	layout.attrib_count = 6u;
	layout.attribs[0] = { 3u, yar_attrib_format_float, offsetof(VertexStatic, position) };
	layout.attribs[1] = { 3u, yar_attrib_format_float, offsetof(VertexStatic, normal) };
	layout.attribs[2] = { 3u, yar_attrib_format_float, offsetof(VertexStatic, tangent) };
	layout.attribs[3] = { 3u, yar_attrib_format_float, offsetof(VertexStatic, bitangent) };
	layout.attribs[4] = { 2u, yar_attrib_format_float, offsetof(VertexStatic, uv0) };
	layout.attribs[5] = { 2u, yar_attrib_format_float, offsetof(VertexStatic, uv1) };
}

void VertexUnlit::setup_layout(yar_vertex_layout& layout)
{
	layout.attrib_count = 3u;
	layout.attribs[0] = { 3u, yar_attrib_format_float, offsetof(VertexUnlit, position) };
	layout.attribs[1] = { 2u, yar_attrib_format_float, offsetof(VertexUnlit, uv) };
	layout.attribs[2] = { 4u, yar_attrib_format_float, offsetof(VertexUnlit, color) };
}

void VertexSkybox::setup_layout(yar_vertex_layout& layout)
{
	layout.attrib_count = 1u;
	layout.attribs[0] = { 3u, yar_attrib_format_float, offsetof(VertexSkybox, position) };
}
