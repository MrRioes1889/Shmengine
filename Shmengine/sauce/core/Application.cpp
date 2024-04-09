#include "Application.hpp"
#include "platform/Platform.hpp"
#include "ApplicationTypes.hpp"
#include "core/Memory.hpp"

struct application_state 
{

	Game* game_inst;
	bool32 is_running;
	bool32 is_suspended;
	Platform::PlatformState platform;
	int32 width, height;
	float64 last_time;

};

namespace Application
{

	static bool32 initialized = false;
	static application_state app_state;

	bool32 init_primitive_subsystems()
	{
		Platform::init_console();
		Memory::init_memory();
		return true;
	}

	bool32 create(Game* game_inst)
	{

		if (initialized)
			return false;

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
			return false;
		}

		if (!game_inst->init(game_inst))
		{
			return false;
		}

		game_inst->on_resize(app_state.game_inst, app_state.width, app_state.height);

		initialized = true;
		return true;

	}

	bool32 run()
	{

		while (app_state.is_running)
		{
			if (!Platform::pump_messages(&app_state.platform))
			{
				app_state.is_running = false;
			}

			if (!app_state.is_suspended)
			{
				if (!app_state.game_inst->update(app_state.game_inst, 0.0f))
				{
					app_state.is_running = false;
					break;
				}

				if (!app_state.game_inst->render(app_state.game_inst, 0.0f))
				{
					app_state.is_running = false;
					break;
				}
			}
		}

		app_state.is_running = false;

		Platform::shutdown(&app_state.platform);

		return true;

	}

}