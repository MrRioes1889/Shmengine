#pragma once

#include "core/Engine.hpp"
#include "platform/Platform.hpp"
#include "systems/FontSystem.hpp"
#include "systems/RenderViewSystem.hpp"
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

struct ApplicationConfig 
{

	const char* name;
	const char* renderer_module_name;
	int32 start_pos_x, start_pos_y;
	uint32 start_width, start_height;

	uint64 state_size;
	uint64 app_frame_data_size;

	bool8 limit_framerate;
	
};

struct Application
{
	typedef bool32(*FP_load_config)(ApplicationConfig* out_config);
	typedef bool32(*FP_init)(Application* app_inst);
	typedef void(*FP_shutdown)();
	typedef bool32(*FP_update)(FrameData* frame_data);
	typedef bool32(*FP_render)(Renderer::RenderPacket* packet, FrameData* frame_data);
	typedef void(*FP_on_resize)(uint32 width, uint32 height);
	typedef void(*FP_on_module_reload)(void* app_state);
	typedef void(*FP_on_module_unload)();

	FP_load_config load_config;
	FP_init init;
	FP_shutdown shutdown;
	FP_update update;
	FP_render render;
	FP_on_resize on_resize;
	FP_on_module_reload on_module_reload;
	FP_on_module_unload on_module_unload;

	Sarray<RenderView> render_views;

	void* state;

	const Platform::Window* main_window;

	Platform::DynamicLibrary application_lib;

	const char* name;

	ApplicationStage stage;
	bool8 limit_framerate;
	bool32 is_suspended;

};