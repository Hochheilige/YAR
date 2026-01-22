#pragma once

#include <DirectXMath.h>

struct Vector3
{
	Vector3() = default;

	Vector3(const float x, const float y, const float z) : data(x, y, z) {}
	Vector3(const Vector3& v) : data(v.data) {}

	explicit Vector3(float scalar) : data(scalar, scalar, scalar) {}
	explicit Vector3(const float* array) : data(array[0], array[1], array[2]) {}
	explicit Vector3(const DirectX::XMFLOAT3& v) : data(v) {}
	explicit Vector3(DirectX::FXMVECTOR v) { store(v); }

	float& x() { return data.x; }
	float& y() { return data.y; }
	float& z() { return data.z; }

	float x() const { return data.x; }
	float y() const { return data.y; }
	float z() const { return data.z; }

	float& operator[](size_t index)
	{
		return (&data.x)[index];
	}

	float operator[](size_t index) const
	{
		return (&data.x)[index];
	}

	DirectX::XMVECTOR load() const 
	{ 
		return DirectX::XMLoadFloat3(&data);
	}

	void store(DirectX::FXMVECTOR v)
	{
		DirectX::XMStoreFloat3(&data, v);
	}


	DirectX::XMFLOAT3 data;
};