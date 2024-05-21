#pragma once

#include "Defines.hpp"
#include "Vec2.hpp"
#include "Vec4.hpp"

namespace Math
{

#pragma pack(push, 1)

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

#pragma pack(pop)

	typedef Vec4f Quat;
}
