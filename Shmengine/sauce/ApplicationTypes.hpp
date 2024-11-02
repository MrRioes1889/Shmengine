#pragma once

#include "core/Engine.hpp"
#include "memory/LinearAllocator.hpp"

struct ApplicationFrameData
{
	Darray<Renderer::GeometryRenderData> world_geometries;
};

struct ApplicationConfig {

	int32 start_pos_x, start_pos_y;
	int32 start_width, start_height;

	char* name;

	FontSystem::SystemConfig fontsystem_config;
	Sarray<FontSystem::BitmapFontConfig> bitmap_font_configs;
	Sarray<FontSystem::TruetypeFontConfig> truetype_font_configs;
	Sarray<Renderer::RenderViewConfig> render_view_configs;

	Renderer::Module renderer_module;
};

struct Application
{
	ApplicationConfig config;
	
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

	void* engine_state;
};