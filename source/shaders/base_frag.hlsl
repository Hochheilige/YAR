struct PSInput {
    float4 position : SV_POSITION;
};

cbuffer c_color : register(b0)
{
    float3 rgb;
    float padding;
};

float4 main(PSInput input) : SV_TARGET {
    return float4(rgb, 1.0); 
}