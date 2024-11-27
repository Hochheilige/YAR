#include "common.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float3 frag_pos : POSITION0;
    float2 tex_coord : TEXCOORD;
    float3 normal : NORMAL;
};

Texture2D<float4> diffuse_map : register(t0, space0);
Texture2D<float4> specular_map : register(t1, space0);
SamplerState samplerState : register(s0, space0);

struct LightCalculationParams
{
    float4 diffuse_map_color;
    float4 specular_map_color;
    float4 norm;
    float4 frag_pos;
    float4 cam_pos;
    float4 ambient;
    float4 diffuse;
    float4 specular;
};

float4 calculate_dir_light(float4 light_dir, const LightCalculationParams lcp)
{
    float4 diffuse_map_color  = lcp.diffuse_map_color;
    float4 specular_map_color = lcp.specular_map_color;
    float4 norm               = lcp.norm;
    float4 frag_pos           = lcp.frag_pos;
    float4 cam_pos            = lcp.cam_pos;
    float4 light_amb          = lcp.ambient;
    float4 light_diff         = lcp.diffuse;
    float4 light_spec         = lcp.specular;

    float4 ambient = diffuse_map_color * light_amb;
    
    light_dir = normalize(light_dir);
    float diff = max(dot(norm, light_dir), 0.0f);
    float4 diffuse = light_diff * diff * diffuse_map_color;

    float4 view_dir = normalize(cam_pos - frag_pos);
    float4 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 64);
    float4 specular = light_spec * specular_map_color * spec;

    return ambient + diffuse + specular;
}

float4 calculate_point_light(float4 position, float4 attenuation_params, const LightCalculationParams lcp)
{
    float4 diffuse_map_color  = lcp.diffuse_map_color;
    float4 specular_map_color = lcp.specular_map_color;
    float4 norm               = lcp.norm;
    float4 frag_pos           = lcp.frag_pos;
    float4 cam_pos            = lcp.cam_pos;
    float4 light_amb          = lcp.ambient;
    float4 light_diff         = lcp.diffuse;
    float4 light_spec         = lcp.specular;
    float con                 = attenuation_params.x;
    float lin                 = attenuation_params.y;
    float quadr               = attenuation_params.z;

    float distance = length(position - frag_pos);
    float attenuation = 1.0f / (con  + lin * distance + quadr * (distance * distance)); 

    float4 ambient = diffuse_map_color * light_amb * attenuation;

    float4 light_dir = normalize(position - frag_pos);
    float diff = max(dot(norm, light_dir), 0.0f);
    float4 diffuse = light_diff * diff * diffuse_map_color * attenuation;

    float4 view_dir = normalize(cam_pos - frag_pos);
    float4 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 64);
    float4 specular = light_spec * specular_map_color * spec * attenuation;

    return ambient + diffuse + specular;
}

float4 calculate_spot_light(float4 position, float4 direction, float4 cutoff, float4 attenuation_params, const LightCalculationParams lcp)
{
    float4 diffuse_map_color  = lcp.diffuse_map_color;
    float4 specular_map_color = lcp.specular_map_color;
    float4 norm               = lcp.norm;
    float4 frag_pos           = lcp.frag_pos;
    float4 cam_pos            = lcp.cam_pos;
    float4 light_amb          = lcp.ambient;
    float4 light_diff         = lcp.diffuse;
    float4 light_spec         = lcp.specular;
    float con                 = attenuation_params.x;
    float lin                 = attenuation_params.y;
    float quadr               = attenuation_params.z;

    float4 light_dir = normalize(position - frag_pos);
    float theta = dot(light_dir, normalize(-direction));
    float epsilon   = cutoff.x - cutoff.y;
    float intensity = clamp((theta - cutoff.y) / epsilon, 0.0, 1.0);   

    float distance = length(position - frag_pos);
    float attenuation = 1.0f / (con  + lin * distance + quadr * (distance * distance)); 

    float4 ambient = diffuse_map_color * light_amb;

    float diff = max(dot(norm, light_dir), 0.0f);
    float4 diffuse = light_diff * diff * diffuse_map_color * attenuation * intensity;
    
    float4 view_dir = normalize(cam_pos - frag_pos);
    float4 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 64);
    float4 specular = light_spec * specular_map_color * spec * attenuation * intensity;

    return ambient + diffuse + specular;
}

float4 main(PSInput input) : SV_TARGET {
    LightCalculationParams lcp;
    lcp.diffuse_map_color  = diffuse_map.Sample(samplerState, input.tex_coord);
    lcp.specular_map_color = specular_map.Sample(samplerState, input.tex_coord);
    lcp.norm               = float4(normalize(input.normal), 0.0f);
    lcp.frag_pos           = float4(input.frag_pos, 0.0f);
    lcp.cam_pos            = cam.pos;
    Material mt = material[index]; 

    float4 light_contribution = float4(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < DIR_LIGHT_COUNT; ++i)
    {
        lcp.ambient  = light_params.ambient[i];
        lcp.diffuse  = light_params.diffuse[i];
        lcp.specular = light_params.specular[i];
        light_contribution += calculate_dir_light(dir_light.direction[i], lcp);
    }

    for (i = DIR_LIGHT_COUNT; i < DIR_LIGHT_COUNT + POINT_LIGHT_COUNT; ++i)
    {
        lcp.ambient  = light_params.ambient[i];
        lcp.diffuse  = light_params.diffuse[i];
        lcp.specular = light_params.specular[i];

        light_contribution += 
            calculate_point_light(
                point_light.position[i - DIR_LIGHT_COUNT],
                point_light.attenuation[i - DIR_LIGHT_COUNT], 
                lcp
            );
    }

    for (i = DIR_LIGHT_COUNT + POINT_LIGHT_COUNT; i < LS_COUNT; ++i)
    {
        lcp.ambient  = light_params.ambient[i];
        lcp.diffuse  = light_params.diffuse[i];
        lcp.specular = light_params.specular[i];

        uint index = i - DIR_LIGHT_COUNT - POINT_LIGHT_COUNT;
        light_contribution +=
            calculate_spot_light(
                spot_light.position[index],
                spot_light.direction[index],
                spot_light.cutoff[index],
                spot_light.attenuation[index],
                lcp
            );
    }

    // For now index == 0 is an index of a light source cube so I just color it with
    // its light color
    // FIX ME
    float4 res = lerp(light_params.specular[1], light_contribution, index > 0);

    return res;
}