#pragma once

#include <defines.hpp>
#include <ApplicationTypes.hpp>

#include <utility/MathTypes.hpp>
#include <systems/CameraSystem.hpp>

// TODO: temp
#include "resources/UIText.hpp"
#include "resources/Skybox.hpp"
// end

typedef struct GameState {
    float32 delta_time;
    uint32 allocation_count;
    bool32 world_meshes_loaded;
    uint32 hovered_object_id;
    uint32 width, height;

    Camera* world_camera;
    Math::Frustum camera_frustum;

    Skybox skybox;

    Darray<Mesh> world_meshes;
    Mesh* car_mesh;
    Mesh* sponza_mesh; 

    Darray<Mesh> ui_meshes;
    UIText test_bitmap_text;
    UIText test_truetype_text;

} game_state;

bool32 game_boot(Game* game_inst);

bool32 game_init(Game* game_inst);

bool32 game_update(Game* game_inst, float64 delta_time);

bool32 game_render(Game* game_inst, Renderer::RenderPacket* packet, float64 delta_time);

void game_on_resize(Game* game_inst, uint32 width, uint32 height);

void game_shutdown(Game* game_inst);