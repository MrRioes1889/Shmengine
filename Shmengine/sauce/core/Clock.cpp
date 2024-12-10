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

struct MetricsState {
	static const uint32 avg_count = 30;

	float64 frame_start_timestamp;
	float64 logic_finish_timestamp;
	float64 render_finish_timestamp;

	uint32 frame_avg_counter;
	int32 frames;
	float64 ms_times[avg_count];
	float64 ms_avg;	
	float64 accumulated_frame_ms;
	float64 fps;

	float64 last_frametime;
	float64 logic_time;
	float64 render_time;
};

static MetricsState metrics_state = {};

void metrics_update_frame()
{

	float64 frame_end_timestamp = Platform::get_absolute_time();
	metrics_state.last_frametime = frame_end_timestamp - metrics_state.frame_start_timestamp;
	metrics_state.frame_start_timestamp = frame_end_timestamp;
	//metrics_state.last_frametime = elapsed_time;

	float64 frame_ms = (metrics_state.last_frametime * 1000.0);
	metrics_state.ms_times[metrics_state.frame_avg_counter] = frame_ms;
	if (metrics_state.frame_avg_counter == MetricsState::avg_count - 1) 
	{
		for (uint32 i = 0; i < MetricsState::avg_count; ++i) 
		{
			metrics_state.ms_avg += metrics_state.ms_times[i];
		}
		metrics_state.ms_avg /= (float64)MetricsState::avg_count;
	}
	metrics_state.frame_avg_counter++;
	metrics_state.frame_avg_counter %= MetricsState::avg_count;
	// Calculate frames per second.
	metrics_state.accumulated_frame_ms += frame_ms;
	if (metrics_state.accumulated_frame_ms > 1000) {
		metrics_state.fps = metrics_state.frames;
		metrics_state.accumulated_frame_ms -= 1000;
		metrics_state.frames = 0;
	}
	// Count all frames.
	metrics_state.frames++;

}

void metrics_update_logic()
{
	metrics_state.logic_finish_timestamp = Platform::get_absolute_time();
	metrics_state.logic_time = metrics_state.logic_finish_timestamp - metrics_state.frame_start_timestamp;
}

void metrics_update_render()
{
	metrics_state.render_finish_timestamp = Platform::get_absolute_time();
	metrics_state.render_time = metrics_state.render_finish_timestamp - metrics_state.logic_finish_timestamp;
}

SHMINLINE float64 metrics_fps()
{
	return metrics_state.fps;
}

SHMINLINE float64 metrics_frametime_avg()
{
	return metrics_state.ms_avg;
}

SHMINLINE float64 metrics_last_frametime()
{
	return metrics_state.last_frametime;
}

SHMINLINE float64 metrics_logic_time()
{
	return metrics_state.logic_time;
}

SHMINLINE float64 metrics_render_time()
{
	return metrics_state.render_time;
}

SHMINLINE void metrics_frame_time(float64* out_fps, float64* out_frametime)
{
	*out_fps = metrics_state.fps;
	*out_frametime = metrics_state.ms_avg;
}

SHMINLINE float64 metrics_frame_start_time()
{
	return metrics_state.frame_start_timestamp;
}