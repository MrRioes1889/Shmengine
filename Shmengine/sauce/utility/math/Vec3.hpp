#pragma once

#include "Defines.hpp"

#define VEC3_ZERO {0, 0, 0}
#define VEC3_ONE {1, 1, 1}
#define VEC3F_ONE {1.0f, 1.0f, 1.0f}
		   
#define VEC3F_DOWN {0.0f, -1.0f, 0.0f}
#define VEC3F_UP {0.0f, 1.0f, 0.0f}
#define VEC3F_RIGHT {1.0f, 0.0f, 0.0f}
#define VEC3F_LEFT {-1.0f, 0.0f, 0.0f}
#define VEC3F_FRONT {0.0f, 0.0f, -1.0f}
#define VEC3F_BACK {0.0f, 0.0f, 1.0f}

namespace Math
{

//#pragma pack(push, 1)

	struct Vec3f
	{
		union
		{
			struct {
				float32 x;
				float32 y;
				float32 z;
			};
			struct {
				float32 r;
				float32 g;
				float32 b;
			};
			struct {
				float32 pitch;
				float32 yaw;
				float32 roll;
			};
			float32 e[3];
		};

	};

	struct Vec3i
	{
		union
		{
			struct {
				int32 x;
				int32 y;
				int32 z;
			};
			struct {
				int32 r;
				int32 g;
				int32 b;
			};
			int32 e[3];
		};

	};

//#pragma pack(pop)

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

	SHMINLINE bool32 vec_compare(Vec3f v1, Vec3f v2, float32 tolerance = FLOAT_EPSILON)
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

}