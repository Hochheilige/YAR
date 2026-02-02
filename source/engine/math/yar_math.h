// yar_math.h — single include for all engine math types

#pragma once

#include "Vector2.h"
#include "Vector4.h"
#include "Vector3.h"
#include "Quaternion.h"
#include "Matrix4x4.h"
#include "Transform.h"
#include "math_utils.h"

// Deferred implementations that require both Vector3 and Vector4 to be complete

inline Vector3::Vector3(const Vector4& vec)
    : data(vec.x(), vec.y(), vec.z()) {}

inline Vector4::Vector4(const Vector3& v, float w)
    : data(v.x(), v.y(), v.z(), w) {}

inline Vector3 Vector4::xyz() const {
    return Vector3(data.x, data.y, data.z);
}
