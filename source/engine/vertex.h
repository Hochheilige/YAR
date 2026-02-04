#pragma once

#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "render.h"

struct VertexStatic
{
	Vector3 position;
	Vector3 normal;
	Vector3 tangent;
	Vector3 bitangent;
	Vector2 uv0;
	Vector2 uv1;

	static void setup_layout(yar_vertex_layout& layout);
};

struct VertexUnlit
{
	Vector3 position;
	Vector2 uv;
	Vector4 color;

	static void setup_layout(yar_vertex_layout& layout);
};

struct VertexSkybox
{
	Vector3 position;

	static void setup_layout(yar_vertex_layout& layout);
};
