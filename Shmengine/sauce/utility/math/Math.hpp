#pragma once

#include "Defines.hpp"
//#include <math.h>

#define VEC2F_ADD(a, b) (a##.x + b##.x, a##.y + b##.y)
#define VEC2F_SUB(a, b) (a##.x - b##.x, a##.y - b##.y)
#define VEC2F_SCALE(a, b) (a##.x * b, a##.y * b)
#define VEC2F_DIV(a, b) (a##.x / b, a##.y / b)

#define VEC2F_DOWN {0.0f, -1.0f}
#define VEC2F_UP {0.0f, 1.0f}
#define VEC2F_RIGHT {1.0f, 0.0f}
#define VEC2F_LEFT {-1.0f, 0.0f}

#define PI 3.1415926f

#pragma pack(push, 1)

namespace Math
{

	struct Vec2f
	{
		union
		{
			struct {
				float x;
				float y;
			};
			struct {
				float width;
				float height;
			};
			float e[2];
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
			float e[2];
		};
	};

	struct Rect2D
	{
		Vec2f pos;
		uint32 width, height;
	};

	struct Circle2D
	{
		Vec2f pos;
		float radius;
	};

	struct vec3f
	{
		union
		{
			struct {
				float x;
				float y;
				float z;
			};
			struct {
				float r;
				float g;
				float b;
			};
			float e[3];
		};

		inline vec3f operator+(const vec3f& other)
		{
			vec3f result;
			result.x = this->x + other.x;
			result.y = this->y + other.y;
			result.z = this->z + other.z;
			return result;
		}

		inline vec3f operator-()
		{
			vec3f result;
			result.x = -this->x;
			result.y = -this->y;
			result.z = -this->z;
			return result;
		}

		inline vec3f operator*(const float& scalar)
		{
			vec3f result;
			result.x = this->x * scalar;
			result.y = this->y * scalar;
			result.z = this->z * scalar;
			return result;
		}

		inline void operator*=(const float& scalar)
		{
			*this = *this * scalar;
		}

		/*inline void operator/=(const float& divisor)
		{
			*this = *this / divisor;
		}*/

		inline void operator+=(const vec3f& other)
		{
			*this = *this + other;
		}

		/*inline void operator-=(const vec3f& other)
		{
			*this = *this - other;
		}*/
	};

	struct vec4f
	{
		union
		{
			struct {
				float x;
				float y;
				float z;
				float w;
			};
			struct {
				float r;
				float g;
				float b;
				float a;
			};
			float e[4];
		};

		inline vec4f operator+(const vec4f& other)
		{
			vec4f result;
			result.x = this->x + other.x;
			result.y = this->y + other.y;
			result.z = this->z + other.z;
			result.w = this->w + other.w;
			return result;
		}

		inline vec4f operator-()
		{
			vec4f result;
			result.x = -this->x;
			result.y = -this->y;
			result.z = -this->z;
			result.w = -this->w;
			return result;
		}

		inline vec4f operator*(const float& scalar)
		{
			vec4f result;
			result.x = this->x * scalar;
			result.y = this->y * scalar;
			result.z = this->z * scalar;
			result.w = this->w * scalar;
			return result;
		}

		inline void operator*=(const float& scalar)
		{
			*this = *this * scalar;
		}

		/*inline void operator/=(const float& divisor)
		{
			*this = *this / divisor;
		}*/

		inline void operator+=(const vec4f& other)
		{
			*this = *this + other;
		}

		/*inline void operator-=(const vec4f& other)
		{
			*this = *this - other;
		}*/
	};
#pragma pack(pop)

	inline Vec2f operator+(Vec2f a, Vec2f b)
	{
		return { a.x + b.x , a.y + b.y };
	}

	inline Vec2f operator-(Vec2f a, Vec2f b)
	{
		return { a.x - b.x , a.y - b.y };
	}

	//inline vec2f operator-(vec2f& a)
	//{
	//	vec2f result;
	//	result.x = -a.x;
	//	result.y = -a.y;
	//	return result;
	//}

	inline Vec2f operator*(Vec2f a, float scalar)
	{
		Vec2f result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	inline Vec2f operator*(float scalar, Vec2f a)
	{
		Vec2f result;
		result.x = a.x * scalar;
		result.y = a.y * scalar;
		return result;
	}

	inline Vec2f operator/(Vec2f a, const float& divisor)
	{
		Vec2f result;
		result.x = a.x / divisor;
		result.y = a.y / divisor;
		return result;
	}

	inline void operator*=(Vec2f& a, const float& scalar)
	{
		a = a * scalar;
	}

	inline void operator/=(Vec2f& a, const float& divisor)
	{
		a = a / divisor;
	}

	inline void operator+=(Vec2f& a, const Vec2f& other)
	{
		a = a + other;
	}

	inline void operator-=(Vec2f& a, const Vec2f& other)
	{
		a = a - other;
	}

	int32 round_f_to_i(float x);
	int64 round_f_to_i64(float x);
	int32 floor_f_to_i(float x);
	int32 ceil_f_to_i(float x);
	float length(Vec2f a);
	float sqrrt(float a);

	inline float inner_product(Vec2f a, Vec2f b)
	{
		float res = a.x * b.x + a.y * b.y;
		return res;
	}

	inline float length_squared(Vec2f a)
	{
		float res = inner_product(a, a);
		return res;
	}

	

	inline int32 pow(int32 a, int32 b)
	{
		if (b <= 0)
			return 1;

		int32 ret = a;
		for (int i = 0; i < b - 1; i++)
			ret *= a;

		return ret;
	}

	inline float pow(float a, int32 b)
	{
		if (b <= 0)
			return 1;

		float ret = a;
		for (int i = 0; i < b - 1; i++)
			ret *= a;

		return ret;
	}

	inline uint32 swap_endianness32(uint32 i)
	{
		uint32 b0, b1, b2, b3;
		uint32 res;

		b0 = (i & 0x000000FF) << 24;
		b1 = (i & 0x0000FF00) << 8;
		b2 = (i & 0x00FF0000) >> 8;
		b3 = (i & 0xFF000000) >> 24;

		res = b0 | b1 | b2 | b3;
		return res;
	}

	inline int32 swap_endianness32(int32 i)
	{
		return (int32)swap_endianness32((uint32)i);
	}

	inline uint64 swap_endianness64(uint64 i)
	{
		uint64 b0, b1, b2, b3, b4, b5, b6, b7;
		uint64 res;

		b0 = (i & 0x00000000000000FF) << 56;
		b1 = (i & 0x000000000000FF00) << 40;
		b2 = (i & 0x0000000000FF0000) << 24;
		b3 = (i & 0x00000000FF000000) << 8;
		b4 = (i & 0x000000FF00000000) >> 8;
		b5 = (i & 0x0000FF0000000000) >> 24;
		b6 = (i & 0x00FF000000000000) >> 40;
		b7 = (i & 0xFF00000000000000) >> 56;

		res = b0 | b1 | b2 | b3 | b4 | b5 | b6 | b7;
		return res;
	}

	inline int64 swap_endianness64(int64 i)
	{
		return (int64)swap_endianness64((uint64)i);
	}

	uint32 get_random_uint32(uint32& seed);

}


