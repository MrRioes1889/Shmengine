#pragma once

#include "Defines.hpp"

#include "math/Common.hpp"

#include "math/Vec2.hpp"
#include "math/Vec3.hpp"
#include "math/Vec4.hpp"

#include "math/Geometry.hpp"

namespace Math
{

	struct Vert3
	{
		Vec3f position;
	};
	
	SHMINLINE bool32 is_power_of_2(uint64 value) {
		return (value != 0) && (value & (value - 1) == 0);
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

	SHMINLINE uint32 swap_endianness32(uint32 i)
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

	SHMINLINE int32 swap_endianness32(int32 i)
	{
		return (int32)swap_endianness32((uint32)i);
	}

	SHMINLINE uint64 swap_endianness64(uint64 i)
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

	SHMINLINE int64 swap_endianness64(int64 i)
	{
		return (int64)swap_endianness64((uint64)i);
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


