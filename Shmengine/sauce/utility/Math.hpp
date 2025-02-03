#pragma once

#include "Defines.hpp"

#include "MathTypes.hpp"
#include "math/Common.hpp"

#include "math/Vec2.hpp"
#include "math/Vec3.hpp"
#include "math/Vec4.hpp"
#include "math/Mat.hpp"

#include "math/Geometry.hpp"

namespace Math
{
	
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

	SHMINLINE float32 pow(float32 a, int32 b)
	{
		if (b <= 0)
			return 1;

		float32 ret = a;
		for (int i = 0; i < b - 1; i++)
			ret *= a;

		return ret;
	}

	SHMINLINE float64 pow(float64 a, int32 b)
	{
		if (b <= 0)
			return 1;

		float64 ret = a;
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

	SHMINLINE float32 range_convert_f32(float32 value, float32 old_min, float32 old_max, float32 new_min, float32 new_max) 
	{
		return (((value - old_min) * (new_max - new_min)) / (old_max - old_min)) + new_min;
	}

	SHMINLINE uint32 rgb_to_uint32(uint32 r, uint32 g, uint32 b) 
	{
		return (((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
	}

	SHMINLINE void uint32_to_rgb(uint32 rgbu, uint32* out_r, uint32* out_g, uint32* out_b) 
	{
		*out_r = (rgbu >> 16) & 0x0FF;
		*out_g = (rgbu >> 8) & 0x0FF;
		*out_b = (rgbu) & 0x0FF;
	}

	SHMINLINE Math::Vec3f rgb_uint32_to_vec3(uint32 r, uint32 g, uint32 b) 
	{
		Math::Vec3f res;
		res.r = (float32)r / 255.0f;
		res.g = (float32)g / 255.0f;
		res.b = (float32)b / 255.0f;
		return res;
	}

	SHMINLINE void vec3_to_rgb_uint32(Math::Vec3f v, uint32* out_r, uint32* out_g, uint32* out_b) 
	{
		*out_r = (uint32)(v.r * 255.0f);
		*out_g = (uint32)(v.g * 255.0f);
		*out_b = (uint32)(v.b * 255.0f);
	}

	SHMINLINE bool8 float_cmp(float32 f0, float32 f1, float32 epsilon = FLOAT_EPSILON)
	{
		return (Math::abs(f0 - f1) < epsilon);
	}

}


