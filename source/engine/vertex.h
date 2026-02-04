#pragma once

#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"

struct VertexStatic
{
	Vector3 position;
	Vector3 normal;
	Vector3 tangent;
	Vector3 bitangent;
	Vector2 uv0;
	Vector2 uv1;
};

struct VertexUnlit
{
	Vector3 position;
	Vector2 uv;
	Vector4 color;
};

struct VertexSkybox
{
	Vector3 position;
};
