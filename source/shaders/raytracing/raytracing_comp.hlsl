RWTexture2D<float4> quad_tex : register(u0, space0);

cbuffer ubo : register(b0, space1)
{
    float4x4 invViewProj;
    float4 camera_pos;   
};

struct ray
{
    float3 origin;
    float3 direction;
};

float3 ray_at(const ray r, const float t)
{
    return r.origin + t * r.direction;
}

float hit_sphere(const float3 center, float radius, const ray r) {
    float3 oc = center - r.origin;
    float a = dot(r.direction, r.direction);
    float b = -2.0 * dot(r.direction, oc);
    float c = dot(oc, oc) - radius*radius;
    float discriminant = b*b - 4*a*c;

    if (discriminant < 0)
        return -1.0f;
    else
        return (-b - sqrt(discriminant)) / (2.0f * a);
}

float3 ray_color(const ray r)
{
    float3 sphere_center = float3(0.0f, 0.0f, -1.0f);
    float t = hit_sphere(sphere_center, 0.5f, r); 
    if (t > 0.0f)
    {
        float3 normal = normalize(ray_at(r, t) - sphere_center);
        return 0.5f * (normal + 1);
    }

    float3 unit_direction = normalize(r.direction);
    float a = 0.5*(unit_direction.y + 1.0);
    return lerp(a, float3(1.0f, 1.0f, 1.0f), float3(0.5f, 0.7f, 1.0f));
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID)
{
    uint width;
    uint height;
    quad_tex.GetDimensions(width, height);

    float2 ndc = (float2(dispatchThreadID.xy) / float2(width, height)) * 2.0f - 1.0f; 
    float4 clipSpace = float4(ndc, 0.0f, 1.0f);

    float4 worldSpace = mul(invViewProj, clipSpace);
    worldSpace /= worldSpace.w;
    float3 rayDir = normalize(worldSpace.xyz - camera_pos.xyz);
    ray r;
    r.direction = rayDir;
    r.origin = camera_pos.xyz;

    float3 rayColor = ray_color(r);
    quad_tex[dispatchThreadID.xy] = float4(rayColor, 1.0f);
}
