struct VSInput {
    float3 position : POSITION;
    float2 tex_coord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 tex_coord : TEXCOORD;
};

struct MVP
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
cbuffer ubo : register(b1, space2)
{
    MVP mvp;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(mul(mul(mvp.proj, mvp.view), mvp.model), float4(input.position, 1.0f));
    output.tex_coord = input.tex_coord;
    return output;
}