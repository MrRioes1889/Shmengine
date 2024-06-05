#pragma once

#include "Defines.hpp"

#include "math/Common.hpp"

#include "math/Vec2.hpp"
#include "math/Vec3.hpp"
#include "math/Vec4.hpp"
#include "math/Mat.hpp"

#include "math/Geometry.hpp"

namespace Math
{

	struct Vert3
	{
		Vec3f position;
		Vec2f tex_coordinates;
	};
	
	SHMINLINE bool32 is_power_of_2(uint64 value) {
		return (value != 0) && ((value & (value - 1)) == 0);
	}

	SHMINLINE int32 pow(int32 a, int32 b)
	{
		if (b <= 0)
			return 1;

		int32 ret = a;
		for (int i = 0; i < b - 1; i++)
			ret *= a;

		return ret;
	}

	SHMINLINE float pow(float a, int32 b)
	{
		if (b <= 0)
			return 1;

		float ret = a;
		for (int i = 0; i < b - 1; i++)
			ret *= a;

		return ret;
	}

	SHMINLINE float32 deg_to_rad(float32 deg)
	{
		return deg * DEG2RAD_MULTIPLIER;
	}

	SHMINLINE float32 rad_to_deg(float32 rad)
	{
		return rad * RAD2DEG_MULTIPLIER;
	}

}


