#include "../Math.hpp"
#include <math.h>
#include <stdlib.h>

#include "platform/Platform.hpp"

namespace Math
{

	static uint32 rand_seed = 0;

	int32 round_f_to_i(float32 x)
	{
		return (int32)roundf(x);
	}
	int64 round_f_to_i64(float32 x)
	{
		return (int64)roundf(x);
	}

	int32 floor_f_to_i(float32 x)
	{
		return (int32)floorf(x);
	}

	int32 ceil_f_to_i(float32 x)
	{
		return (int32)ceilf(x);
	}

	float32 sqrt(float32 a)
	{
		float32 res = sqrtf(a);
		return res;
	}

	float32 sin(float32 x)
	{
		return sinf(x);
	}

	float32 cos(float32 x)
	{
		return cosf(x);
	}

	float32 tan(float32 x)
	{
		return tanf(x);
	}

	float32 acos(float32 x)
	{
		return acosf(x);
	}

	float32 abs(float32 x)
	{
		return fabsf(x);
	}

	static uint32 pcg_hash()
	{
		if (!rand_seed)
			rand_seed = (uint32)Platform::get_absolute_time();

		uint32 state = rand_seed * 747796405u + 2891336453u;
		uint32 word = ((state >> ((state >> 28u) + 4u)) ^ state);
		return (word >> 22u) ^ word;
	}

	uint32 random_uint32()
	{
		rand_seed = pcg_hash();
		return rand_seed;
	}

	int32 random_int32()
	{
		rand_seed = pcg_hash();
		return *(int32*)&rand_seed;
	}

	int32 random_int32_clamped(int32 min, int32 max)
	{
		rand_seed = pcg_hash();
		int32 res = (*(int32*)&rand_seed % (max - min + 1)) + min;
		return res;
	}

	float32 random_float32()
	{
		rand_seed = pcg_hash();
		return *(float32*)&rand_seed / RAND_MAX;
	}

	float32 random_float32_clamped(float32 min, float32 max)
	{
		rand_seed = pcg_hash();
		float32 res = (*(float32*)&rand_seed / RAND_MAX / (max - min)) + min;
		return res;
	}

}

