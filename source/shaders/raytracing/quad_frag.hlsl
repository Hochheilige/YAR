Texture2D quad_tex : register(t0, space0);
SamplerState samplerState : register(s0, space0);

struct PSInput {
    float2 texCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET {
    return quad_tex.Sample(samplerState, input.texCoord);
}
