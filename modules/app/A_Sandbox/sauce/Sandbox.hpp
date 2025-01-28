#pragma once

#include <defines.hpp>

struct Application;
struct FrameData;
namespace Renderer
{
    struct RenderPacket;
}

namespace SandboxRenderViews
{
    enum
    {
        SKYBOX,
        WORLD,
        WORLD_EDITOR,
        UI,
        PICK,
        VIEW_COUNT
    };
}

extern "C"
{

    SHMAPI bool32 application_boot(Application* app_inst);

    SHMAPI bool32 application_init(Application* app_inst);

    SHMAPI void application_shutdown();

    SHMAPI bool32 application_update(FrameData* frame_data);

    SHMAPI bool32 application_render(Renderer::RenderPacket* packet, FrameData* frame_data);

    SHMAPI void application_on_resize(uint32 width, uint32 height);

    SHMAPI void application_on_module_reload(void* app_state);
    SHMAPI void application_on_module_unload();

}