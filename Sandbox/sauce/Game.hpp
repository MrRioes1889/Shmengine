#pragma once

#include <defines.hpp>
#include <ApplicationTypes.hpp>

typedef struct game_state {
    float32 delta_time;
} game_state;

bool32 game_init(Game* game_inst);

bool32 game_update(Game* game_inst, float32 delta_time);

bool32 game_render(Game* game_inst, float32 delta_time);

void game_on_resize(Game* game_inst, uint32 width, uint32 height);