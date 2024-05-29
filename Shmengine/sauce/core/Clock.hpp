#pragma once

#include "Defines.hpp"

struct Clock
{
	float64 start_time;
	float32 elapsed;
};

SHMAPI void clock_update(Clock* clock);
SHMAPI void clock_start(Clock* clock);
SHMAPI void clock_stop(Clock* clock);
