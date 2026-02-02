#include "common.h"

struct VSInput {
    float3 position   : POSITION;
    float3 normal     : NORMAL;
    float3 tangent    : TANGENT;
    float3 bitangent  : BITANGENT;
    float2 tex_coord  : TEXCOORD0;
    float2 tex_coord1 : TEXCOORD1;
};

struct VSOutput {
    float4 position             : SV_POSITION;
    float3 frag_pos             : POSITION0;
    float4 frag_pos_light_space : POSITION1;
    float2 tex_coord            : TEXCOORD0;
    float2 tex_coord1           : TEXCOORD1;
    float3 tangent              : TEXCOORD2;
    float3 bitangent            : TEXCOORD3;
    float3 normal               : TEXCOORD4;
};

VSOutput main(VSInput input) {
    VSOutput output;
    float4x4 model = mvp.model[index];
    output.position = mul(mul(mul(mvp.proj, mvp.view), model), float4(input.position, 1.0f));
    output.frag_pos = mul(model, float4(input.position, 1.0f));
    output.tex_coord = input.tex_coord;
    output.tex_coord1 = input.tex_coord1;
    output.normal = normalize(mul(transpose(inverse(model)), float4(input.normal, 1.0f))); 
    output.tangent = normalize(mul(transpose(inverse(model)), float4(input.tangent, 1.0f)));
    output.bitangent = normalize(mul(transpose(inverse(model)), float4(input.bitangent, 1.0f)));
    output.frag_pos_light_space = mul(mul(mvp.light_space, model), float4(input.position, 1.0f));
    return output;
}
