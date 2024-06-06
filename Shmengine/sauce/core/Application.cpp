#include "Application.hpp"
#include "platform/Platform.hpp"
#include "ApplicationTypes.hpp"
#include "core/Memory.hpp"
#include "memory/LinearAllocator.hpp"
#include "core/Logging.hpp"

#include "core/Clock.hpp"
#include "core/Event.hpp"
#include "core/Input.hpp"

#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"

namespace Application
{

	struct ApplicationState
	{

		Game* game_inst;
		bool32 is_running;
		bool32 is_suspended;
		uint32 width, height;
		float32 last_time;
		Clock clock;

		Memory::LinearAllocator systems_allocator;

		void* logging_system_state;
		void* memory_system_state;
		void* event_system_state;
		void* input_system_state;
		void* renderer_system_state;
		void* texture_system_state;

	};

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data);
	bool32 on_key(uint16 code, void* sender, void* listener_inst, EventData data);
	bool32 on_resized(uint16 code, void* sender, void* listener_inst, EventData data);

	static bool32 initialized = false;
	static ApplicationState* app_state;

	void* allocate_subsystem_callback (uint64 size)
	{
		void* ptr = Memory::linear_allocator_allocate(&app_state->systems_allocator, size);
		Memory::zero_memory(ptr, size);
		return ptr;
	};

	bool32 init_primitive_subsystems(Game* game_inst)
	{

		*game_inst = {};
		game_inst->app_state = Memory::allocate(sizeof(ApplicationState), true, AllocationTag::RAW);
		app_state = (ApplicationState*)game_inst->app_state;

		app_state->game_inst = game_inst;
		app_state->is_running = true;
		app_state->is_suspended = false;
		Memory::linear_allocator_create(Mebibytes(64), &app_state->systems_allocator);

		Platform::init_console();

		if (!Log::system_init(allocate_subsystem_callback, app_state->logging_system_state))
		{
			SHMFATAL("Failed to initialize logging subsytem!");
			return false;
		}

		if (!Memory::system_init(allocate_subsystem_callback, app_state->memory_system_state))
		{
			SHMFATAL("Failed to initialize memory subsytem!");
			return false;
		}

		if (!Input::system_init(allocate_subsystem_callback, app_state->input_system_state))
		{
			SHMFATAL("Failed to initialize input subsystem!");
			return false;
		}

		if (!Event::system_init(allocate_subsystem_callback, app_state->event_system_state))
		{
			SHMFATAL("Failed to initialize event subsystem!");
			return false;
		}

		return true;
	}

	bool32 create(Game* game_inst)
	{

		if (initialized)
			return false;

		Event::event_register(EVENT_CODE_APPLICATION_QUIT, 0, on_event);
		Event::event_register(EVENT_CODE_KEY_PRESSED, 0, on_key);
		Event::event_register(EVENT_CODE_KEY_RELEASED, 0, on_key);
		Event::event_register(EVENT_CODE_WINDOW_RESIZED, 0, on_resized);			

		if (!Platform::system_init(
			allocate_subsystem_callback,
			app_state->memory_system_state,
			game_inst->config.name,
			game_inst->config.start_pos_x,
			game_inst->config.start_pos_y,
			game_inst->config.start_width,
			game_inst->config.start_height))
		{
			SHMFATAL("ERROR: Failed to startup platform layer!");
			return false;
		}

		if (!Renderer::system_init(allocate_subsystem_callback, app_state->renderer_system_state, game_inst->config.name))
		{
			SHMFATAL("ERROR: Failed to initialize renderer. Application shutting down..");
			return false;
		}

		TextureSystem::Config texture_sys_config;
		texture_sys_config.max_texture_count = 0xFFFF;
		if (!TextureSystem::system_init(allocate_subsystem_callback, app_state->texture_system_state, texture_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize texture system. Application shutting down..");
			return false;
		}

		if (!game_inst->init(game_inst))
		{
			SHMFATAL("ERROR: Failed to initialize game instance!");
			return false;
		}

		game_inst->on_resize(app_state->game_inst, app_state->width, app_state->height);

		initialized = true;
		return true;

	}

	bool32 run()
	{

		clock_start(&app_state->clock);
		clock_update(&app_state->clock);
		app_state->last_time = app_state->clock.elapsed;

		float32 running_time = 0;
		uint32 frame_count = 0;
		float32 target_frame_seconds = 1.0f / 120.0f;

		while (app_state->is_running)
		{

			clock_update(&app_state->clock);
			float32 current_time = app_state->clock.elapsed;
			float32 delta_time = current_time - app_state->last_time;
			float64 frame_start_time = Platform::get_absolute_time();
			app_state->last_time = current_time;

			if (!Platform::pump_messages())
			{
				app_state->is_running = false;
			}

			if (!app_state->is_suspended)
			{
				if (!app_state->game_inst->update(app_state->game_inst, delta_time))
				{
					app_state->is_running = false;
					break;
				}

				if (!app_state->game_inst->render(app_state->game_inst, delta_time))
				{
					app_state->is_running = false;
					break;
				}

				Renderer::RenderData r_data;
				r_data.delta_time = delta_time;
				Renderer::draw_frame(&r_data);
			}

			float64 frame_end_time = Platform::get_absolute_time();
			float64 frame_elapsed_time = frame_end_time - frame_start_time;
			running_time += (float32)frame_elapsed_time;
			float64 remaining_s = target_frame_seconds - frame_elapsed_time;

			if (remaining_s > 0)
			{
				uint32 remaining_ms = (uint32)(remaining_s * 1000);

				bool32 limit_frames = false;
				if (limit_frames)
					Platform::sleep(remaining_ms);
			}
			frame_count++;

			Input::system_update(delta_time);	
		}

		app_state->is_running = false;
	
		TextureSystem::system_shutdown();
		Renderer::system_shutdown();
		Platform::system_shutdown();
		Event::system_shutdown();
		Input::system_shutdown();
		Memory::system_shutdown();
		Log::system_shutdown();	

		return true;

	}

	void get_framebuffer_size(uint32* width, uint32* height)
	{
		*width = app_state->width;
		*height = app_state->height;
	}

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		switch (code)
		{
		case EVENT_CODE_APPLICATION_QUIT:
		{
			SHMINFO("Application Quit event received. Shutting down.");
			app_state->is_running = false;
			return true;
		}
		}

		return false;
	}

	bool32 on_key(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		if (code == EVENT_CODE_KEY_PRESSED)
		{
			uint32 key_code = data.ui32[0];

			switch (key_code)
			{
			case Keys::KEY_ESCAPE:
				Event::event_fire(EVENT_CODE_APPLICATION_QUIT, 0, {});
				return true;
				break;
			case Keys::KEY_A:
				SHMDEBUG("A key pressed!");
				break;
			default:
				SHMDEBUGV("'%c' key pressed!", key_code);
				break;
			}
		}
		else if (code == EVENT_CODE_KEY_RELEASED)
		{
			uint32 key_code = data.ui32[0];

			switch (key_code)
			{
			case Keys::KEY_B:
				SHMDEBUG("B key released!");
				break;
			default:
				SHMDEBUGV("'%c' key released!", key_code);
				break;
			}
		}
		return false;

	}

	bool32 on_resized(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		if (code == EVENT_CODE_WINDOW_RESIZED)
		{
			uint32 width = data.ui32[0];
			uint32 height = data.ui32[1];

			if (width != app_state->width || height != app_state->height)
			{
				app_state->width = width;
				app_state->height = height;

				SHMDEBUGV("Window resize occured: %u, %u", width, height);

				if (!width || !height)
				{
					SHMDEBUG("Window minimized. Suspending application.");
					app_state->is_suspended = true;
					return true;
				}
				else
				{
					if (app_state->is_suspended)
					{
						SHMDEBUG("Window restores. Continuing application.");
						app_state->is_suspended = false;
					}
					app_state->game_inst->on_resize(app_state->game_inst, width, height);
					Renderer::on_resized(width, height);
				}
				
			}
				
		}

		return false;
	}

}