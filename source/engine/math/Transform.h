#pragma once
#include "Vector3.h"
#include "Quaternion.h"
#include "Matrix4x4.h"

struct Transform {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;

    Transform()
        : position(Vector3::zero)
        , rotation(Quaternion::identity)
        , scale(Vector3::one)
    {
    }

    Transform(const Vector3& pos, const Quaternion& rot, const Vector3& scl = Vector3::one)
        : position(pos), rotation(rot), scale(scl)
    {
    }

    Matrix4x4 to_matrix() const
    {
        XMVECTOR pos = position.load();
        XMVECTOR rot = rotation.load();
        XMVECTOR scl = scale.load();

        return Matrix4x4(XMMatrixAffineTransformation(scl, XMVectorZero(), rot, pos));
    }

    Transform operator*(const Transform& other) const
    {
        Transform result;

        // Rotation: parent_rot * child_rot
        result.rotation = rotation * other.rotation;

        // Scale: parent_scale * child_scale (component-wise)
        result.scale = scale * other.scale;

        // Position: parent_pos + parent_rot.rotate(parent_scale * child_pos)
        Vector3 scaled_pos = scale * other.position;
        Vector3 rotated_pos = rotation.rotate_vector(scaled_pos);
        result.position = position + rotated_pos;

        return result;
    }

    Vector3 transform_position(const Vector3& point) const
    {
        Vector3 scaled = scale * point;
        Vector3 rotated = rotation.rotate_vector(scaled);
        return position + rotated;
    }

    Vector3 transform_direction(const Vector3& direction) const
    {
        // Directions ignore translation and scale
        return rotation.rotate_vector(direction);
    }

    Transform inverse() const 
    {
        Transform result;

        // Inverse rotation
        result.rotation = rotation.inverse();

        // Inverse scale
        result.scale = Vector3(1.0f / scale.x(), 1.0f / scale.y(), 1.0f / scale.z());

        // Inverse position
        Vector3 scaled_pos = result.scale * position;
        result.position = -(result.rotation.rotate_vector(scaled_pos));

        return result;
    }

    static const Transform identity;
};

// Lerp between transforms
inline Transform lerp(const Transform& a, const Transform& b, float t)
{
    return Transform(
        lerp(a.position, b.position, t),
        slerp(a.rotation, b.rotation, t),
        lerp(a.scale, b.scale, t)
    );
}