#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"
#include "systems/FontSystem.hpp"

struct Application;

namespace Engine
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

	SHMAPI bool32 init_primitive_subsystems(Application* game_inst);

	SHMAPI bool32 create(Application* game_inst);

	SHMAPI bool32 run();

}

