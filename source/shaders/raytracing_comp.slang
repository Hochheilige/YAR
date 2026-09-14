#include "rt_common.h"

#define INF (1.0f / 0.0f)
#define PI 3.14159265358979323846f

static const uint MAT_LAMBERTIAN = 0;
static const uint MAT_METAL      = 1;
static const uint MAT_DIELECTRIC = 2;

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
    // Guard the sqrt: Random01inclusive can return exactly 1.0, and rounding
    // can push 1 - y*y a hair below zero, which yields NaN and a black pixel.
    float r = sqrt(max(0.0f, 1.0f - y * y));
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

func near_zero(float3 v) -> bool
{
    float s = 1e-8;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

float reflectance(float cosine, float refraction_index)
{
    // Use Schlick's approximation for reflectance
    float r0 = (1 - refraction_index) / (1 + refraction_index);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5.0f);
}

struct Interval
{
    float min_;
    float max_;

    __init(float min, float max)
    {
        min_ = min;
        max_ = max;
    }

    func size()->float
    {
        return max_ - min_;
    }

    func contains(const float x)->bool
    {
        return min_ <= x && x <= max_;
    }

    func surrounds(const float x)->bool
    {
        return min_ < x && x < max_;
    }
};

extension MaterialData
{
    func scatter(inout uint state, inout HitRecord rec, in Ray ray,
                 out vector<float, 3> attenuation, out Ray scattered) -> bool
    {
        switch (type)
        {
        case MaterialType.Lambertian:
        {
            Lambertian mat = { albedo };
            return mat.scatter(state, rec, ray, attenuation, scattered);
        }
        case MaterialType.Metal:
        {
            Metal mat = { albedo, fuzz };
            return mat.scatter(state, rec, ray, attenuation, scattered);
        }
        case MaterialType.Dielectric:
        {
            Dielectric mat = { refraction_index };
            return mat.scatter(state, rec, ray, attenuation, scattered);
        }
        default:
        {
            // Error MaterialType shows with magenta color
            attenuation = vector<float, 3>(1.0f, 0.0f, 1.0f);
            scattered.origin = rec.position;
            scattered.direction = vector<float, 3>(0.0f, 1.0f, 0.0f);
            return true;
        }
        }
    }
};

struct HitRecord
{
    vector<float, 3> position;
    vector<float, 3> normal;

    MaterialData mat;

    float t;
    bool front_face;

    // Slang treats 'this' as immutable inside a method unless the method is
    // marked [mutating], so writing to front_face/normal without it is an error.
    [mutating]
    func set_face_normal(const Ray r, vector<float, 3> outward_normal)
    {
        front_face = dot(r.direction, outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
}

struct Ray
{
    vector<float, 3> origin;
    vector<float, 3> direction;

    __init(vector<float, 3> o, vector<float, 3> dir)
    {
        origin = o;
        direction = dir;
    }

    func at(const float t)->vector<float, 3>
    {
        return origin + t * direction;
    }
};

interface Hittable
{
    bool hit(const Ray r, Interval ray_t, out HitRecord rec);
}

extension Sphere : Hittable
{
    override bool hit(const Ray r, Interval ray_t, out HitRecord rec)
    {
        // 'out' parameters start undefined; zero it so the early returns below
        // never leave the caller reading garbage.
        rec = (HitRecord)0;

        vector<float, 3> oc = center - r.origin; // TODO: need better naming
        float a = dot(r.direction, r.direction);
        float h = dot(r.direction, oc);
        float c = dot(oc, oc) - radius * radius;

        float discriminant = h * h -  a * c;
        if (discriminant < 0)
            return false;

        float sqrtd = sqrt(discriminant);

        // find the nearest root that lies in the acceptable range
        float root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root))
        {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root))
                return false;
        }

        rec.t = root;
        rec.position = r.at(rec.t);

        vector<float, 3> outward_normal = (rec.position - center) / radius;
        rec.set_face_normal(r, outward_normal);

        return true;
    }
};

extension BVHNode
{
    bool hit_aabb(const Ray r, Interval ray_t)
    {
        float3 inv_dir = 1.0f / r.direction;
        float3 t1 = (aabb_min - r.origin) * inv_dir;
        float3 t2 = (aabb_max - r.origin) * inv_dir;
        float3 t_near = min(t1, t2);
        float3 t_far = max(t1, t2);

        float tmin = max(ray_t.min_, max(t_near.x, max(t_near.y, t_near.z)));
        float tmax = min(ray_t.max_, min(t_far.x, min(t_far.y, t_far.z)));
        return tmin <= tmax;
    }   
};

static const uint BVH_STACK_SIZE = 32;

struct World : Hittable
{
    override bool hit(const Ray r, Interval ray_t, out HitRecord rec)
    {
        HitRecord temp_rec = {};
        bool hit_anything = false;
        float closest_to_far = ray_t.max_;

        uint stack[BVH_STACK_SIZE];
        uint stack_ptr = 0;
        stack[stack_ptr++] = 0; // root

        while (stack_ptr > 0)
        {
            BVHNode node = bvh[stack[--stack_ptr]];
            if (!node.hit_aabb(r, Interval(ray_t.min_, closest_to_far)))
                continue;

            if (node.prim_count > 0) // leaf
            {
                for (uint j = 0; j < node.prim_count; ++j)
                {
                    uint sphere_index = spheres_indexes[node.left_first + j];
                    HitRecord temp_rec;
                    if (spheres[sphere_index].hit(r, Interval(ray_t.min_, closest_to_far), temp_rec))
                    {
                        hit_anything = true;
                        closest_to_far = temp_rec.t;
                        rec = temp_rec;
                        rec.mat = mats[sphere_index];
                    }
                }
            }
            else
            {
                stack[stack_ptr++] = node.left_first;
                stack[stack_ptr++] = node.left_first + 1;
            }
        }

        return hit_anything;
    }
}

interface IMaterial
{
    bool scatter(inout uint state, inout HitRecord rec, in Ray ray, out vector<float, 3> attenuation, out Ray scattered);
}

struct Lambertian : IMaterial
{
    vector<float, 3> albedo;

    override bool scatter(inout uint state, inout HitRecord rec, in Ray ray, out vector<float, 3> attenuation, out Ray scattered)
    {
        scattered.origin = rec.position;
        attenuation = albedo;

        var scatter_direction = rec.normal + random_vector_on_sphere(state);
        if (near_zero(scatter_direction))
            scatter_direction = rec.normal;

        scattered.direction = scatter_direction;
        return true;
    }
}

struct Metal : IMaterial
{
    vector<float, 3> albedo;
    float fuzz;

    override bool scatter(inout uint state, inout HitRecord rec, in Ray ray, out vector<float, 3> attenuation, out Ray scattered)
    {
        scattered.origin = rec.position;
        attenuation = albedo;

        vector<float, 3> reflected = reflect(ray.direction, rec.normal);
        reflected = normalize(reflected) + (fuzz * random_vector_on_sphere(state));
        scattered.direction = reflected;
        return dot(scattered.direction, rec.normal) > 0;
    }
}

struct Dielectric : IMaterial
{
    float refraction_index;

    override bool scatter(inout uint state, inout HitRecord rec, in Ray ray, out vector<float, 3> attenuation, out Ray scattered)
    {
        scattered.origin = rec.position;
        attenuation = vector<float, 3>(1.0f, 1.0f, 1.0f);

        float ri = rec.front_face ? (1.0f / refraction_index) : refraction_index;

        var unit_dir = normalize(ray.direction);

        float cos_theta = min(dot(-unit_dir, rec.normal), 1.0f);
        float sin_theta = sqrt(max(0.0f, 1 - cos_theta * cos_theta));
        bool cannot_refract = ri * sin_theta > 1.0f;

        vector<float, 3> direction;
        if (cannot_refract || reflectance(cos_theta, ri) > Random01inclusive(state))
            direction = reflect(unit_dir, rec.normal);
        else
            direction = refract(unit_dir, rec.normal, ri);

        scattered.direction = direction;
        return true;
    }
}

func trace(World world, Ray r, inout uint state, int max_depth)->vector<float, 3>
{
    Interval interval = { 0.001f, INF };
    
    var color = float3(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < max_depth; ++i)
    {
        HitRecord rec = {};

        if (!world.hit(r, interval, rec))
        {
            var unit_direction = normalize(r.direction);
            var a = 0.5f * (unit_direction.y + 1.0f);
            return lerp(float3(1.0f, 1.0f, 1.0f), float3(0.5f, 0.7f, 1.0f), a) * color;
        }

        Ray scattered;
        float3 attenuation = 0.0f;
        if (!rec.mat.scatter(state, rec, r, attenuation, scattered))
            return { 0.0f, 0.0f, 0.0f };

        color *= attenuation;
        r = scattered;
    }

    // never reached the light within the bounce budget
    return { 0.0f, 0.0f, 0.0f };
}

float2 sample_square(inout uint state)
{
    return 2.0f * float2(Random01(state), Random01(state)) - 1.0f;
}

// sqrt and max are component-wise, so this handles all three channels at once
// and the max() replaces the branch: negative input clamps to 0 instead of
// returning NaN from sqrt.
float3 linear_to_gamma(float3 linear_color)
{
    return sqrt(max(linear_color, 0.0f));
}

// register(u0, space0) also works but warns on every build; the vk attribute
// is the Vulkan-native spelling and is what the engine reflects on.
[[vk::binding(0, 0)]] RWTexture2D<vector<float, 4>> quad_tex;
[[vk::binding(1, 0)]] StructuredBuffer<Sphere> spheres;
[[vk::binding(2, 0)]] StructuredBuffer<BVHNode> bvh;
[[vk::binding(3, 0)]] StructuredBuffer<uint> spheres_indexes;
[[vk::binding(4, 0)]] StructuredBuffer<MaterialData> mats;
[[vk::binding(1, 1)]] ConstantBuffer<UBO> ubo;


[shader("compute")] [numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint width, height;
    quad_tex.GetDimensions(width, height);

    World world = {};

    // Clip space runs [-1, 1], so the [0, 1] pixel ratio has to be remapped.
    const vector<float, 2> ndc = (float2(dispatchThreadID.xy) / float2(width, height)) * 2.0f - 1.0f;
    vector<float, 3> final_color = { 0.0f, 0.0f, 0.0f };
    uint state = SeedThread(dispatchThreadID.x + dispatchThreadID.y * width + ubo.seed);
    for (int s = 0; s < ubo.samples_per_pixel; ++s)
    {
        var offset = sample_square(state);
        var jittered_ndc = ndc + offset / float2(width, height);

        vector<float, 4> clip_space = { jittered_ndc, 0.0f, 1.0f };
        var world_space = mul(ubo.inv_view_proj, clip_space);
        world_space /= world_space.w;

        var ray_dir = normalize(world_space.xyz - ubo.camera_pos.xyz);
        Ray r = { ubo.camera_pos.xyz, ray_dir };
        final_color += trace(world, r, state, ubo.max_ray_depth);
    }

    final_color /= ubo.samples_per_pixel;
    final_color = linear_to_gamma(final_color);

    quad_tex[dispatchThreadID.xy] = float4(final_color, 1.0f);
}
