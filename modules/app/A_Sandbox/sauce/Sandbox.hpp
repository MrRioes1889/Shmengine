#pragma once

#include <defines.hpp>

struct Application;
struct FrameData;
namespace Renderer
{
    struct RenderPacket;
}

extern "C"
{

    SHMAPI bool32 application_boot(Application* application);

    SHMAPI bool32 application_init();

    SHMAPI void application_shutdown();

    SHMAPI bool32 application_update(const FrameData* frame_data);

    SHMAPI bool32 application_render(Renderer::RenderPacket* packet, const FrameData* frame_data);

    SHMAPI void application_on_resize(uint32 width, uint32 height);

    SHMAPI void application_on_module_reload(Application* application);
    SHMAPI void application_on_module_unload();

}