#pragma once

#include "core/Engine.hpp"
#include "memory/LinearAllocator.hpp"
#include "platform/Platform.hpp"

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

	typedef bool32(*FP_boot)(Application* game_inst);
	typedef bool32(*FP_init)(Application* game_inst);
	typedef void(*FP_shutdown)(Application* game_inst);
	typedef bool32(*FP_update)(Application* app_inst, float64 delta_time);
	typedef bool32(*FP_render)(Application* app_inst, Renderer::RenderPacket* packet, float64 delta_time);
	typedef void(*FP_on_resize)(Application* game_inst, uint32 width, uint32 height);
		
	FP_boot boot;
	FP_init init;
	FP_shutdown shutdown;
	FP_update update;
	FP_render render;
	FP_on_resize on_resize;

	Memory::LinearAllocator frame_allocator;
	ApplicationFrameData frame_data;

	uint64 state_size;
	void* state;

	void* engine_state;

	Platform::DynamicLibrary renderer_lib;
	Platform::DynamicLibrary application_lib;
};