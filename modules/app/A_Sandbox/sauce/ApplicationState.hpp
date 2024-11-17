#pragma once

#include <defines.hpp>
#include <ApplicationTypes.hpp>

#include <core/Keymap.hpp>
#include <utility/MathTypes.hpp>
#include <systems/CameraSystem.hpp>

#include "DebugConsole.hpp"

// TODO: temp
#include <resources/UIText.hpp>
#include <resources/Skybox.hpp>
// end

struct ApplicationState
{
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

    DirectionalLight dir_light;
    static const uint32 p_lights_count = 3;
    PointLight p_lights[p_lights_count];

    Input::Keymap console_keymap;

    DebugConsole::ConsoleState debug_console;

};

struct ApplicationFrameData
{
    Darray<Renderer::GeometryRenderData> world_geometries;
};