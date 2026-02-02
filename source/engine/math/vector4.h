#pragma once
#include <DirectXMath.h>
#include <string>

#include "Vector3.h"

using namespace DirectX;

struct Vector4 {
    XMFLOAT4 data;

    Vector4() : data(0.0f, 0.0f, 0.0f, 0.0f) {}
    Vector4(float x, float y, float z, float w) : data(x, y, z, w) {}
    Vector4(const Vector3& v, float w) : data(v.x(), v.y(), v.z(), w) {}
    explicit Vector4(float scalar) : data(scalar, scalar, scalar, scalar) {}
    explicit Vector4(const XMFLOAT4& xm) : data(xm) {}
    explicit Vector4(FXMVECTOR v) { XMStoreFloat4(&data, v); }

    float& x() { return data.x; }
    float& y() { return data.y; }
    float& z() { return data.z; }
    float& w() { return data.w; }

    float x() const { return data.x; }
    float y() const { return data.y; }
    float z() const { return data.z; }
    float w() const { return data.w; }

    // Extract xyz as Vector3
    Vector3 xyz() const { return Vector3(data.x, data.y, data.z); }

    float& operator[](size_t index) {
        return (&data.x)[index];
    }

    float operator[](size_t index) const {
        return (&data.x)[index];
    }

    XMVECTOR load() const {
        return XMLoadFloat4(&data);
    }

    void store(FXMVECTOR v) {
        XMStoreFloat4(&data, v);
    }

    Vector4 operator+(const Vector4& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector4(XMVectorAdd(v1, v2));
    }

    Vector4 operator-(const Vector4& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector4(XMVectorSubtract(v1, v2));
    }

    Vector4 operator*(float scalar) const {
        XMVECTOR v = load();
        return Vector4(XMVectorScale(v, scalar));
    }

    Vector4 operator/(float scalar) const {
        XMVECTOR v = load();
        return Vector4(XMVectorScale(v, 1.0f / scalar));
    }

    Vector4 operator*(const Vector4& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector4(XMVectorMultiply(v1, v2));
    }

    Vector4 operator-() const {
        XMVECTOR v = load();
        return Vector4(XMVectorNegate(v));
    }

    Vector4& operator+=(const Vector4& other) {
        store(XMVectorAdd(load(), other.load()));
        return *this;
    }

    Vector4& operator-=(const Vector4& other) {
        store(XMVectorSubtract(load(), other.load()));
        return *this;
    }

    Vector4& operator*=(float scalar) {
        store(XMVectorScale(load(), scalar));
        return *this;
    }

    Vector4& operator/=(float scalar) {
        store(XMVectorScale(load(), 1.0f / scalar));
        return *this;
    }

    float length() const {
        XMVECTOR v = load();
        return XMVectorGetX(XMVector4Length(v));
    }

    float length_squared() const {
        XMVECTOR v = load();
        return XMVectorGetX(XMVector4LengthSq(v));
    }

    Vector4 normalized() const {
        XMVECTOR v = load();
        return Vector4(XMVector4Normalize(v));
    }

    void normalize() {
        store(XMVector4Normalize(load()));
    }

    float dot(const Vector4& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return XMVectorGetX(XMVector4Dot(v1, v2));
    }

    std::string to_string() const {
        return "(" + std::to_string(data.x) + ", " +
            std::to_string(data.y) + ", " +
            std::to_string(data.z) + ", " +
            std::to_string(data.w) + ")";
    }

    static const Vector4 zero;
    static const Vector4 one;
};

inline Vector4 operator*(float scalar, const Vector4& v) {
    return v * scalar;
}

inline Vector4 lerp(const Vector4& a, const Vector4& b, float t) {
    XMVECTOR v1 = a.load();
    XMVECTOR v2 = b.load();
    return Vector4(XMVectorLerp(v1, v2, t));
}
