#ifdef __cplusplus
#pragma once
#include <cstdint>
#include <cstddef>
#include "math/yar_math.h"

using float3 = Vector3;
using float4 = Vector4;
using uint   = uint32_t;
using float4x4 = Matrix4x4;
struct uint2 { uint x, y; };

#define YAR_ENUM(name) enum class name : uint
#else
#define YAR_ENUM(name) enum name : uint
#endif

YAR_ENUM(MaterialType)
{
    Lambertian = 0,
    Metal      = 1,
    Dielectric = 2,
};

YAR_ENUM(PrimitiveType)
{
	Sphere 	= 0,
	Quad 	= 1,
};

struct PrimitiveRef
{
	PrimitiveType type;
	uint index;
};

struct Sphere
{
    float3 center;
    float  radius;
};

struct Quad
{
	float3 corner;
	float plane_d;
	float3 normal;
	float pad1;
	float3 dual_u;
	float pad2;
	float3 dual_v;
	float pad3;
};

struct BVHNode
{
	float3 aabb_min;
	uint left_first;
	float3 aabb_max;

	/*
		if prim_count == 0 then left_first -> index of the left child node
		otherwise left_first -> index of the first primitive index
	*/
	uint prim_count;
};

struct MaterialData
{
    float3       albedo;
    float        fuzz;
    float        refraction_index;
    MaterialType type;
    uint2        pad_;
};

struct UBO
{
	float4x4 inv_view_proj;
	float4x4 ui_ortho;
	float4   camera_pos;
	int  samples_per_pixel;
	int max_ray_depth;
	uint seed;
	uint use_bvh;
};

#ifdef __cplusplus
// Everything above crosses into the shader, where the layout is fixed by
// std430 for the storage buffers and std140 for the uniform block. A mismatch
// is silent: no warning, no error, just wrong pixels or garbage traversal.
// Make it a build failure instead.

static_assert(sizeof(MaterialType) == 4, "MaterialType must stay a 4-byte uint");

static_assert(sizeof(Sphere) == 16, "Sphere must stay 16 bytes");
static_assert(offsetof(Sphere, center) == 0, "Sphere::center moved");
static_assert(offsetof(Sphere, radius) == 12, "Sphere::radius moved");

static_assert(sizeof(PrimitiveRef) == 8, "PrimitiveRef must stay 8 bytes");
static_assert(offsetof(PrimitiveRef, type) == 0, "PrimitiveRef::type moved");
static_assert(offsetof(PrimitiveRef, index) == 4, "PrimitiveRef::index moved");

static_assert(sizeof(Quad) == 64, "Quad must stay 64 bytes");
static_assert(offsetof(Quad, corner) == 0, "Quad::corner moved");
static_assert(offsetof(Quad, plane_d) == 12, "Quad::plane_d moved");
static_assert(offsetof(Quad, normal) == 16, "Quad::normal moved");
static_assert(offsetof(Quad, pad1) == 28, "Quad::pad1 moved");
static_assert(offsetof(Quad, dual_u) == 32, "Quad::dual_u moved");
static_assert(offsetof(Quad, pad2) == 44, "Quad::pad2 moved");
static_assert(offsetof(Quad, dual_v) == 48, "Quad::dual_v moved");
static_assert(offsetof(Quad, pad3) == 60, "Quad::pad3 moved");

static_assert(sizeof(MaterialData) == 32, "MaterialData must stay 32 bytes");
static_assert(offsetof(MaterialData, albedo) == 0, "MaterialData::albedo moved");
static_assert(offsetof(MaterialData, fuzz) == 12, "MaterialData::fuzz moved");
static_assert(offsetof(MaterialData, refraction_index) == 16, "MaterialData::refraction_index moved");
static_assert(offsetof(MaterialData, type) == 20, "MaterialData::type moved");
static_assert(offsetof(MaterialData, pad_) == 24, "MaterialData::pad_ moved");

static_assert(sizeof(UBO) == 160, "UBO must stay 160 bytes");
static_assert(offsetof(UBO, inv_view_proj) == 0, "UBO::inv_view_proj moved");
static_assert(offsetof(UBO, ui_ortho) == 64, "UBO::ui_ortho moved");
static_assert(offsetof(UBO, camera_pos) == 128, "UBO::camera_pos moved");
static_assert(offsetof(UBO, samples_per_pixel) == 144, "UBO::samples_per_pixel moved");
static_assert(offsetof(UBO, max_ray_depth) == 148, "UBO::max_ray_depth moved");
static_assert(offsetof(UBO, seed) == 152, "UBO::seed moved");
static_assert(offsetof(UBO, use_bvh) == 156, "UBO::use_bvh moved");

static_assert(sizeof(BVHNode) == 32, "BVHNode must stay 32 bytes");
static_assert(offsetof(BVHNode, left_first) == 12, "BVHNode::left_first moved");
static_assert(offsetof(BVHNode, aabb_max) == 16, "BVHNode::aabb_max moved");
static_assert(offsetof(BVHNode, prim_count) == 28, "BVHNode::prim_count moved");
#endif
