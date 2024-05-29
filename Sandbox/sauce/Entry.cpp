#include "game.hpp"

#include <entry.hpp>
#include <core/Memory.hpp>

// Define the function to create a game
bool32 create_game(Game* out_game) {
    // Application configuration.
    out_game->config.start_pos_x = 100;
    out_game->config.start_pos_y = 100;
    out_game->config.start_width = 1280;
    out_game->config.start_height = 720;
    out_game->config.name = (char*)"Shmengine Sandbox";
    out_game->update = game_update;
    out_game->render = game_render;
    out_game->init = game_init;
    out_game->on_resize = game_on_resize;

    // Create the game state.
    out_game->state = Memory::allocate(sizeof(game_state), false, AllocationTag::MAIN);

    return true;
}