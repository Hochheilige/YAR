RWTexture2D<float4> quad_tex : register(u0, space0);

cbuffer ubo : register(b0, space1)
{
    float4x4 invViewProj;
    float4 camera_pos;   
};

#define INF 1.0f / 0.0f
#define PI 3.14159265358979323846f

float degrees_to_radians(float degrees)
{
    return degrees * PI / 180;
}

struct Interval
{
    float min_;
    float max_;
};

float interval_size(const Interval i)
{
    return i.max_ - i.min_;
}

bool interval_contains(const Interval i, const float x)
{
    return i.min_ <= x && x <= i.max_;
}

bool interval_surrounds(const Interval i, const float x)
{
    return i.min_ < x && x < i.max_;
}

struct Ray
{
    float3 origin;
    float3 direction;
};

struct Sphere
{
    float3 center;
    float radius;
};

struct HitRecord
{
    float3 p;
    float3 normal;
    float t;
    bool front_face;
};

void set_face_normal(const Ray r, float3 outward_normal, out HitRecord rec)
{
    rec.front_face = dot(r.direction, outward_normal) < 0;
    rec.normal = rec.front_face ? outward_normal : -outward_normal;
}

float3 ray_at(const Ray r, const float t)
{
    return r.origin + t * r.direction;
}

bool hit_sphere(const Ray r, const Sphere s, const Interval ray_t, out HitRecord rec)
{
    float3 oc = s.center - r.origin;
    float a = dot(r.direction, r.direction);
    float h = dot(r.direction, oc);
    float c = dot(oc, oc) - s.radius * s.radius;

    float discriminant = h * h - a * c;
    if (discriminant < 0)
        return false;

    float sqrtd = sqrt(discriminant);
    float root = (h - sqrtd) / a;
    if (!interval_surrounds(ray_t, root))
    {
        root = (h + sqrtd) / a;
        if (!interval_surrounds(ray_t, root))
            return false;
    } 

    rec.t = root;
    rec.p = ray_at(r, rec.t);
    float3 outward_normal = (rec.p - s.center) / s.radius;
    set_face_normal(r, outward_normal, rec);

    return true;
}

bool hit(const Ray r, Interval ray_t, Sphere spheres[2], out HitRecord rec)
{
    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_to_far = ray_t.max_;

    for (uint i = 0; i < 2; ++i)
    {
        Interval interval;
        interval.min_ = ray_t.min_;
        interval.max_ = closest_to_far;
        if (hit_sphere(r, spheres[i], interval, temp_rec))
        {
            hit_anything = true;
            closest_to_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hit_anything;
}

float3 ray_color(const Ray r, Sphere spheres[2])
{
    Interval interval;
    interval.min_ = 0.0f;
    interval.max_ = INF;
    HitRecord rec;
    if (hit(r, interval, spheres, rec))
    {
        return 0.5f * (rec.normal + 1);
    }

    float3 unit_direction = normalize(r.direction);
    float a = 0.5*(unit_direction.y + 1.0);
    return lerp(a, float3(1.0f, 1.0f, 1.0f), float3(0.5f, 0.7f, 1.0f));
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID)
{

    Interval empty;
    empty.min_ = INF;
    empty.max_ = -INF;
    Interval universe;
    universe.min_ = -INF;
    universe.max_ = INF;

    uint width;
    uint height;
    quad_tex.GetDimensions(width, height);

    float2 ndc = (float2(dispatchThreadID.xy) / float2(width, height)) * 2.0f - 1.0f; 
    float4 clipSpace = float4(ndc, 0.0f, 1.0f);

    float4 worldSpace = mul(invViewProj, clipSpace);
    worldSpace /= worldSpace.w;
    float3 rayDir = normalize(worldSpace.xyz - camera_pos.xyz);
    Ray r;
    r.direction = rayDir;
    r.origin = camera_pos.xyz;

    // temp add 2 spheres here
    // better to add it through ubo
    Sphere spheres[2];
    spheres[0].center = float3(0.0f, 0.0f, -1.0f);
    spheres[0].radius = 0.5f;
    spheres[1].center = float3(0.0f, -100.5f, -1.0f);
    spheres[1].radius = 100;

    float3 rayColor = ray_color(r, spheres);
    quad_tex[dispatchThreadID.xy] = float4(rayColor, 1.0f);
}
