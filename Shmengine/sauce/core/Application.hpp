#pragma once

#include "Defines.hpp"

struct Game;

namespace Application
{

	struct Config {

		int32 start_pos_x, start_pos_y;
		int32 start_width, start_height;

		char* name;

	};

	SHMAPI bool32 init_primitive_subsystems(Game* game_inst);

	SHMAPI bool32 create(Game* game_inst);

	SHMAPI bool32 run();

	void get_framebuffer_size(uint32* width, uint32* height);
}

