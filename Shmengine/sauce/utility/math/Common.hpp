#pragma once

#include "Defines.hpp"

namespace Math
{

	SHMAPI float32 sin(float32 x);
	SHMAPI float32 cos(float32 x);
	SHMAPI float32 tan(float32 x);
	SHMAPI float32 acos(float32 x);
	SHMAPI float32 abs(float32 x);
	SHMAPI float32 sqrt(float32 a);

	SHMAPI int32 round_f_to_i(float32 x);
	SHMAPI int64 round_f_to_i64(float32 x);
	SHMAPI int32 floor_f_to_i(float32 x);
	SHMAPI int32 ceil_f_to_i(float32 x);

	SHMAPI uint32 random_uint32();
	SHMAPI int32 random_int32();
	SHMAPI int32 random_int32_clamped(int32 min, int32 max);
	SHMAPI float32 random_float32();
	SHMAPI float32 random_float32_clamped(float32 min, float32 max);

}
