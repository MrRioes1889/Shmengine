#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"
#include "systems/FontSystem.hpp"

struct Game;

namespace Application
{

	struct Config {

		int32 start_pos_x, start_pos_y;
		int32 start_width, start_height;

		char* name;

		FontSystem::Config fontsystem_config;
		Sarray<FontSystem::BitmapFontConfig> bitmap_font_configs;
		Sarray<FontSystem::TruetypeFontConfig> truetype_font_configs;
		Sarray<Renderer::RenderViewConfig> render_view_configs;
	};

	SHMAPI bool32 init_primitive_subsystems(Game* game_inst);

	SHMAPI bool32 create(Game* game_inst);

	SHMAPI bool32 run();

	void get_framebuffer_size(uint32* width, uint32* height);
}

