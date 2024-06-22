#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"

struct Range
{
	uint64 offset;
	uint64 size;
};

SHMINLINE SHMAPI uint32 swap_endianness32(uint32 i)
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

SHMINLINE SHMAPI int32 swap_endianness32(int32 i)
{
	return (int32)swap_endianness32((uint32)i);
}

SHMINLINE SHMAPI uint64 swap_endianness64(uint64 i)
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

SHMINLINE SHMAPI int64 swap_endianness64(int64 i)
{
	return (int64)swap_endianness64((uint64)i);
}

SHMINLINE SHMAPI uint32 s_truncate_uint64(uint64 value)
{
	SHMASSERT(value < 0xFFFFFFFF);
	uint32 result = (uint32)value;
	return result;
}

SHMINLINE SHMAPI float32 clamp(float32 x, float32 min, float32 max)
{
	return (x < min ? min : (x > max ? max : x));
}

SHMINLINE SHMAPI uint32 clamp(uint32 x, uint32 min, uint32 max)
{
	return (x < min ? min : (x > max ? max : x));
}

SHMINLINE uint64 get_aligned(uint64 operand, uint64 granularity)
{
	return ((operand + (granularity - 1)) & ~(granularity - 1));
}

SHMINLINE Range get_aligned_range(uint64 offset, uint64 size, uint64 granularity)
{
	return { get_aligned(offset, granularity), get_aligned(size, granularity) };
}