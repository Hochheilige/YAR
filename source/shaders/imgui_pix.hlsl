Texture2D font : register(t0);
SamplerState samplerState : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

float4 main(PS_INPUT input) : SV_Target
{
    float4 texColor = font.Sample(samplerState, input.uv);
    return texColor * input.col;
}
