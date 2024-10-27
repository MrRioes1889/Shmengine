#pragma once

#include <defines.hpp>
#include <ApplicationTypes.hpp>

#include <core/Keymap.hpp>
#include <utility/MathTypes.hpp>
#include <systems/CameraSystem.hpp>

// TODO: temp
#include "resources/UIText.hpp"
#include "resources/Skybox.hpp"
// end

struct ApplicationState 
{
    float64 delta_time;
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

    Input::Keymap console_keymap;

};

bool32 application_boot(Application* app_inst);

bool32 application_init(Application* app_inst);

bool32 application_update(Application* app_inst, float64 delta_time);

bool32 application_render(Application* app_inst, Renderer::RenderPacket* packet, float64 delta_time);

void application_on_resize(Application* app_inst, uint32 width, uint32 height);

void application_shutdown(Application* app_inst);