#pragma once

#include <defines.hpp>
#include <ApplicationTypes.hpp>

#include <core/Keymap.hpp>
#include <utility/MathTypes.hpp>
#include <systems/CameraSystem.hpp>

#include "DebugConsole.hpp"

// TODO: temp
#include <resources/Mesh.hpp>
#include <resources/UIText.hpp>
// end

struct ApplicationState
{

    uint32 allocation_count;
    uint32 width, height;

    Darray<Mesh> ui_meshes;
    UIText debug_info_text;

    Input::Keymap console_keymap;
    DebugConsole::ConsoleState debug_console;

};

struct ApplicationFrameData
{
};