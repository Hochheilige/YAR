#include "common.hlsli"

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 frag_pos : POSITION0;
    float2 tex_coord : TEXCOORD;
    float3 normal : NORMAL;
    float4 frag_pos_light_space : POSITION1;
};

VSOutput main(VSInput input) {
    VSOutput output;
    float4x4 model = mvp.model[index];
    output.position = mul(mul(mul(mvp.proj, mvp.view), model), float4(input.position, 1.0f));
    output.frag_pos = mul(model, float4(input.position, 1.0f));
    output.tex_coord = input.tex_coord;
    output.normal = normalize(mul(transpose(inverse(model)), float4(input.normal, 1.0f)));
    output.frag_pos_light_space = mul(mul(mvp.light_space, model), float4(input.position, 1.0f));
    return output;
}