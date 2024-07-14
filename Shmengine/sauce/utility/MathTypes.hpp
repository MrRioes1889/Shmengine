#pragma once

#include "Defines.hpp"

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

#define VEC3_ZERO {0, 0, 0}
#define VEC3_ONE {1, 1, 1}
#define VEC3F_ONE {1.0f, 1.0f, 1.0f}

#define VEC3F_DOWN {0.0f, -1.0f, 0.0f}
#define VEC3F_UP {0.0f, 1.0f, 0.0f}
#define VEC3F_RIGHT {1.0f, 0.0f, 0.0f}
#define VEC3F_LEFT {-1.0f, 0.0f, 0.0f}
#define VEC3F_FRONT {0.0f, 0.0f, -1.0f}
#define VEC3F_BACK {0.0f, 0.0f, 1.0f}

#define VEC4_ZERO {0, 0, 0, 0}
#define VEC4_ONE {1, 1, 1, 1}
#define VEC4F_ONE {1.0f, 1.0f, 1.0f, 1.0f}

#define QUAT_IDENTITY {0.0f, 0.0f, 0.0f, 1.0f}

#define MAT4_IDENTITY \
{1.0f, 0.0f, 0.0f, 0.0f,\
 0.0f, 1.0f, 0.0f, 0.0f,\
 0.0f, 0.0f, 1.0f, 0.0f,\
 0.0f, 0.0f, 0.0f, 1.0f}	

namespace Math
{

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

	struct Vec2u
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

	struct Vec4f //alignas(16) Vec4f
	{
		union
		{
			//__m128 data;
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

	};

	struct Mat4//alignas(16) Mat4
	{
		union
		{
			float32 data[16];

			Vec4f rows[4];
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

	typedef Vec4f Quat;

	struct Transform
	{
		Vec3f position;
		Quat rotation;
		Vec3f scale;

		Mat4 local;
		Transform* parent;
		bool32 is_dirty;
	};

	struct Extents2D
	{
		Vec2f min;
		Vec2f max;
	};

	struct Extents3D
	{
		Vec3f min;
		Vec3f max;
	};

}