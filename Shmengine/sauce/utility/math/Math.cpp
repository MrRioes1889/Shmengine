#include "../Math.hpp"
#include <math.h>

namespace Math
{

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

	static uint32 pcg_hash(uint32 seed)
	{
		uint32 state = seed * 747796405u + 2891336453u;
		uint32 word = ((state >> ((state >> 28u) + 4u)) ^ state);
		return (word >> 22u) ^ word;
	}

	uint32 random_uint32(uint32& seed)
	{
		seed = pcg_hash(seed);
		return seed;
	}

	int32 random_int32(uint32& seed)
	{
		seed = pcg_hash(seed);
		return *(int32*)&seed;
	}

	float32 random_float32(uint32& seed)
	{
		seed = pcg_hash(seed);
		return *(float32*)&seed;
	}

}

