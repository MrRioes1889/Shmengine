#pragma once

#include "Defines.hpp"
#include "../MathTypes.hpp"

namespace Math
{

	//------- Vec4f functions

	SHMINLINE Vec4f operator+(Vec4f a, Vec4f b)
	{
		return { a.x + b.x , a.y + b.y, a.z + b.z, a.w + b.w };
	}

	SHMINLINE Vec4f operator-(Vec4f a, Vec4f b)
	{
		return { a.x - b.x , a.y - b.y, a.z - b.z, a.w - b.w };
	}

	SHMINLINE Vec4f operator*(Vec4f a, float scalar)
	{
		Vec4f result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		result.z = a.z * scalar;
		result.w = a.w * scalar;
		return result;
	}

	SHMINLINE Vec4f operator*(float scalar, Vec4f a)
	{
		Vec4f result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		result.z = a.z * scalar;
		result.w = a.w * scalar;
		return result;
	}

	SHMINLINE Vec4f operator/(Vec4f a, const float& divisor)
	{
		Vec4f result;
		result.x = a.x / divisor;
		result.y = a.y / divisor;
		result.z = a.z / divisor;
		result.w = a.w / divisor;
		return result;
	}

	SHMINLINE void operator*=(Vec4f& a, const float& scalar)
	{
		a.x *= scalar;
		a.y *= scalar;
		a.z *= scalar;
		a.w *= scalar;
	}

	SHMINLINE void operator/=(Vec4f& a, const float& divisor)
	{
		a.x /= divisor;
		a.y /= divisor;
		a.z /= divisor;
		a.w /= divisor;
	}

	SHMINLINE void operator+=(Vec4f& a, const Vec4f& other)
	{
		a.x += other.x;
		a.y += other.y;
		a.z += other.z;
		a.w += other.w;
	}

	SHMINLINE void operator-=(Vec4f& a, const Vec4f& other)
	{
		a.x -= other.x;
		a.y -= other.y;
		a.z -= other.z;
		a.w -= other.w;
	}

	SHMINLINE bool8 operator==(const Vec4f& a, const Vec4f& other)
	{
		return (a.x == other.x && a.y == other.y && a.z == other.z && a.w == other.w);
	}

	SHMINLINE float inner_product(Vec4f a, Vec4f b)
	{
		float res = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		return res;
	}

	SHMINLINE float inner_product_f32(
		float32 a0, float32 a1, float32 a2, float32 a3,
		float32 b0, float32 b1, float32 b2, float32 b3)
	{
		float res = a0 * b0 + a1 * b1 + a2 * b2 + a3 * b3;
		return res;
	}

	SHMINLINE float length_squared(Vec4f a)
	{
		float res = inner_product(a, a);
		return res;
	}

	SHMINLINE float length(Vec4f a)
	{
		float res = sqrt(inner_product(a, a));
		return res;
	}

	SHMINLINE void normalize(Vec4f& a)
	{
		float32 l = length(a);
		a.x /= l;
		a.y /= l;
		a.z /= l;
		a.w /= l;
	}

	SHMINLINE Vec4f normalized(Vec4f a)
	{
		normalize(a);
		return a;
	}

	SHMINLINE bool32 vec_compare(Vec4f v1, Vec4f v2, float32 tolerance = FLOAT_EPSILON)
	{
		return (
			abs(v1.x - v2.x) <= tolerance && 
			abs(v1.y - v2.y) <= tolerance && 
			abs(v1.z - v2.z) <= tolerance && 
			abs(v1.w - v2.w) <= tolerance);
	}

	SHMINLINE float32 vec_distance(Vec4f v1, Vec4f v2)
	{
		Vec4f dist = { v2.x - v1.x, v2.y - v1.y, v2.z - v1.z, v2.w - v1.w };
		return length(dist);
	}

	SHMINLINE Vec4f vec_mul(Vec4f v1, Vec4f v2)
	{
		Vec4f r = { v2.x * v1.x, v2.y * v1.y, v2.z * v1.z, v2.w * v1.w };
		return r;
	}

	SHMINLINE Vec3f to_vec3(Vec4f v)
	{
		return { v.x, v.y, v.z };
	}

	SHMINLINE Vec4f to_vec4(Vec3f v, float32 w)
	{
		return { v.x, v.y, v.z, w };
	}

	
	
}