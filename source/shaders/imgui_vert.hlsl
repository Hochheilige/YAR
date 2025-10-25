#include "common.hlsli"

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
    output.pos = mul(mvp.ui_ortho, float4(input.pos.xy, 0.0f, 1.0f));
    output.uv = input.uv;
    output.col = input.col;
    return output;
}
