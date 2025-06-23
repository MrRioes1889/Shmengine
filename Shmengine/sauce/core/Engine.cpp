#include "Engine.hpp"
#include "platform/Platform.hpp"
#include "platform/FileSystem.hpp"
#include "ApplicationTypes.hpp"
#include "core/Memory.hpp"
#include "core/FrameData.hpp"
#include "core/Logging.hpp"
#include "core/Clock.hpp"
#include "core/Event.hpp"
#include "core/Input.hpp"
#include "core/Console.hpp"
#include "utility/CString.hpp"

#include "renderer/RendererFrontend.hpp"
#include "systems/RenderViewSystem.hpp"

#include "core/Subsystems.hpp"

#include "optick.h"

typedef uint64(*FP_get_module_state_size)();

static const char* application_module_name = "A_Sandbox2D";

namespace Engine
{

	struct EngineState
	{
		Application* app_inst;
		bool32 is_running;
		//Clock clock;

		Memory::LinearAllocator systems_allocator;

		FrameData frame_data;
	};

	static bool32 initialized = false;
	static EngineState* engine_state = 0;

	static bool32 boot_application(ApplicationConfig* app_config);
	static bool32 load_application_library(const char* module_name, const char* lib_filename, bool32 reload);
	static bool32 on_watched_file_written(uint16 code, void* sender, void* listener_inst, EventData e_data);
	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data);
	bool32 on_resized(uint16 code, void* sender, void* listener_inst, EventData data);

	void* allocate_subsystem_callback (void* allocator, uint64 size)
	{
		Memory::LinearAllocator* lin_allocator = (Memory::LinearAllocator*)allocator;
		void* ptr = lin_allocator->allocate(size);
		return ptr;
	};

	bool32 init(Application* app_inst)
	{

		if (initialized)
			return false;

		app_inst->stage = ApplicationStage::UNINITIALIZED;

		if (!SubsystemManager::init_basic())
		{
			SHMFATAL("Failed to initialize subsystem manager!");
			return false;
		}

		engine_state = (EngineState*)Memory::allocate(sizeof(EngineState), AllocationTag::ENGINE);

		engine_state->app_inst = app_inst;
		engine_state->is_running = true;

		Event::event_register(SystemEventCode::APPLICATION_QUIT, app_inst, on_event);
		Event::event_register(SystemEventCode::WINDOW_RESIZED, app_inst, on_resized);

		app_inst->stage = ApplicationStage::BOOTING;
		ApplicationConfig app_config = {};
		if (!boot_application(&app_config))
		{
			SHMFATAL("Failed to init application!");
			return false;
		}
		app_inst->stage = ApplicationStage::BOOT_COMPLETE;

		if (!SubsystemManager::init_advanced(&app_config))
		{
			SHMFATAL("Failed to initialize subsystem manager!");
			return false;
		}

		app_inst->stage = ApplicationStage::INITIALIZING;
		if (!app_inst->init(app_inst))
		{
			SHMFATAL("ERROR: Failed to initialize game instance!");
			return false;
		}
		app_inst->stage = ApplicationStage::INITIALIZED;	

		Renderer::on_resized(app_inst->main_window->client_width, app_inst->main_window->client_height);
		app_inst->on_resize(app_inst->main_window->client_width, app_inst->main_window->client_height);	

		initialized = true;
		return true;

	}

	bool32 run(Application* app_inst)
	{
		uint32 frame_count = 0;
		float64 target_frame_seconds = 1.0f / 120.0f;
		app_inst->stage = ApplicationStage::RUNNING;

		Renderer::RenderPacket render_packet = {};

		float64 last_frametime = 0.0;
		metrics_update_frame();

		while (engine_state->is_running)
		{		
			metrics_update_frame();
			last_frametime = metrics_last_frametime();
			engine_state->frame_data.delta_time = last_frametime;
			engine_state->frame_data.total_time += last_frametime;

			OPTICK_FRAME("MainThread");

			engine_state->frame_data.frame_allocator.free_all_data();

			SubsystemManager::update(&engine_state->frame_data);

			if (!Platform::pump_messages())
			{
				engine_state->is_running = false;
			}

			static float64 timer = 0;
			timer += last_frametime;

			if (timer > 1.0)
			{
				char window_title[128] = {};
				CString::safe_print_s<float64>(window_title, 128, "Sandbox - Last frametime: %lf4 ms", engine_state->frame_data.delta_time * 1000.0);
				Platform::set_window_text(app_inst->main_window->handle, window_title);
				Platform::update_file_watches();
				timer = 0.0;
			}

			Input::frame_start();

			if (!app_inst->is_suspended)
			{

				if (!engine_state->app_inst->update(&engine_state->frame_data))
				{
					SHMFATAL("Failed to update application.");
					engine_state->is_running = false;
					break;
				}

				metrics_update_logic();

				render_packet = {};
				if (!engine_state->app_inst->render(&render_packet, &engine_state->frame_data))
				{
					SHMFATAL("Failed to render application.");
					engine_state->is_running = false;
					break;
				}

				Renderer::draw_frame(&render_packet, &engine_state->frame_data);

				for (uint32 i = 0; i < render_packet.views.capacity; i++)
				{
					RenderViewSystem::on_end_frame(render_packet.views[i]);
				}

				metrics_update_render();

			}

			Input::frame_end(&engine_state->frame_data);

			float64 frame_elapsed_time = Platform::get_absolute_time() - metrics_frame_start_time();
			static float64 remaining_s = target_frame_seconds - frame_elapsed_time;

			if (remaining_s > 0 && app_inst->limit_framerate)
			{
				// NOTE: Only using platform sleep for delays of > 2ms, since sleeping reliably for 1ms is asking too much, apparently
				static int32 remaining_ms = (int32)(remaining_s * 1000.0) - 2;
				if (remaining_ms > 0)
					Platform::sleep(remaining_ms);

				while(frame_elapsed_time < target_frame_seconds)
					frame_elapsed_time = Platform::get_absolute_time() - metrics_frame_start_time();
			}
			if (app_inst->limit_framerate)
			{
				while (frame_elapsed_time < target_frame_seconds)
					frame_elapsed_time = Platform::get_absolute_time() - metrics_frame_start_time();
			}

			frame_count++;			
		}

		OPTICK_SHUTDOWN();

		app_inst->stage = ApplicationStage::SHUTTING_DOWN;
		engine_state->is_running = false;
		engine_state->app_inst->shutdown();	

		SubsystemManager::shutdown_advanced();

		app_inst->on_module_unload();
		Platform::unload_dynamic_library(&app_inst->application_lib);
		app_inst->render_views.free_data();

		SubsystemManager::shutdown_basic();

		app_inst->stage = ApplicationStage::UNINITIALIZED;		

		return true;

	}


	static bool32 boot_application(ApplicationConfig* app_config)
	{
		char application_module_filename[Constants::max_filepath_length];
		char application_loaded_module_filename[Constants::max_filepath_length];
		CString::print_s(application_module_filename, Constants::max_filepath_length, "%s%s%s%s", Platform::get_root_dir(), Platform::dynamic_library_prefix, application_module_name, Platform::dynamic_library_ext);
		CString::print_s(application_loaded_module_filename, Constants::max_filepath_length, "%s%s%s_loaded%s", Platform::get_root_dir(), Platform::dynamic_library_prefix, application_module_name, Platform::dynamic_library_ext);

		Platform::ReturnCode r_code;
		do
		{
			r_code = FileSystem::file_copy(application_module_filename, application_loaded_module_filename, true);
			if (r_code == Platform::ReturnCode::FILE_LOCKED)
				Platform::sleep(100);
		} 
		while (r_code == Platform::ReturnCode::FILE_LOCKED);

		if (r_code != Platform::ReturnCode::SUCCESS)
		{
			SHMERRORV("Failed to copy dll file for module '%s'.", application_module_name);
			return false;
		}

		if (!load_application_library(application_module_name, application_loaded_module_filename, false))
		{
			SHMERRORV("Failed to load application library '%s'.", application_module_name);
			return false;
		}

		Application* app_inst = engine_state->app_inst;
		if (!app_inst->load_config(app_config))
		{
			SHMERRORV("Failed to load application config '%s'.", application_module_name);
			return false;
		}

		Platform::WindowConfig window_config = {};
		window_config.pos_x = app_config->start_pos_x;
		window_config.pos_y = app_config->start_pos_y;
		window_config.width = app_config->start_width;
		window_config.height = app_config->start_height;
		window_config.title = app_config->name;

		if (!Platform::create_window(window_config))
		{
			SHMERRORV("Failed to create window '%s'.", application_module_name);
			return false;
		}
		app_inst->main_window = Platform::get_active_window();
		
		app_inst->state = 0;
		if (app_config->state_size)
			app_inst->state = Memory::allocate(app_config->state_size, AllocationTag::APPLICATION);

		uint64 frame_allocator_size = mebibytes(32);
		void* f_data = Memory::allocate(frame_allocator_size, AllocationTag::ENGINE);
		engine_state->frame_data.frame_allocator.init(frame_allocator_size, f_data);

		if (app_config->app_frame_data_size)
			engine_state->frame_data.app_data = Memory::allocate(app_config->app_frame_data_size, AllocationTag::APPLICATION);

		app_inst->is_suspended = false;
		app_inst->name = app_config->name;
		app_inst->limit_framerate = app_config->limit_framerate;

		Event::event_register(SystemEventCode::WATCHED_FILE_WRITTEN, app_inst, on_watched_file_written);

		if (Platform::register_file_watch(application_module_filename, &app_inst->application_lib.watch_id) != Platform::ReturnCode::SUCCESS)
		{
			SHMERROR("Failed to register library file watch");
			return false;
		}

		return true;

	}

	static bool32 load_application_library(const char* module_name, const char* lib_filename, bool32 reload) 
	{
		Application* app = engine_state->app_inst;
		if (!Platform::load_dynamic_library(application_module_name, lib_filename, &app->application_lib))
			return false;

		if (!Platform::load_dynamic_library_function(&app->application_lib, "application_load_config", (void**)&app->load_config))
			return false;
		if (!Platform::load_dynamic_library_function(&app->application_lib, "application_init", (void**)&app->init))
			return false;
		if (!Platform::load_dynamic_library_function(&app->application_lib, "application_shutdown", (void**)&app->shutdown))
			return false;
		if (!Platform::load_dynamic_library_function(&app->application_lib, "application_update", (void**)&app->update))
			return false;
		if (!Platform::load_dynamic_library_function(&app->application_lib, "application_render", (void**)&app->render))
			return false;
		if (!Platform::load_dynamic_library_function(&app->application_lib, "application_on_resize", (void**)&app->on_resize))
			return false;
		if (!Platform::load_dynamic_library_function(&app->application_lib, "application_on_module_reload", (void**)&app->on_module_reload))
			return false;
		if (!Platform::load_dynamic_library_function(&app->application_lib, "application_on_module_unload", (void**)&app->on_module_unload))
			return false;

		if (reload)
			app->on_module_reload(app->state);

		return true;

	}

	static bool32 on_watched_file_written(uint16 code, void* sender, void* listener_inst, EventData e_data)
	{

		Application* app = (Application*)listener_inst;
		if (e_data.ui32[0] == app->application_lib.watch_id)
			SHMINFO("Hot reloading application library");

		app->on_module_unload();
		if (!Platform::unload_dynamic_library(&app->application_lib))
		{
			SHMERROR("Failed to unload application library.");
			return false;
		}

		Platform::sleep(100);

		char application_module_filename[Constants::max_filepath_length];
		char application_loaded_module_filename[Constants::max_filepath_length];
		CString::print_s(application_module_filename, Constants::max_filepath_length, "%s%s%s%s", Platform::get_root_dir(), Platform::dynamic_library_prefix, application_module_name, Platform::dynamic_library_ext);
		CString::print_s(application_loaded_module_filename, Constants::max_filepath_length, "%s%s%s_loaded%s", Platform::get_root_dir(), Platform::dynamic_library_prefix, application_module_name, Platform::dynamic_library_ext);

		Platform::ReturnCode r_code;
		do
		{    
			r_code = FileSystem::file_copy(application_module_filename, application_loaded_module_filename, true);
			if (r_code == Platform::ReturnCode::FILE_LOCKED)
				Platform::sleep(100);
		} 
		while (r_code == Platform::ReturnCode::FILE_LOCKED);

		if (r_code != Platform::ReturnCode::SUCCESS)
		{
			SHMERRORV("Failed to copy dll file for module '%s'.", application_module_name);
			return false;
		}

		// TODO: turn back to loaded filename
		if (!load_application_library(application_module_name, application_loaded_module_filename, true))
		{
			SHMERRORV("Failed to load application library '%s'.", application_module_name);
			return false;
		}

		return true;
	}

	float64 get_frame_delta_time()
	{
		return engine_state->frame_data.delta_time;
	}

	const char* get_application_name()
	{
		return engine_state->app_inst->name;
	}

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		switch (code)
		{
		case SystemEventCode::APPLICATION_QUIT:
		{
			SHMINFO("Application Quit event received. Shutting down.");
			engine_state->is_running = false;
			return true;
		}
		}

		return false;
	}

	bool32 on_resized(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		Application* app_inst = (Application*)listener_inst;
		
		if (code == SystemEventCode::WINDOW_RESIZED)
		{
			uint32 width = data.ui32[0];
			uint32 height = data.ui32[1];

			//SHMDEBUGV("Window resize occured: %u, %u", width, height);

			if (!width || !height)
			{
				SHMDEBUG("Window minimized. Suspending application.");
				app_inst->is_suspended = true;
				return true;
			}
			else
			{
				if (app_inst->is_suspended)
				{
					SHMDEBUG("Window restores. Continuing application.");
					app_inst->is_suspended = false;
				}
				engine_state->app_inst->on_resize(width, height);
				Renderer::on_resized(width, height);
			}
		}

		return false;
	}

}