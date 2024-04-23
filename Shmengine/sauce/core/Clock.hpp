#pragma once

#include "Defines.hpp"

struct Clock
{
	float64 start_time;
	float32 elapsed;
};

void clock_update(Clock* clock);
void clock_start(Clock* clock);
void clock_stop(Clock* clock);
