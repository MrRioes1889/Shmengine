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
    Application::init_primitive_subsystems();
    SHMINFOV("Shmengine Engine Version: %c\n", "0.001a");
    SHMINFO("Starting the engines :)\n");

    // Request the game instance from the application.
    Game game_inst;
    if (!create_game(&game_inst)) {
        SHMERROR("Failed to create game!");
        return -1;
    }  

    // Ensure the function pointers exist.
    if (!game_inst.render || !game_inst.update || !game_inst.init || !game_inst.on_resize) {
        SHMERROR("Failed to initialize function pointers!");
        return -2;
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

    Application::init_primitive_subsystems();
    SHMINFOV("Shmengine Engine Version: %i\n", 1);
    SHMINFO("Starting the engines :)\n");

    // Request the game instance from the application.
    Game game_inst;
    if (!create_game(&game_inst)) {
        SHMERROR("Failed to create game!");
        return -1;
    }

    // Ensure the function pointers exist.
    if (!game_inst.render || !game_inst.update || !game_inst.init || !game_inst.on_resize) {
        SHMERROR("Failed to initialize function pointers!");
        return -2;
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
