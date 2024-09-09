struct VSInput {
    float3 position : POSITION;
    float3 color : COLOR;
    float2 tex_coord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float2 tex_coord : TEXCOORD;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    output.tex_coord = input.tex_coord;
    return output;
}