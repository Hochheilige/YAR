struct VSInput {
    float3 position : POSITION;
    float2 tex_coord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 tex_coord : TEXCOORD;
};

struct CameraUniform
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
cbuffer camera : register(b1, space1)
{
    CameraUniform ubo;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(mul(mul(ubo.proj, ubo.view), ubo.model), float4(input.position, 1.0f));
    output.tex_coord = input.tex_coord;
    return output;
}