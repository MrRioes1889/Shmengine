#pragma once

#include "Defines.hpp"
#include "../MathTypes.hpp"
#include "Common.hpp"

namespace Math
{

	//------- Vec2f functions

	SHMINLINE Vec2f operator+(Vec2f a, Vec2f b)
	{
		return { a.x + b.x , a.y + b.y };
	}

	SHMINLINE Vec2f operator-(Vec2f a, Vec2f b)
	{
		return { a.x - b.x , a.y - b.y };
	}

	SHMINLINE Vec2f operator*(Vec2f a, float scalar)
	{
		Vec2f result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	SHMINLINE Vec2f operator*(float scalar, Vec2f a)
	{
		Vec2f result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	SHMINLINE Vec2f operator/(Vec2f a, const float& divisor)
	{
		Vec2f result;
		result.x = a.x / divisor;
		result.y = a.y / divisor;
		return result;
	}

	SHMINLINE void operator*=(Vec2f& a, const float& scalar)
	{
		a.x *= scalar;
		a.y *= scalar;
	}

	SHMINLINE void operator/=(Vec2f& a, const float& divisor)
	{
		a.x /= divisor;
		a.y /= divisor;
	}

	SHMINLINE void operator+=(Vec2f& a, const Vec2f& other)
	{
		a.x += other.x;
		a.y += other.y;
	}

	SHMINLINE void operator-=(Vec2f& a, const Vec2f& other)
	{
		a.x -= other.x;
		a.y -= other.y;
	}

	SHMINLINE bool8 operator==(const Vec2f& a, const Vec2f& other)
	{
		return (a.x == other.x && a.y == other.y);
	}

	SHMINLINE float inner_product(Vec2f a, Vec2f b)
	{
		float res = a.x * b.x + a.y * b.y;
		return res;
	}

	SHMINLINE float length_squared(Vec2f a)
	{
		float res = inner_product(a, a);
		return res;
	}	

	SHMINLINE float length(Vec2f a)
	{
		float res = sqrt(inner_product(a, a));
		return res;
	}

	SHMINLINE void normalize(Vec2f& a)
	{
		float32 l = length(a);
		a.x /= l;
		a.y /= l;
	}

	SHMINLINE Vec2f normalized(Vec2f a)
	{
		normalize(a);
		return a;
	}

	SHMINLINE bool8 vec_compare(Vec2f v1, Vec2f v2, float32 tolerance = Constants::FLOAT_EPSILON)
	{
		return (
			abs(v1.x - v2.x) <= tolerance && 
			abs(v1.y - v2.y) <= tolerance);
	}

	SHMINLINE float32 vec_distance(Vec2f v1, Vec2f v2)
	{
		Vec2f dist = { v2.x - v1.x, v2.y - v1.y };
		return length(dist);
	}

	SHMINLINE Vec2f vec_mul(Vec2f v1, Vec2f v2)
	{
		Vec2f r = { v2.x * v1.x, v2.y * v1.y};
		return r;
	}

	//------- Vec2i functions

	SHMINLINE Vec2i operator+(Vec2i a, Vec2i b)
	{
		return { a.x + b.x , a.y + b.y };
	}

	SHMINLINE Vec2i operator-(Vec2i a, Vec2i b)
	{
		return { a.x - b.x , a.y - b.y };
	}

	SHMINLINE Vec2i operator*(Vec2i a, int32 scalar)
	{
		Vec2i result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	SHMINLINE Vec2i operator*(int32 scalar, Vec2i a)
	{
		Vec2i result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	SHMINLINE Vec2i operator/(Vec2i a, const int32& divisor)
	{
		Vec2i result;
		result.x = a.x / divisor;
		result.y = a.y / divisor;
		return result;
	}

	SHMINLINE void operator*=(Vec2i& a, const int32& scalar)
	{
		a.x *= scalar;
		a.y *= scalar;
	}

	SHMINLINE void operator/=(Vec2i& a, const int32& divisor)
	{
		a.x /= divisor;
		a.y /= divisor;
	}

	SHMINLINE void operator+=(Vec2i& a, const Vec2i& other)
	{
		a.x += other.x;
		a.y += other.y;
	}

	SHMINLINE void operator-=(Vec2i& a, const Vec2i& other)
	{
		a.x -= other.x;
		a.y -= other.y;
	}

	SHMINLINE bool8 operator==(const Vec2i& a, const Vec2i& other)
	{
		return (a.x == other.x && a.y == other.y);
	}

	SHMINLINE int32 inner_product(Vec2i a, Vec2i b)
	{
		int32 res = a.x * b.x + a.y * b.y;
		return res;
	}

	SHMINLINE int32 length_squared(Vec2i a)
	{
		int32 res = inner_product(a, a);
		return res;
	}

	SHMINLINE Vec2i vec_mul(Vec2i v1, Vec2i v2)
	{
		Vec2i r = { v2.x * v1.x, v2.y * v1.y };
		return r;
	}

	//------- Vec2ui functions

	SHMINLINE Vec2u operator+(Vec2u a, Vec2u b)
	{
		return { a.x + b.x , a.y + b.y };
	}

	SHMINLINE Vec2u operator-(Vec2u a, Vec2u b)
	{
		return { a.x - b.x , a.y - b.y };
	}

	SHMINLINE Vec2u operator*(Vec2u a, uint32 scalar)
	{
		Vec2u result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	SHMINLINE Vec2u operator*(uint32 scalar, Vec2u a)
	{
		Vec2u result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	SHMINLINE Vec2u operator/(Vec2u a, const uint32& divisor)
	{
		Vec2u result;
		result.x = a.x / divisor;
		result.y = a.y / divisor;
		return result;
	}

	SHMINLINE void operator*=(Vec2u& a, const uint32& scalar)
	{
		a.x *= scalar;
		a.y *= scalar;
	}

	SHMINLINE void operator/=(Vec2u& a, const uint32& divisor)
	{
		a.x /= divisor;
		a.y /= divisor;
	}

	SHMINLINE void operator+=(Vec2u& a, const Vec2u& other)
	{
		a.x += other.x;
		a.y += other.y;
	}

	SHMINLINE void operator-=(Vec2u& a, const Vec2u& other)
	{
		a.x -= other.x;
		a.y -= other.y;
	}

	SHMINLINE bool8 operator==(const Vec2u& a, const Vec2u& other)
	{
		return (a.x == other.x && a.y == other.y);
	}

	SHMINLINE uint32 inner_product(Vec2u a, Vec2u b)
	{
		uint32 res = a.x * b.x + a.y * b.y;
		return res;
	}

	SHMINLINE int32 length_squared(Vec2u a)
	{
		uint32 res = inner_product(a, a);
		return res;
	}

	SHMINLINE Vec2u vec_mul(Vec2u v1, Vec2u v2)
	{
		Vec2u r = { v2.x * v1.x, v2.y * v1.y};
		return r;
	}

}