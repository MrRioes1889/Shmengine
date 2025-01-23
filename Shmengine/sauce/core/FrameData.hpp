#pragma once

#include "Defines.hpp"
#include "memory/LinearAllocator.hpp"

struct FrameData
{
	float64 delta_time;
	float64 total_time;

	uint32 drawn_geometry_count;

	Memory::LinearAllocator frame_allocator;
	void* app_data;
};