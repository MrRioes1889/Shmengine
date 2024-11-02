#include "Application.hpp"

#include <entry.hpp>
#include <VulkanRendererModule.hpp>

// Define the function to create a game
bool32 create_application(Application* out_app) {
    // Application configuration.
    out_app->config.start_pos_x = 100;
    out_app->config.start_pos_y = 100;
    out_app->config.start_width = 1600;
    out_app->config.start_height = 900;
    out_app->config.name = (char*)"Shmengine Sandbox";
    out_app->init = application_init;
    out_app->boot = application_boot;
    out_app->update = application_update;
    out_app->render = application_render;
    out_app->on_resize = application_on_resize;
    out_app->shutdown = application_shutdown;

    if (!Renderer::Vulkan::create_module(&out_app->config.renderer_module))
        return false;

    // Create the game state.
    out_app->state_size = sizeof(ApplicationState);
    out_app->state = 0;

    return true;
}