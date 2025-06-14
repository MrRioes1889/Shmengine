#pragma once

#include "Defines.hpp"
#include "../MathTypes.hpp"
#include "Common.hpp"

namespace Math
{

	//------- Vec3f functions

	SHMINLINE Vec3f operator+(Vec3f a, Vec3f b)
	{
		return { a.x + b.x , a.y + b.y, a.z + b.z };
	}

	SHMINLINE Vec3f operator-(Vec3f a, Vec3f b)
	{
		return { a.x - b.x , a.y - b.y, a.z - b.z };
	}

	SHMINLINE Vec3f operator*(Vec3f a, float scalar)
	{
		Vec3f result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		result.z = a.z * scalar;
		return result;
	}

	SHMINLINE Vec3f operator*(float scalar, Vec3f a)
	{
		Vec3f result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		result.z = a.z * scalar;
		return result;
	}

	SHMINLINE Vec3f operator/(Vec3f a, const float& divisor)
	{
		Vec3f result;
		result.x = a.x / divisor;
		result.y = a.y / divisor;
		result.z = a.z / divisor;
		return result;
	}

	SHMINLINE void operator*=(Vec3f& a, const float& scalar)
	{
		a.x *= scalar;
		a.y *= scalar;
		a.z *= scalar;
	}

	SHMINLINE void operator/=(Vec3f& a, const float& divisor)
	{
		a.x /= divisor;
		a.y /= divisor;
		a.z /= divisor;
	}

	SHMINLINE void operator+=(Vec3f& a, const Vec3f& other)
	{
		a.x += other.x;
		a.y += other.y;
		a.z += other.z;
	}

	SHMINLINE void operator-=(Vec3f& a, const Vec3f& other)
	{
		a.x -= other.x;
		a.y -= other.y;
		a.z -= other.z;
	}

	SHMINLINE bool8 operator==(const Vec3f& a, const Vec3f& other)
	{
		return (a.x == other.x && a.y == other.y && a.z == other.z);
	}

	SHMINLINE float inner_product(Vec3f a, Vec3f b)
	{
		float res = a.x * b.x + a.y * b.y + a.z * b.z;
		return res;
	}

	SHMINLINE Vec3f cross_product(Vec3f v1, Vec3f v2)
	{
		return {
			v1.y * v2.z - v1.z * v2.y,
			v1.z * v2.x - v1.x * v2.z,
			v1.x * v2.y - v1.y * v2.x,
		};
	}

	SHMINLINE float length_squared(Vec3f a)
	{
		float res = inner_product(a, a);
		return res;
	}

	SHMINLINE float length(Vec3f a)
	{
		float res = sqrt(inner_product(a, a));
		return res;
	}

	SHMINLINE void normalize(Vec3f& a)
	{
		float32 l = length(a);
		a.x /= l;
		a.y /= l;
		a.z /= l;
	}

	SHMINLINE Vec3f normalized(Vec3f a)
	{
		normalize(a);
		return a;
	}

	SHMINLINE Vec3f vec_transform(Vec3f v, const Mat4& m)
	{
		Vec3f r;
		r.x = v.x * m.data[0 + 0] + v.y * m.data[4 + 0] + v.z * m.data[8 + 0] + 1.0f * m.data[12 + 0];
		r.y = v.x * m.data[0 + 1] + v.y * m.data[4 + 1] + v.z * m.data[8 + 1] + 1.0f * m.data[12 + 1];
		r.z = v.x * m.data[0 + 2] + v.y * m.data[4 + 2] + v.z * m.data[8 + 2] + 1.0f * m.data[12 + 2];
		return r;
	}

	SHMINLINE bool32 vec_compare(Vec3f v1, Vec3f v2, float32 tolerance = Constants::FLOAT_EPSILON)
	{
		return (
			abs(v1.x - v2.x) <= tolerance && 
			abs(v1.y - v2.y) <= tolerance && 
			abs(v1.z - v2.z) <= tolerance);
	}

	SHMINLINE float32 vec_distance(Vec3f v1, Vec3f v2)
	{
		Vec3f dist = { v2.x - v1.x, v2.y - v1.y, v2.z - v1.z };
		return length(dist);
	}

	SHMINLINE Vec3f vec_mul(Vec3f v1, Vec3f v2)
	{
		Vec3f r = { v2.x * v1.x, v2.y * v1.y, v2.z * v1.z };
		return r;
	}
	//------- Vec3i functions

	SHMINLINE Vec3i operator+(Vec3i a, Vec3i b)
	{
		return { a.x + b.x , a.y + b.y, a.z + b.z };
	}

	SHMINLINE Vec3i operator-(Vec3i a, Vec3i b)
	{
		return { a.x - b.x , a.y - b.y, a.z - b.z };
	}

	SHMINLINE Vec3i operator*(Vec3i a, int32 scalar)
	{
		Vec3i result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		result.z = a.z * scalar;
		return result;
	}

	SHMINLINE Vec3i operator*(int32 scalar, Vec3i a)
	{
		Vec3i result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		result.z = a.z * scalar;
		return result;
	}

	SHMINLINE Vec3i operator/(Vec3i a, const int32& divisor)
	{
		Vec3i result;
		result.x = a.x / divisor;
		result.y = a.y / divisor;
		result.z = a.z / divisor;
		return result;
	}

	SHMINLINE void operator*=(Vec3i& a, const int32& scalar)
	{
		a.x *= scalar;
		a.y *= scalar;
		a.z *= scalar;
	}

	SHMINLINE void operator/=(Vec3i& a, const int32& divisor)
	{
		a.x /= divisor;
		a.y /= divisor;
		a.z /= divisor;
	}

	SHMINLINE void operator+=(Vec3i& a, const Vec3i& other)
	{
		a.x += other.x;
		a.y += other.y;
		a.z += other.z;
	}

	SHMINLINE void operator-=(Vec3i& a, const Vec3i& other)
	{
		a.x -= other.x;
		a.y -= other.y;
		a.z -= other.z;
	}

	SHMINLINE bool8 operator==(const Vec3i& a, const Vec3i& other)
	{
		return (a.x == other.x && a.y == other.y && a.z == other.z);
	}

	SHMINLINE int32 inner_product(Vec3i a, Vec3i b)
	{
		int32 res = a.x * b.x + a.y * b.y + a.z * b.z;
		return res;
	}

	SHMINLINE int32 length_squared(Vec3i a)
	{
		int32 res = inner_product(a, a);
		return res;
	}

	SHMINLINE Vec3i vec_mul(Vec3i v1, Vec3i v2)
	{
		Vec3i r = { v2.x * v1.x, v2.y * v1.y, v2.z * v1.z };
		return r;
	}

}