#include "common.hlsli"

struct PSInput {
    float4 position             : SV_POSITION;
    float3 frag_pos             : POSITION0;
    float4 frag_pos_light_space : POSITION1;
    float2 tex_coord            : TEXCOORD0;
    float2 tex_coord1           : TEXCOORD1;
    float3 tangent              : TEXCOORD2;
    float3 bitangent            : TEXCOORD3;
    float3 normal               : TEXCOORD4;
};

Texture2D<float4> diffuse_map : register(t0, space0);
Texture2D<float> roughness_map : register(t1, space0);
Texture2D<float> metalness_map : register(t2, space0);
Texture2D<float4> normal_map : register(t3, space0);
SamplerState samplerState : register(s0, space0);

Texture2D<float> shadow_map : register(t4, space1);
SamplerState smSampler : register(s1, space1);

struct LightCalculationParams
{
    float4 diffuse_map_color;
    float specular_map_color;
    float4 color;
    float3 frag_pos;
    float3 cam_pos;
    float3 norm;
    float intensity;
    float radius;
    float4 frag_pos_light_space;
};

float calculate_shadow(float4 frag_pos_light_space, float3 normal, float3 light_dir)
{
    float3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5f + 0.5f;
    if (proj_coords.z > 1.0f)
        return 0.0f;

    float current_depth = proj_coords.z;
    float bias = max(0.05 * (1.0f - dot(normal, -light_dir)), 0.005);
    float shadow = 0.0f;
    uint width, height;
    shadow_map.GetDimensions(width, height); 
    float2 texel_size = 1.0f / float2(width, height);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcf_depth = shadow_map.Sample(smSampler, proj_coords.xy + float2(x, y) * texel_size).r;
            shadow += (current_depth - bias) > pcf_depth ? 1.0f : 0.0f;
        }
    }
    shadow /= 9.0f;


    return shadow;
}

float4 calculate_dir_light(float3 light_dir, const LightCalculationParams lcp)
{
    float4 diffuse_map_color  = lcp.diffuse_map_color;
    float specular_map_color = lcp.specular_map_color;
    float4 color              = lcp.color;
    float3 frag_pos           = lcp.frag_pos;
    float3 cam_pos            = lcp.cam_pos;
    float3 norm               = lcp.norm;
    float intensity           = lcp.intensity;
    float4 frag_pos_light_space = lcp.frag_pos_light_space;

    float4 light_color = color * intensity;
    light_dir = normalize(light_dir);
    float diff = saturate(dot(norm, -light_dir));
    float4 diffuse = diff * diffuse_map_color * light_color;

    float3 view_dir = normalize(cam_pos - frag_pos);
    float3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(norm, halfway_dir), 0.0), 64);
    float4 specular = specular_map_color * spec * light_color;

    float4 ambient = 0.15f * diffuse_map_color * light_color;
    float shadow = calculate_shadow(frag_pos_light_space, norm, light_dir);

    return ambient + (1.0 - shadow) * (diffuse + specular);
}

float4 calculate_point_light(float4 position, const LightCalculationParams lcp)
{
    float4 diffuse_map_color  = lcp.diffuse_map_color;
    float specular_map_color = lcp.specular_map_color;
    float4 color              = lcp.color;
    float3 cam_pos            = lcp.cam_pos;
    float3 frag_pos           = lcp.frag_pos;
    float3 norm               = lcp.norm;
    float intensity           = lcp.intensity;

    float distance = length(position.xyz - frag_pos);
    float attenuation = 1.0f / (distance + distance * distance); 

    float4 light_color = color * intensity;
    
    float3 light_dir = normalize(position.xyz - frag_pos);
    float diff = saturate(dot(norm, light_dir));
    float4 diffuse = diff * diffuse_map_color * attenuation * light_color;

    float3 view_dir = normalize(cam_pos - frag_pos);
    float3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(norm, halfway_dir), 0.0), 64);
    float4 specular = specular_map_color * spec * attenuation * light_color;

    return diffuse + specular;
}

float4 calculate_spot_light(float4 position, float3 direction, float4 cutoff, float4 attenuation_params, const LightCalculationParams lcp)
{
    float4 diffuse_map_color  = lcp.diffuse_map_color;
    float specular_map_color = lcp.specular_map_color;
    float4 color              = lcp.color;
    float3 frag_pos           = lcp.frag_pos;
    float3 norm               = lcp.norm;
    float3 cam_pos            = lcp.cam_pos;
    float con                 = attenuation_params.x;
    float lin                 = attenuation_params.y;
    float quadr               = attenuation_params.z;
    float ext_intensity       = lcp.intensity;

    float3 light_dir = normalize(position.xyz - frag_pos);
    float theta = dot(light_dir, normalize(-direction));
    float epsilon   = cutoff.x - cutoff.y;
    float intensity = ext_intensity > 0.0f ? clamp((theta - cutoff.y) / epsilon, 0.0, 1.0) : 0.0f;   

    float distance = length(position.xyz - frag_pos);
    float attenuation = 1.0f / (con  + lin * distance + quadr * (distance * distance)); 

    float diff = saturate(dot(norm, light_dir));
    float4 diffuse = diff * diffuse_map_color * attenuation * intensity * color;
    
    float3 view_dir = normalize(cam_pos - frag_pos);
    float3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(saturate(dot(norm, halfway_dir)), 64);
    float4 specular = specular_map_color * spec * attenuation * intensity * color;

    return diffuse + specular;
}

float4 main(PSInput input) : SV_TARGET {
    LightCalculationParams lcp;
    lcp.diffuse_map_color  = diffuse_map.Sample(samplerState, input.tex_coord);
    if (lcp.diffuse_map_color.a < 0.1f)
        discard;
    lcp.specular_map_color = lerp(0.04f, 1.0f, metalness_map.Sample(samplerState, input.tex_coord1).r)
     * (1.0f - roughness_map.Sample(samplerState, input.tex_coord1).r);
    lcp.norm = normal_map.Sample(samplerState, input.tex_coord).rgb;
    float3x3 tbn = float3x3(input.tangent, input.bitangent, input.normal);
    lcp.norm = normalize(mul(lcp.norm, tbn));
    lcp.frag_pos           = input.frag_pos;
    lcp.cam_pos            = cam.pos.xyz;
    lcp.frag_pos_light_space = input.frag_pos_light_space;

    float4 light_contribution = float4(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < DIR_LIGHT_COUNT; ++i)
    {
        lcp.color = float4(light_params.color[i].rgb, 0.0f);
        lcp.intensity = light_params.color[i].a;
        light_contribution += calculate_dir_light(dir_light.direction[i].xyz, lcp);
    }

    for (uint i = DIR_LIGHT_COUNT; i < DIR_LIGHT_COUNT + POINT_LIGHT_COUNT; ++i)
    {
        lcp.color = float4(light_params.color[i].rgb, 0.0f);
        lcp.intensity = light_params.color[i].a;
        light_contribution += 
            calculate_point_light(
                point_light.position[i - DIR_LIGHT_COUNT],
                lcp
            );
    }

    for (uint i = DIR_LIGHT_COUNT + POINT_LIGHT_COUNT; i < LS_COUNT; ++i)
    {
        lcp.color = float4(light_params.color[i].rgb, 0.0f);
        lcp.intensity = light_params.color[i].a;

        uint index = i - DIR_LIGHT_COUNT - POINT_LIGHT_COUNT;
        light_contribution +=
            calculate_spot_light(
                spot_light.position[index],
                spot_light.direction[index].xyz,
                spot_light.cutoff[index],
                spot_light.attenuation[index],
                lcp
            );
    }

    // For now index == 0 is an index of a light source cube so I just color it with
    // its light color
    // FIX ME
    float4 res = lerp(light_params.color[1], light_contribution, index > 0);

    return res;
}
