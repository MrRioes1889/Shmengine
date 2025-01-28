#pragma once

#include <defines.hpp>
#include <ApplicationTypes.hpp>

#include <core/Keymap.hpp>
#include <utility/MathTypes.hpp>
#include <systems/CameraSystem.hpp>

#include "DebugConsole.hpp"

#include "resources/Scene.hpp"

// TODO: temp
#include <resources/UIText.hpp>
#include <resources/Skybox.hpp>
#include <resources/Mesh.hpp>
#include <resources/Box3D.hpp>
#include <resources/Gizmo3D.hpp>
// end

struct ApplicationState
{
    uint32 allocation_count;
    uint32 hovered_object_id;
    uint32 width, height;

    Camera* world_camera;
    Math::Frustum camera_frustum;

    Scene main_scene;

    Gizmo3D editor_gizmo;

    Darray<Mesh> ui_meshes;
    UIText debug_info_text;

    Input::Keymap console_keymap;

    DebugConsole::ConsoleState debug_console;

};

struct ApplicationFrameData
{
};