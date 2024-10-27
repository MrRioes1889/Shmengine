#pragma once

#include "core/Engine.hpp"
#include "memory/LinearAllocator.hpp"

struct ApplicationFrameData
{
	Darray<Renderer::GeometryRenderData> world_geometries;
};

struct Application
{
	Engine::Config config;
	
	bool32 (*boot)(Application* game_inst);
	bool32(*init)(Application* game_inst);
	bool32 (*update)(Application* game_inst, float64 delta_time);
	bool32 (*render)(Application* game_inst, Renderer::RenderPacket* packet, float64 delta_time);
	void (*on_resize)(Application* game_inst, uint32 width, uint32 height);
	void (*shutdown)(Application* game_inst);

	Memory::LinearAllocator frame_allocator;
	ApplicationFrameData frame_data;

	uint64 state_size;
	void* state;

	void* app_state;
};