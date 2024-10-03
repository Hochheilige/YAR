struct PSInput {
    float4 position : SV_POSITION;
    float3 frag_pos : POSITION0;
    float2 tex_coord : TEXCOORD;
    float3 normal : NORMAL;
};

Texture2D<float4> diffuse_map : register(t0, space0);
Texture2D<float4> specular_map : register(t1, space0);
SamplerState samplerState : register(s0, space0);

struct MVP
{
    float4x4 view;
    float4x4 proj;
    float4x4 model[10];
};

struct Material
{
    float shinines;
};

struct LightSource
{
    float4 position[2];
    float4 ambient[2];
    float4 diffuse[2];
    float4 specular[2];
};

struct Camera
{
    float4 pos;
};

cbuffer push_constant : register(b0, space2)
{
    uint index;
};

cbuffer ubo : register(b1, space1)
{
    MVP mvp;
    LightSource ls;
    Camera cam;
};

cbuffer mat : register(b2, space0)
{
    Material material[10];
};

float4 main(PSInput input) : SV_TARGET {
    float4 diffuse_map_color = diffuse_map.Sample(samplerState, input.tex_coord);
    float4 specular_map_color = specular_map.Sample(samplerState, input.tex_coord);
    Material mater = material[index];

    float4 mat = float4(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < 2; ++i)
    {
        float4 light_pos = ls.position[i];
        float4 light_diff = ls.diffuse[i];
        float4 light_spec = ls.specular[i];

        float4 norm = float4(normalize(input.normal), 0.0f);
        float4 frag_pos = float4(input.frag_pos, 0.0f);
        // This component is 0.0f for directional light
        float dir_light_component = light_pos.w;

        // calculate ambient ligth
        float4 ambient = diffuse_map_color * ls.ambient[i];

        // calculate diffuse light
        float4 light_dir = normalize(lerp(light_pos, 
                                light_pos - frag_pos, 
                                dir_light_component > 0)
                            );

        float diff = max(dot(norm, light_dir), 0.0f);
        float4 diffuse = light_diff * diff * diffuse_map_color;

        // calculate specular light
        float4 view_dir = normalize(cam.pos - frag_pos);
        float4 reflect_dir = reflect(-light_dir, norm);
        float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 64);
        float4 specular = light_spec * specular_map_color * spec;
        mat += (ambient + diffuse + specular);
    }

    // For now index == 0 is an index of a light source cube so I just color it with
    // its light color
    // FIX ME
    float4 res = lerp(ls.specular[0], mat, index > 0);

    return res;
}