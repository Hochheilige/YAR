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
    float4x4 model[10];
};

struct Material
{
    float3 ambient;
    float3 diffuse;
    float3 specular;
    float shinines;
};

struct LightSource
{
    float3 position;
    float3 color;
};

struct Camera
{
    float3 pos;
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
    float3 textureColor = logo.Sample(samplerState, input.tex_coord);
    Material mater = material[index];

    // calculate ambient ligth
    float3 ambient = mater.ambient * ls.color;

    // calculate diffuse light
    float3 norm = normalize(input.normal);
    float3 light_dir = normalize(ls.position - input.frag_pos);
    float diff = max(dot(norm, light_dir), 0.0f);
    float3 diffuse = (diff * mater.diffuse) * ls.color;

    // calculate specular light
    float3 view_dir = normalize(cam.pos - input.frag_pos);
    float3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), mater.shinines);
    float3 specular = (mater.specular * spec) * ls.color;

    float3 mat = ambient + diffuse + specular;

    // 0th cube is LightSource, so it renders with light source color
    float4 res = lerp(float4(ls.color, 1.0f), float4(mat, 1.0f), index > 0);
    return res;
}