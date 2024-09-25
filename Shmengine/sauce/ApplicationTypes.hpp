#pragma once

#include "core/Application.hpp"
#include "memory/LinearAllocator.hpp"

struct Game
{
	Application::Config config;
	
	bool32 (*boot)(Game* game_inst);
	bool32(*init)(Game* game_inst);
	bool32 (*update)(Game* game_inst, float64 delta_time);
	bool32 (*render)(Game* game_inst, Renderer::RenderPacket* packet, float64 delta_time);
	void (*on_resize)(Game* game_inst, uint32 width, uint32 height);
	void (*shutdown)(Game* game_inst);

	Memory::LinearAllocator frame_allocator;

	uint64 state_size;
	void* state;

	void* app_state;
};