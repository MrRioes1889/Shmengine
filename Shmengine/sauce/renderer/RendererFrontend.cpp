#include "RendererFrontend.hpp"
#include "RendererBackend.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"

namespace Renderer
{
	static Backend* backend = 0;

	bool32 init(const char* application_name, Platform::PlatformState* plat_state)
	{
		backend = (Backend*)Memory::allocate(sizeof(Backend), true);

		backend_create(RENDERER_BACKEND_TYPE_VULKAN, plat_state, backend);

		if (!backend->init(backend, application_name, plat_state))
		{
			SHMFATAL("ERROR: Failed to initialize renderer backend.");
			return false;
		}

		return true;
	}

	void shutdown()
	{
		backend->shutdown(backend);
		Memory::free_memory(backend, true);
	}

	static bool32 begin_frame(float32 delta_time)
	{
		return backend->begin_frame(backend, delta_time);
	}

	static bool32 end_frame(float32 delta_time)
	{
		bool32 r = backend->end_frame(backend, delta_time);
		backend->frame_count++;
		return r;
	}

	bool32 draw_frame(RenderData* data)
	{
		if (begin_frame(data->delta_time))
		{
			if (!end_frame(data->delta_time))
			{
				SHMERROR("ERROR: Failed to finish drawing frame. Application shutting down...");
				return false;
			};
		}

		return true;
	}

	void on_resized(uint32 width, uint32 height)
	{
	}

	
}