#pragma once

#include <defines.hpp>
#include <ApplicationTypes.hpp>

#include <utility/Math.hpp>

typedef struct GameState {
    float32 delta_time;
    Math::Mat4 view;
    Math::Vec3f camera_position;
    Math::Vec3f camera_euler;
    bool32 camera_view_dirty;
} game_state;

bool32 game_init(Game* game_inst);

bool32 game_update(Game* game_inst, float32 delta_time);

bool32 game_render(Game* game_inst, float32 delta_time);

void game_on_resize(Game* game_inst, uint32 width, uint32 height);