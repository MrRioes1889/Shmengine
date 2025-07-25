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
	bool8 timer_running;

	void reset();
	void timer_start(const char* name);
	void timer_stop();
};

extern TimerPool global_timerpool;

SHMAPI void metrics_update_frame();
SHMAPI void metrics_update_logic();
SHMAPI void metrics_update_render();
SHMAPI float64 metrics_fps();
SHMAPI float64 metrics_frametime_avg();
SHMAPI float64 metrics_last_frametime();
SHMAPI float64 metrics_logic_time();
SHMAPI float64 metrics_render_time();
SHMAPI void metrics_frame_time(float64* out_fps, float64* out_frametime);
SHMAPI float64 metrics_frame_start_time();
