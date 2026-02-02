// math/vector3.h

#pragma once
#include <DirectXMath.h>
#include <cmath>
#include <string>

using namespace DirectX;

struct Vector3 {
    XMFLOAT3 data;

    Vector3() : data(0.0f, 0.0f, 0.0f) {}
    Vector3(float x, float y, float z) : data(x, y, z) {}
    explicit Vector3(float scalar) : data(scalar, scalar, scalar) {}
    explicit Vector3(const XMFLOAT3& xm) : data(xm) {}
    explicit Vector3(FXMVECTOR v) { XMStoreFloat3(&data, v); }

    float& x() { return data.x; }
    float& y() { return data.y; }
    float& z() { return data.z; }

    float x() const { return data.x; }
    float y() const { return data.y; }
    float z() const { return data.z; }

    float& operator[](size_t index) {
        return (&data.x)[index];  // data.x, data.y, data.z are sequential
    }

    float operator[](size_t index) const {
        return (&data.x)[index];
    }

    XMVECTOR load() const {
        return XMLoadFloat3(&data);
    }

    void store(FXMVECTOR v) {
        XMStoreFloat3(&data, v);
    }

    Vector3 operator+(const Vector3& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector3(XMVectorAdd(v1, v2));
    }

    Vector3 operator-(const Vector3& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector3(XMVectorSubtract(v1, v2));
    }

    Vector3 operator*(float scalar) const {
        XMVECTOR v = load();
        return Vector3(XMVectorScale(v, scalar));
    }

    Vector3 operator/(float scalar) const {
        XMVECTOR v = load();
        return Vector3(XMVectorScale(v, 1.0f / scalar));
    }

    Vector3 operator*(const Vector3& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector3(XMVectorMultiply(v1, v2));
    }

    Vector3 operator-() const {
        XMVECTOR v = load();
        return Vector3(XMVectorNegate(v));
    }

    Vector3& operator+=(const Vector3& other) {
        store(XMVectorAdd(load(), other.load()));
        return *this;
    }

    Vector3& operator-=(const Vector3& other) {
        store(XMVectorSubtract(load(), other.load()));
        return *this;
    }

    Vector3& operator*=(float scalar) {
        store(XMVectorScale(load(), scalar));
        return *this;
    }

    Vector3& operator/=(float scalar) {
        store(XMVectorScale(load(), 1.0f / scalar));
        return *this;
    }

    float length() const {
        XMVECTOR v = load();
        return XMVectorGetX(XMVector3Length(v));
    }

    float length_squared() const {
        XMVECTOR v = load();
        return XMVectorGetX(XMVector3LengthSq(v));
    }

    Vector3 normalized() const {
        XMVECTOR v = load();
        return Vector3(XMVector3Normalize(v));
    }

    void normalize() {
        store(XMVector3Normalize(load()));
    }

    float dot(const Vector3& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return XMVectorGetX(XMVector3Dot(v1, v2));
    }

    Vector3 cross(const Vector3& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector3(XMVector3Cross(v1, v2));
    }

    float distance(const Vector3& other) const {
        return (*this - other).length();
    }

    bool is_nearly_zero(float tolerance = 1e-4f) const {
        return length_squared() < tolerance * tolerance;
    }

    std::string to_string() const {
        return "(" + std::to_string(data.x) + ", " +
            std::to_string(data.y) + ", " +
            std::to_string(data.z) + ")";
    }

    static const Vector3 zero;
    static const Vector3 one;
    static const Vector3 up;
    static const Vector3 down;
    static const Vector3 left;
    static const Vector3 right;
    static const Vector3 forward;
    static const Vector3 back;
};

inline Vector3 operator*(float scalar, const Vector3& v) {
    return v * scalar;
}

inline Vector3 lerp(const Vector3& a, const Vector3& b, float t) {
    XMVECTOR v1 = a.load();
    XMVECTOR v2 = b.load();
    return Vector3(XMVectorLerp(v1, v2, t));
}

inline Vector3 reflect(const Vector3& incident, const Vector3& normal) {
    XMVECTOR v_incident = incident.load();
    XMVECTOR v_normal = normal.load();
    return Vector3(XMVector3Reflect(v_incident, v_normal));
}

inline Vector3 min(const Vector3& a, const Vector3& b) {
    XMVECTOR v1 = a.load();
    XMVECTOR v2 = b.load();
    return Vector3(XMVectorMin(v1, v2));
}

inline Vector3 max(const Vector3& a, const Vector3& b) {
    XMVECTOR v1 = a.load();
    XMVECTOR v2 = b.load();
    return Vector3(XMVectorMax(v1, v2));
}