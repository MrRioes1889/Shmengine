#pragma once

#include <defines.hpp>
#include <ApplicationTypes.hpp>

#include <utility/MathTypes.hpp>
#include <systems/CameraSystem.hpp>

typedef struct GameState {
    float32 delta_time;
    uint32 allocation_count;
    Camera* world_camera;
} game_state;

bool32 game_init(Game* game_inst);

bool32 game_update(Game* game_inst, float32 delta_time);

bool32 game_render(Game* game_inst, float32 delta_time);

void game_on_resize(Game* game_inst, uint32 width, uint32 height);