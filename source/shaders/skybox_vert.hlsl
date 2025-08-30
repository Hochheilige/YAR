#include "common.hlsli"

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 tex_coord : TEXCOORD;
};

VSOutput main(VSInput input) {
    VSOutput output;
    float4 pos = mul(mul(mvp.proj, mvp.view_sb), float4(input.position, 1.0f));
    output.position = pos.xyww;

    output.tex_coord = input.position;
    return output;
}