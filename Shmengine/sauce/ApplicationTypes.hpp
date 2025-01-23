#pragma once

#include "core/Engine.hpp"
#include "platform/Platform.hpp"
#include "systems/FontSystem.hpp"
#include "renderer/RendererTypes.hpp"

struct FrameData;

enum class ApplicationStage
{
	UNINITIALIZED,
	BOOTING,
	BOOT_COMPLETE,
	INITIALIZING,
	INITIALIZED,
	RUNNING,
	SHUTTING_DOWN
};

struct ApplicationConfig {

	int32 start_pos_x, start_pos_y;
	uint32 start_width, start_height;

	uint64 state_size;
	uint64 app_frame_data_size;

	char* name;

	FontSystem::SystemConfig fontsystem_config;
	Sarray<FontSystem::BitmapFontConfig> bitmap_font_configs;
	Sarray<FontSystem::TruetypeFontConfig> truetype_font_configs;

	Renderer::Module renderer_module;
	
};

struct Application
{
	ApplicationConfig config;

	typedef bool32(*FP_boot)(Application* app_inst);
	typedef bool32(*FP_init)(Application* app_inst);
	typedef void(*FP_shutdown)();
	typedef bool32(*FP_update)(FrameData* frame_data);
	typedef bool32(*FP_render)(Renderer::RenderPacket* packet, FrameData* frame_data);
	typedef void(*FP_on_resize)(uint32 width, uint32 height);
	typedef void(*FP_on_module_reload)(void* app_state);
	typedef void(*FP_on_module_unload)();
		
	FP_boot boot;
	FP_init init;
	FP_shutdown shutdown;
	FP_update update;
	FP_render render;
	FP_on_resize on_resize;
	FP_on_module_reload on_module_reload;
	FP_on_module_unload on_module_unload;

	ApplicationStage stage;

	Sarray<RenderView> render_views;
	
	void* state;
	void* engine_state;

	Platform::DynamicLibrary renderer_lib;
	Platform::DynamicLibrary application_lib;
};