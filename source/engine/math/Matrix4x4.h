#pragma once
#include <DirectXMath.h>

#include "Vector3.h"
#include "Quaternion.h"

using namespace DirectX;

struct Matrix4x4 {
    XMFLOAT4X4 data;

    Matrix4x4()
    {
        XMStoreFloat4x4(&data, XMMatrixIdentity());
    }

    explicit Matrix4x4(const XMFLOAT4X4& xm) : data(xm) {}
    explicit Matrix4x4(FXMMATRIX m) { XMStoreFloat4x4(&data, m); }

    XMMATRIX load() const
    {
        return XMLoadFloat4x4(&data);
    }

    void store(FXMMATRIX m) 
    {
        XMStoreFloat4x4(&data, m);
    }

    static Matrix4x4 identity()
    {
        return Matrix4x4(XMMatrixIdentity());
    }

    static Matrix4x4 translation(const Vector3& position)
    {
        XMVECTOR pos = position.load();
        return Matrix4x4(XMMatrixTranslationFromVector(pos));
    }

    static Matrix4x4 rotation_quaternion(const Quaternion& rotation)
    {
        XMVECTOR quat = rotation.load();
        return Matrix4x4(XMMatrixRotationQuaternion(quat));
    }

    static Matrix4x4 scaling(const Vector3& scale)
    {
        XMVECTOR scl = scale.load();
        return Matrix4x4(XMMatrixScalingFromVector(scl));
    }

    static Matrix4x4 look_at(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        XMVECTOR eye_vec = eye.load();
        XMVECTOR target_vec = target.load();
        XMVECTOR up_vec = up.load();
        return Matrix4x4(XMMatrixLookAtLH(eye_vec, target_vec, up_vec));
    }

    static Matrix4x4 perspective_lh(float fov_radians, float aspect_ratio, float near_plane, float far_plane) 
    {
        return Matrix4x4(XMMatrixPerspectiveFovLH(fov_radians, aspect_ratio, near_plane, far_plane));
    }
    
    static Matrix4x4 perspective_rh(float fov_radians, float aspect_ratio, float near_plane, float far_plane) 
    {
        return Matrix4x4(XMMatrixPerspectiveFovRH(fov_radians, aspect_ratio, near_plane, far_plane));
    }

    static Matrix4x4 orthographic(float width, float height, float near_plane, float far_plane)
    {
        return Matrix4x4(XMMatrixOrthographicLH(width, height, near_plane, far_plane));
    }

    Matrix4x4 operator*(const Matrix4x4& other) const
    {
        XMMATRIX m1 = load();
        XMMATRIX m2 = other.load();
        return Matrix4x4(m1 * m2);
    }

    Matrix4x4& operator*=(const Matrix4x4& other) 
    {
        store(load() * other.load());
        return *this;
    }

    Vector3 transform_position(const Vector3& position) const 
    {
        XMVECTOR pos = position.load();
        XMMATRIX mat = load();
        return Vector3(XMVector3TransformCoord(pos, mat));
    }

    Vector3 transform_direction(const Vector3& direction) const
    {
        XMVECTOR dir = direction.load();
        XMMATRIX mat = load();
        return Vector3(XMVector3TransformNormal(dir, mat));
    }

    Matrix4x4 transpose() const
    {
        return Matrix4x4(XMMatrixTranspose(load()));
    }

    Matrix4x4 inverse() const
    {
        XMVECTOR det;
        return Matrix4x4(XMMatrixInverse(&det, load()));
    }

    float determinant() const
    {
        return XMVectorGetX(XMMatrixDeterminant(load()));
    }
};