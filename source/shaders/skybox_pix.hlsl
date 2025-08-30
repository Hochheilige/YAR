#include "common.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float3 tex_coord : TEXCOORD;
};

TextureCube skybox : register(t0, space0);
SamplerState samplerState : register(s0, space0);

float4 main(PSInput input) : SV_TARGET 
{
    return skybox.Sample(samplerState, input.tex_coord);
}