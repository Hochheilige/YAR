#pragma once
#include <DirectXMath.h>
#include <string>

using namespace DirectX;

struct Vector2 {
    XMFLOAT2 data;

    Vector2() : data(0.0f, 0.0f) {}
    Vector2(float x, float y) : data(x, y) {}
    explicit Vector2(float scalar) : data(scalar, scalar) {}
    explicit Vector2(const XMFLOAT2& xm) : data(xm) {}
    explicit Vector2(FXMVECTOR v) { XMStoreFloat2(&data, v); }

    float& x() { return data.x; }
    float& y() { return data.y; }

    float x() const { return data.x; }
    float y() const { return data.y; }

    float& operator[](size_t index) {
        return (&data.x)[index];
    }

    float operator[](size_t index) const {
        return (&data.x)[index];
    }

    XMVECTOR load() const {
        return XMLoadFloat2(&data);
    }

    void store(FXMVECTOR v) {
        XMStoreFloat2(&data, v);
    }

    Vector2 operator+(const Vector2& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector2(XMVectorAdd(v1, v2));
    }

    Vector2 operator-(const Vector2& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector2(XMVectorSubtract(v1, v2));
    }

    Vector2 operator*(float scalar) const {
        XMVECTOR v = load();
        return Vector2(XMVectorScale(v, scalar));
    }

    Vector2 operator/(float scalar) const {
        XMVECTOR v = load();
        return Vector2(XMVectorScale(v, 1.0f / scalar));
    }

    Vector2 operator*(const Vector2& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return Vector2(XMVectorMultiply(v1, v2));
    }

    Vector2 operator-() const {
        XMVECTOR v = load();
        return Vector2(XMVectorNegate(v));
    }

    Vector2& operator+=(const Vector2& other) {
        store(XMVectorAdd(load(), other.load()));
        return *this;
    }

    Vector2& operator-=(const Vector2& other) {
        store(XMVectorSubtract(load(), other.load()));
        return *this;
    }

    Vector2& operator*=(float scalar) {
        store(XMVectorScale(load(), scalar));
        return *this;
    }

    Vector2& operator/=(float scalar) {
        store(XMVectorScale(load(), 1.0f / scalar));
        return *this;
    }

    float length() const {
        XMVECTOR v = load();
        return XMVectorGetX(XMVector2Length(v));
    }

    float length_squared() const {
        XMVECTOR v = load();
        return XMVectorGetX(XMVector2LengthSq(v));
    }

    Vector2 normalized() const {
        XMVECTOR v = load();
        return Vector2(XMVector2Normalize(v));
    }

    void normalize() {
        store(XMVector2Normalize(load()));
    }

    float dot(const Vector2& other) const {
        XMVECTOR v1 = load();
        XMVECTOR v2 = other.load();
        return XMVectorGetX(XMVector2Dot(v1, v2));
    }

    std::string to_string() const {
        return "(" + std::to_string(data.x) + ", " +
            std::to_string(data.y) + ")";
    }

    static const Vector2 zero;
    static const Vector2 one;
};

inline Vector2 operator*(float scalar, const Vector2& v) {
    return v * scalar;
}

inline Vector2 lerp(const Vector2& a, const Vector2& b, float t) {
    XMVECTOR v1 = a.load();
    XMVECTOR v2 = b.load();
    return Vector2(XMVectorLerp(v1, v2, t));
}
