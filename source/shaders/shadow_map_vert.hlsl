#include "common.h"

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(mul(mvp.light_space, mvp.model[index]), float4(input.position, 1.0f));
    return output;
}