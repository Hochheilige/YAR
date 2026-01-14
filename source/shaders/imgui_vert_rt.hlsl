#define SPHERES_COUNT 5

struct Material
{
    float3 albedo;
    float fuzz; // only for Metals
    float refraction_index; // only for Dielectrics
    uint type;
};

struct Sphere
{
    float3 center;
    float radius;
};

cbuffer ubo : register(b1, space1)
{
    float4x4 invViewProj;
    float4x4 ui_ortho;
    float4 camera_pos;
    Sphere spheres[SPHERES_COUNT];
    Material mats[SPHERES_COUNT];
    int samples_per_pixel;
    int max_ray_depth;
    float seed;   
};

struct VSInput
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = mul(ui_ortho, float4(input.pos.xy, 0.0f, 1.0f));
    output.uv = input.uv;
    output.col = input.col;
    return output;
}
