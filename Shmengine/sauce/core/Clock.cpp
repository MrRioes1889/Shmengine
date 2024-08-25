#include "Clock.hpp"
#include "platform/Platform.hpp"

TimerPool global_timerpool = TimerPool();

void clock_update(Clock* clock)
{
	if (clock->start_time)
		clock->elapsed = Platform::get_absolute_time() - clock->start_time;
}

void clock_start(Clock* clock)
{
	clock->start_time = Platform::get_absolute_time();
	clock->elapsed = 0.0;
}

void clock_stop(Clock* clock)
{
	clock->start_time = 0.0;
	clock->elapsed = 0.0;
}


void TimerPool::reset()
{
	if (timer_running)
		timer_stop();

	timer_count = 0;
}

void TimerPool::timer_start(const char* name)
{
	if (timer_count + 1 >= pool_size)
		return;

	if (timer_running)
		timer_stop();

	timer_names[timer_count++] = name;
	timer_running = true;
	last_timestamp = Platform::get_absolute_time();	
}

void TimerPool::timer_stop()
{
	if (!timer_running)
		return;

	timer_times[timer_count-1] = Platform::get_absolute_time() - last_timestamp;
	timer_running = false;
}