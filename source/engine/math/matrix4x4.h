#pragma once
#include <DirectXMath.h>
#include <cmath>

#include "Vector3.h"
#include "Vector4.h"
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

    template<typename T>
    explicit Matrix4x4(const T* ptr)
    {
        data._11 = static_cast<float>(ptr[0]);  data._12 = static_cast<float>(ptr[1]);
        data._13 = static_cast<float>(ptr[2]);  data._14 = static_cast<float>(ptr[3]);

        data._21 = static_cast<float>(ptr[4]);  data._22 = static_cast<float>(ptr[5]);
        data._23 = static_cast<float>(ptr[6]);  data._24 = static_cast<float>(ptr[7]);

        data._31 = static_cast<float>(ptr[8]);  data._32 = static_cast<float>(ptr[9]);
        data._33 = static_cast<float>(ptr[10]); data._34 = static_cast<float>(ptr[11]);

        data._41 = static_cast<float>(ptr[12]); data._42 = static_cast<float>(ptr[13]);
        data._43 = static_cast<float>(ptr[14]); data._44 = static_cast<float>(ptr[15]);
    }

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

    static Matrix4x4 ortho_off_center(float left, float right, float bottom, float top, float near_plane, float far_plane)
    {
        return Matrix4x4(XMMatrixOrthographicOffCenterLH(left, right, bottom, top, near_plane, far_plane));
    }

    static Matrix4x4 rotation_axis(const Vector3& axis, float angle_radians)
    {
        XMVECTOR axis_vec = axis.load();
        return Matrix4x4(XMMatrixRotationAxis(axis_vec, angle_radians));
    }

    static Matrix4x4 look_at_rh(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        return Matrix4x4(XMMatrixLookAtRH(eye.load(), target.load(), up.load()));
    }

    // OpenGL-compatible perspective: RH coordinate system, [-1,1] z range
    static Matrix4x4 perspective_fov_rh_gl(float fov_radians, float aspect_ratio, float near_plane, float far_plane)
    {
        float f = 1.0f / tanf(fov_radians * 0.5f);
        float nf = near_plane - far_plane;

        XMFLOAT4X4 m = {};
        m._11 = f / aspect_ratio;
        m._22 = f;
        m._33 = (far_plane + near_plane) / nf;
        m._34 = -1.0f;
        m._43 = (2.0f * near_plane * far_plane) / nf;
        return Matrix4x4(m);
    }

    // OpenGL-compatible ortho: RH coordinate system, [-1,1] z range
    static Matrix4x4 ortho_off_center_rh_gl(float left, float right, float bottom, float top, float near_plane, float far_plane)
    {
        XMFLOAT4X4 m = {};
        m._11 = 2.0f / (right - left);
        m._22 = 2.0f / (top - bottom);
        m._33 = -2.0f / (far_plane - near_plane);
        m._41 = -(right + left) / (right - left);
        m._42 = -(top + bottom) / (top - bottom);
        m._43 = -(far_plane + near_plane) / (far_plane - near_plane);
        m._44 = 1.0f;
        return Matrix4x4(m);
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

    Vector4 operator*(const Vector4& vector) const
    {
        XMMATRIX m = load();
        XMVECTOR vec = vector.load();

        return Vector4(XMVector4Transform(vec, m));
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