#pragma once

#include <defines.hpp>

struct Application;
namespace Renderer
{
    struct RenderPacket;
}

extern "C"
{

    SHMAPI bool32 application_boot(Application* app_inst);

    SHMAPI bool32 application_init(Application* app_inst);

    SHMAPI void application_shutdown(Application* app_inst);

    SHMAPI bool32 application_update(Application* app_inst, float64 delta_time);

    SHMAPI bool32 application_render(Application* app_inst, Renderer::RenderPacket* packet, float64 delta_time);

    SHMAPI void application_on_resize(Application* app_inst, uint32 width, uint32 height);

    SHMAPI void application_on_module_reload(Application* app_inst);
    SHMAPI void application_on_module_unload(Application* app_inst);

}