#include "Application.hpp"
#include "platform/Platform.hpp"
#include "ApplicationTypes.hpp"
#include "core/Memory.hpp"
#include "core/Logging.hpp"

#include "core/Event.hpp"
#include "core/Input.hpp"
#include "core/Clock.hpp"

#include "renderer/RendererFrontend.hpp"

struct ApplicationState 
{

	Game* game_inst;
	bool32 is_running;
	bool32 is_suspended;
	Platform::PlatformState platform;
	int32 width, height;
	float32 last_time;
	Clock clock;

};

namespace Application
{

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data);
	bool32 on_key(uint16 code, void* sender, void* listener_inst, EventData data);

	static bool32 initialized = false;
	static ApplicationState app_state;

	bool32 init_primitive_subsystems()
	{
		Platform::init_console();
		Memory::init_memory();

		if (!Input::system_init())
		{
			SHMFATAL("ERROR: Failed to initialize input subsystem!");
			return false;
		}

		if (!event_init())
		{
			SHMFATAL("ERROR: Failed to initialize event subsystem!");
			return false;
		}

		return true;
	}

	bool32 create(Game* game_inst)
	{

		if (initialized)
			return false;

		event_register(EVENT_CODE_APPLICATION_QUIT, 0, on_event);
		event_register(EVENT_CODE_KEY_PRESSED, 0, on_key);
		event_register(EVENT_CODE_KEY_RELEASED, 0, on_key);

		app_state.game_inst = game_inst;
		app_state.is_running = true;
		app_state.is_suspended = false;			

		if (!Platform::startup(
			&app_state.platform,
			game_inst->config.name,
			game_inst->config.start_pos_x,
			game_inst->config.start_pos_y,
			game_inst->config.start_width,
			game_inst->config.start_height))
		{
			SHMFATAL("ERROR: Failed to startup platform layer!");
			return false;
		}

		if (!Renderer::init(game_inst->config.name, &app_state.platform))
		{
			SHMFATAL("ERROR: Failed to initialize renderer. Application shutting down..");
			return false;
		}

		if (!game_inst->init(game_inst))
		{
			SHMFATAL("ERROR: Failed to initialize game instance!");
			return false;
		}

		game_inst->on_resize(app_state.game_inst, app_state.width, app_state.height);

		initialized = true;
		return true;

	}

	bool32 run()
	{

		clock_start(&app_state.clock);
		clock_update(&app_state.clock);
		app_state.last_time = app_state.clock.elapsed;

		float32 running_time = 0;
		uint32 frame_count = 0;
		float32 target_frame_seconds = 1.0f / 120.0f;

		while (app_state.is_running)
		{
			clock_update(&app_state.clock);
			float32 current_time = app_state.clock.elapsed;
			float32 delta_time = current_time - app_state.last_time;
			float64 frame_start_time = Platform::get_absolute_time();
			app_state.last_time = current_time;

			if (!Platform::pump_messages(&app_state.platform))
			{
				app_state.is_running = false;
			}

			if (!app_state.is_suspended)
			{
				if (!app_state.game_inst->update(app_state.game_inst, delta_time))
				{
					app_state.is_running = false;
					break;
				}

				if (!app_state.game_inst->render(app_state.game_inst, delta_time))
				{
					app_state.is_running = false;
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

		app_state.is_running = false;
	
		event_shutdown();
		Input::system_shutdown();
		Renderer::shutdown();

		Platform::shutdown(&app_state.platform);

		return true;

	}

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		switch (code)
		{
		case EVENT_CODE_APPLICATION_QUIT:
		{
			SHMINFO("Application Quit event received. Shutting down.");
			app_state.is_running = false;
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
				event_fire(EVENT_CODE_APPLICATION_QUIT, 0, {});
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

}