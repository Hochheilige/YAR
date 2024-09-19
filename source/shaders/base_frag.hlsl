struct PSInput {
    float4 position : SV_POSITION;
    float3 frag_pos : POSITION0;
    float2 tex_coord : TEXCOORD;
    float3 normal : NORMAL;
};

Texture2D<float3> logo : register(t0, space0);
SamplerState samplerState : register(s0, space0);

struct MVP
{
    float4x4 view;
    float4x4 proj;
};

struct Material
{
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

struct LightSource
{
    float3 position;
    float pad1;
    float3 color;
    float pad2;
};

struct Camera
{
    float3 pos;
    float pad;
};

cbuffer ubo : register(b1, space1)
{
    MVP mvp;
    LightSource ls;
    Camera cam;
};

cbuffer mat : register(b3, space0)
{
    Material material;
};

float4 main(PSInput input) : SV_TARGET {
    float3 textureColor = logo.Sample(samplerState, input.tex_coord);

    // calculate ambient ligth
    float ambient_str = 0.1f;
    float3 ambient = ambient_str * ls.color;

    // calculate diffuse light
    float3 norm = normalize(input.normal);
    float3 light_dir = normalize(ls.position - input.frag_pos);
    float diff = max(dot(norm, light_dir), 0.0f);
    float3 diffuse = diff * ls.color;

    // calculate specular light
    float specular_str = 0.5;
    float3 view_dir = normalize(cam.pos - input.frag_pos);
    float3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
    float3 specular = specular_str * spec * ls.color;

    float3 mat = ambient + diffuse + specular;
    float4 res = float4(mat * textureColor, 1.0f);
    return res;
}