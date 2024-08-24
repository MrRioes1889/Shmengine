#pragma once

#include "core/Application.hpp"

struct Game
{
	Application::Config config;

	bool32 (*init)(Game* game_inst);
	bool32 (*update)(Game* game_inst, float64 delta_time);
	bool32 (*render)(Game* game_inst, float64 delta_time);
	void (*on_resize)(Game* game_inst, uint32 width, uint32 height);

	uint64 state_size;
	void* state;

	void* app_state;
};