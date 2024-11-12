#pragma once

#include "Defines.hpp"

namespace Memory { struct LinearAllocator; }

struct FrameData
{
	float64 delta_time;
	float64 total_time;

	Memory::LinearAllocator* frame_allocator;
	void* app_data;
};