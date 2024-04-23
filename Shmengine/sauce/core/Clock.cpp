#include "Clock.hpp"
#include "platform/Platform.hpp"

void clock_update(Clock* clock)
{
	if (clock->start_time)
		clock->elapsed = (float32)(Platform::get_absolute_time() - clock->start_time);
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
