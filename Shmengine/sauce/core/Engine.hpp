#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"
#include "systems/FontSystem.hpp"

struct Application;

namespace Engine
{

	SHMAPI bool32 init(Application* game_inst);
	SHMAPI bool32 run();

	void on_event_system_initialized();	

}

