#pragma once

#include <Defines.hpp>

struct Application;
struct ApplicationConfig;
struct FrameData;
namespace Renderer
{
	struct RenderPacket;
}

namespace Sandbox2DRenderViews
{
	enum
	{
		CANVAS,
		UI,
		VIEW_COUNT
	};
}

extern "C"
{

	SHMAPI bool32 application_load_config(ApplicationConfig* out_config);

	SHMAPI bool32 application_init(Application* app_inst);

	SHMAPI void application_shutdown();

	SHMAPI bool32 application_update(FrameData* frame_data);

	SHMAPI bool32 application_render(FrameData* frame_data);

	SHMAPI void application_on_resize(uint32 width, uint32 height);

	SHMAPI void application_on_module_reload(void* app_state);
	SHMAPI void application_on_module_unload();

}
