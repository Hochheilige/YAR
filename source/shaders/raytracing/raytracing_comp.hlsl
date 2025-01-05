#define LAMBERTIAN 0
#define METAL      1
#define DIELECTRIC 2
#define SPHERES_COUNT 4

struct Material
{
    float3 albedo;
    float fuzz; // only for Metals
    float refraction_index; // only for Dielectrics
    uint type;
};

struct Sphere
{
    float3 center;
    float radius;
};

RWTexture2D<float4> quad_tex : register(u0, space0);

cbuffer ubo : register(b1, space1)
{
    float4x4 invViewProj;
    float4 camera_pos;
    Sphere spheres[SPHERES_COUNT];
    Material mats[SPHERES_COUNT];
    int samples_per_pixel;
    int max_ray_depth;
    float seed;   
};

#define INF 1.0f / 0.0f
#define PI 3.14159265358979323846f

// Create an initial random number for this thread
uint SeedThread(uint seed)
{
    // Thomas Wang hash 
    // Ref: http://www.burtleburtle.net/bob/hash/integer.html
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}
// Generate a random 32-bit integer
uint Random(inout uint state)
{
    // Xorshift algorithm from George Marsaglia's paper.
    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
    return state;
}
// Generate a random float in the range [0.0f, 1.0f)
float Random01(inout uint state)
{
    return asfloat(0x3f800000 | Random(state) >> 9) - 1.0;
}
// Generate a random float in the range [0.0f, 1.0f]
float Random01inclusive(inout uint state)
{
    return Random(state) / float(0xffffffff);
}
// Generate a random integer in the range [lower, upper]
uint Random(inout uint state, uint lower, uint upper)
{
    return lower + uint(float(upper - lower + 1) * Random01(state));
}

float3 random_vector_on_sphere(inout uint state)
{
    float y = 2.0f * Random01inclusive(state) - 1.0f;
    float r = sqrt(1.0f - y * y);
    float l = 2.0f * PI * Random01inclusive(state) - PI;
    return float3(r * sin(l), y, r * cos(l));
}

float3 random_vector_on_hemisphere(inout uint state, const float3 normal)
{
    float3 on_unit_sphere = random_vector_on_sphere(state);
    if (dot(on_unit_sphere, normal) > 0.0f)
        return on_unit_sphere;
    
    return -on_unit_sphere;
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

struct HitRecord
{
    float3 p;
    float3 normal;
    Material mat;
    float t;
    bool front_face;
};

bool near_zero(float3 v) {
    float s = 1e-8;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

float3 reflect(const float3 v, const float3 n)
{
    return v - 2.0f * dot(v, n) * n;
}

float3 refract(const float3 r, const float3 n, const float eta_over_etap)
{
    // Based on Snell's Law
    float cos_theta = min(dot(-r, n), 1.0f);
    float3 r_perp = eta_over_etap * (r + cos_theta * n);
    float r_perp_len = length(r_perp);
    float3 r_parallel = -sqrt(abs(1.0f - r_perp_len * r_perp_len)) * n;
    return r_perp + r_parallel;
}

bool scatter(inout uint state, Ray r_in, inout HitRecord rec, out float3 attenuation, out Ray scattered)
{
    scattered.origin = rec.p;
    attenuation = rec.mat.albedo;

    if (rec.mat.type == LAMBERTIAN)
    {
        float3 scatter_direction = rec.normal + random_vector_on_sphere(state);

        if (near_zero(scatter_direction))
            scatter_direction = rec.normal;

        scattered.direction = scatter_direction;
    }

    if (rec.mat.type == METAL)
    {
        float3 reflected = reflect(r_in.direction, rec.normal);
        reflected = normalize(reflected) + (rec.mat.fuzz * random_vector_on_sphere(state));
        scattered.direction = reflected;
        return dot(scattered.direction, rec.normal) > 0;    
    }

    if (rec.mat.type == DIELECTRIC)
    {
        attenuation = float3(1.0f, 1.0f, 1.0f);
        float ri = rec.front_face ? (1.0f / rec.mat.refraction_index) : rec.mat.refraction_index;

        float3 unit_dir = normalize(r_in.direction);

        float cos_theta = min(dot(-unit_dir, rec.normal), 1.0f);
        float sin_theta = sqrt(1 - cos_theta * cos_theta);
        bool cannot_refract = ri * sin_theta > 1.0f;

        float3 direction;
        if (cannot_refract)
            direction = reflect(unit_dir, rec.normal);
        else
            direction = refract(unit_dir, rec.normal, ri);

        scattered.direction = direction;
    }

    return true;
}

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

bool hit(const Ray r, Interval ray_t, out HitRecord rec)
{
    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_to_far = ray_t.max_;

    for (uint i = 0; i < SPHERES_COUNT; ++i)
    {
        Interval interval;
        interval.min_ = ray_t.min_;
        interval.max_ = closest_to_far;
        if (hit_sphere(r, spheres[i], interval, temp_rec))
        {
            hit_anything = true;
            closest_to_far = temp_rec.t;
            rec = temp_rec;
            rec.mat = mats[i];
        }
    }

    return hit_anything;
}

float3 ray_color(inout uint state, Ray r)
{
    Interval interval;
    interval.min_ = 0.001f;
    interval.max_ = INF;
    HitRecord rec;

    float3 color = float3(1.0f, 1.0f, 1.0f);
    for (uint i = 0; i < max_ray_depth; ++i)
    {
        if (hit(r, interval, rec))
        {
            Ray scattered;
            float3 attenuation;
            if (scatter(state, r, rec, attenuation, scattered))
            {
                color *= attenuation;
                r = scattered;
            }
            else 
            {
                return float3(0.0f, 0.0f, 0.0f);
            }
        }
        else
        {
            break;
        }
    }

    float3 unit_direction = normalize(r.direction);
    float a = 0.5*(unit_direction.y + 1.0);
    return lerp(a, float3(1.0f, 1.0f, 1.0f), float3(0.5f, 0.7f, 1.0f)) * color;
}

float2 sample_square(inout uint state)
{
    return 2.0f * float2(Random01(state), Random01(state)) - 1.0f; 
}

float linear_to_gamma(float linear_color)
{
    if (linear_color > 0)
        return sqrt(linear_color);

    return 0.0f;
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

    const float2 ndc = (float2(dispatchThreadID.xy) / float2(width, height)) * 2.0f - 1.0f; 
    float3 final_color = float3(0.0f, 0.0f, 0.0f);
    uint state = SeedThread(seed);
    for (uint s = 0; s < samples_per_pixel; ++s)    
    {
        float2 offset = sample_square(state);
        float2 jittered_ndc = ndc + offset / float2(width, height);

        float4 clipSpace = float4(jittered_ndc, 0.0f, 1.0f);
        float4 worldSpace = mul(invViewProj, clipSpace);
        worldSpace /= worldSpace.w;

        float3 rayDir = normalize(worldSpace.xyz - camera_pos.xyz);

        Ray r;
        r.direction = rayDir;
        r.origin = camera_pos.xyz;

        final_color += ray_color(state, r);
    }

    final_color /= samples_per_pixel;
    final_color.x = linear_to_gamma(final_color.x);
    final_color.y = linear_to_gamma(final_color.y);
    final_color.z = linear_to_gamma(final_color.z);

    quad_tex[dispatchThreadID.xy] = float4(final_color, 1.0f);
}
