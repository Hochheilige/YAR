#pragma once
#include <DirectXMath.h>
#include "Vector3.h"

using namespace DirectX;

struct Quaternion {
    XMFLOAT4 data;

    Quaternion() : data(0.0f, 0.0f, 0.0f, 1.0f) {} 
    Quaternion(float x, float y, float z, float w) : data(x, y, z, w) {}
    explicit Quaternion(const XMFLOAT4& xm) : data(xm) {}
    explicit Quaternion(FXMVECTOR v) { XMStoreFloat4(&data, v); }

    static Quaternion from_axis_angle(const Vector3& axis, float angle_radians) {
        XMVECTOR axis_vec = axis.load();
        return Quaternion(XMQuaternionRotationAxis(axis_vec, angle_radians));
    }

    static Quaternion from_euler(float pitch, float yaw, float roll) {
        return Quaternion(XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
    }

    static const Quaternion identity;

    XMVECTOR load() const {
        return XMLoadFloat4(&data);
    }

    void store(FXMVECTOR v) {
        XMStoreFloat4(&data, v);
    }

    Quaternion operator*(const Quaternion& other) const {
        XMVECTOR q1 = load();
        XMVECTOR q2 = other.load();
        return Quaternion(XMQuaternionMultiply(q1, q2));
    }

    Quaternion& operator*=(const Quaternion& other) {
        store(XMQuaternionMultiply(load(), other.load()));
        return *this;
    }

    Vector3 rotate_vector(const Vector3& v) const {
        XMVECTOR quat = load();
        XMVECTOR vec = v.load();
        return Vector3(XMVector3Rotate(vec, quat));
    }

    Quaternion conjugate() const {
        return Quaternion(XMQuaternionConjugate(load()));
    }

    Quaternion inverse() const {
        return Quaternion(XMQuaternionInverse(load()));
    }

    void normalize() {
        store(XMQuaternionNormalize(load()));
    }

    Quaternion normalized() const {
        return Quaternion(XMQuaternionNormalize(load()));
    }

    Vector3 get_forward_vector() const {
        return rotate_vector(Vector3::forward);
    }

    Vector3 get_up_vector() const {
        return rotate_vector(Vector3::up);
    }

    Vector3 get_right_vector() const {
        return rotate_vector(Vector3::right);
    }
};

inline Quaternion slerp(const Quaternion& a, const Quaternion& b, float t) {
    XMVECTOR q1 = a.load();
    XMVECTOR q2 = b.load();
    return Quaternion(XMQuaternionSlerp(q1, q2, t));
}