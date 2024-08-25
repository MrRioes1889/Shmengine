#pragma once

#include "Defines.hpp"

struct Clock
{
	float64 start_time;
	float64 elapsed;
};

SHMAPI void clock_update(Clock* clock);
SHMAPI void clock_start(Clock* clock);
SHMAPI void clock_stop(Clock* clock);

struct TimerPool
{
	const static uint32 pool_size = 16;
	float64 last_timestamp;
	const char* timer_names[pool_size];
	float64 timer_times[pool_size];
	uint32 timer_count;
	bool32 timer_running;

	void reset();
	void timer_start(const char* name);
	void timer_stop();
};

extern TimerPool global_timerpool;
