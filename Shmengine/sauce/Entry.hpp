#pragma once

#include "core/Application.hpp"
#include "ApplicationTypes.hpp"
#include "core/Logging.hpp"

// Externally-defined function to create a game.
extern bool32 create_game(Game* out_game);

/**
 * The main entry point of the application.
 */
#ifdef _WIN32

#include <windows.h>

int WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance, _In_ LPSTR lpCmdLine, _In_ int n_show_cmd)
{
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
