#pragma once

#include "core/Engine.hpp"
#include "ApplicationTypes.hpp"
#include "core/Logging.hpp"
#include "core/Assert.hpp"
#include "platform/Platform.hpp"

extern bool32 create_application(Application* out_app);
extern bool32 init_application(Application* app_inst);

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance, _In_ LPSTR lpCmdLine, _In_ int n_show_cmd)
{

    try
    {
        Application app_inst = {};

        Platform::init_console();
        
        SHMINFOV("Shmengine Engine Version: %s", "0.001a");
        SHMINFO("Starting the engines :)");    

        if (!create_application(&app_inst)) {
            SHMERROR("Failed to create application!");
            return -2;
        }

        // Ensure the function pointers exist.
        if (!app_inst.render || !app_inst.update || !app_inst.init || !app_inst.on_resize) {
            SHMERROR("Failed to initialize function pointers!");
            return -3;
        }

        // Initialization.
        if (!Engine::init(&app_inst)) {
            SHMERROR("Failed to init engine!");
            return -4;
        }

        if (!init_application(&app_inst)) {
            SHMERROR("Failed to init application!");
            return -5;
        }

        // Begin the game loop.
        if (!Engine::run(&app_inst)) {
            return -1;
        }

        return 0;
    }
    catch (AssertException& exc)
    {
        Platform::message_box("Fatal Error", exc.message);
        return 1;
    }
}

#else

int main(void) {
    // Request the game instance from the application.
    Game game_inst;
    if (!Application::init_primitive_subsystems(&game_inst))
    {
        SHMFATAL("Failed to initialize vital subsystems. Shutting down.");
        return -1;
    }

    SHMINFOV("Shmengine Engine Version: %s", "0.001a");
    SHMINFO("Starting the engines :)");

    if (!create_game(&game_inst)) {
        SHMERROR("Failed to create game!");
        return -2;
    }

    // Ensure the function pointers exist.
    if (!game_inst.render || !game_inst.update || !game_inst.init || !game_inst.on_resize) {
        SHMERROR("Failed to initialize function pointers!");
        return -3;
    }

    // Initialization.
    if (!Application::create(&game_inst)) {
        SHMERROR("Failed to create_application!");
        return 1;
    }

    // Begin the game loop.
    if (!Application::run()) {
        return 2;
    }

    return 0;
}

#endif
