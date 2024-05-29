#pragma once

#include "Defines.hpp"
#include "Common.hpp"

#define VEC2F_ADD(a, b) (a##.x + b##.x, a##.y + b##.y)
#define VEC2F_SUB(a, b) (a##.x - b##.x, a##.y - b##.y)
#define VEC2F_SCALE(a, b) (a##.x * b, a##.y * b)
#define VEC2F_DIV(a, b) (a##.x / b, a##.y / b)

#define VEC2_ZERO {0, 0}
#define VEC2_ONE {1, 1}
#define VEC2F_ONE {1.0f, 1.0f}

#define VEC2F_DOWN {0.0f, -1.0f}
#define VEC2F_UP {0.0f, 1.0f}
#define VEC2F_RIGHT {1.0f, 0.0f}
#define VEC2F_LEFT {-1.0f, 0.0f}

namespace Math
{

#pragma pack(push, 1)

	struct Vec2f
	{
		union
		{
			struct {
				float32 x;
				float32 y;
			};
			struct {
				float32 width;
				float32 height;
			};
			float32 e[2];
		};
	};

	struct Vec2i
	{
		union
		{
			struct {
				int32 x;
				int32 y;
			};
			struct {
				int32 width;
				int32 height;
			};
			int32 e[2];
		};
	};

	struct Vec2ui
	{
		union
		{
			struct {
				uint32 x;
				uint32 y;
			};
			struct {
				uint32 width;
				uint32 height;
			};
			uint32 e[2];
		};
	};

#pragma pack(pop)

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

	SHMINLINE bool32 vec_compare(Vec2f v1, Vec2f v2, float32 tolerance = FLOAT_EPSILON)
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

	//------- Vec2ui functions

	SHMINLINE Vec2ui operator+(Vec2ui a, Vec2ui b)
	{
		return { a.x + b.x , a.y + b.y };
	}

	SHMINLINE Vec2ui operator-(Vec2ui a, Vec2ui b)
	{
		return { a.x - b.x , a.y - b.y };
	}

	SHMINLINE Vec2ui operator*(Vec2ui a, uint32 scalar)
	{
		Vec2ui result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	SHMINLINE Vec2ui operator*(uint32 scalar, Vec2ui a)
	{
		Vec2ui result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	SHMINLINE Vec2ui operator/(Vec2ui a, const uint32& divisor)
	{
		Vec2ui result;
		result.x = a.x / divisor;
		result.y = a.y / divisor;
		return result;
	}

	SHMINLINE void operator*=(Vec2ui& a, const uint32& scalar)
	{
		a.x *= scalar;
		a.y *= scalar;
	}

	SHMINLINE void operator/=(Vec2ui& a, const uint32& divisor)
	{
		a.x /= divisor;
		a.y /= divisor;
	}

	SHMINLINE void operator+=(Vec2ui& a, const Vec2ui& other)
	{
		a.x += other.x;
		a.y += other.y;
	}

	SHMINLINE void operator-=(Vec2ui& a, const Vec2ui& other)
	{
		a.x -= other.x;
		a.y -= other.y;
	}

	SHMINLINE uint32 inner_product(Vec2ui a, Vec2ui b)
	{
		uint32 res = a.x * b.x + a.y * b.y;
		return res;
	}

	SHMINLINE int32 length_squared(Vec2ui a)
	{
		uint32 res = inner_product(a, a);
		return res;
	}

}