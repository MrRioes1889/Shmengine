#include "RendererFrontend.hpp"
#include "RendererBackend.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "memory/LinearAllocator.hpp"

namespace Renderer
{
	struct SystemState
	{
		Renderer::Backend backend;
	};

	static SystemState* system_state;

	bool32 init(void* linear_allocator, void*& out_state, const char* application_name)
	{
		Memory::LinearAllocator* allocator = (Memory::LinearAllocator*)linear_allocator;
		out_state = Memory::linear_allocator_allocate(allocator, sizeof(SystemState));
		system_state = (SystemState*)out_state;

		backend_create(RENDERER_BACKEND_TYPE_VULKAN, &system_state->backend);
		system_state->backend.frame_count = 0;

		if (!system_state->backend.init(&system_state->backend, application_name))
		{
			SHMFATAL("ERROR: Failed to initialize renderer backend.");
			return false;
		}

		return true;
	}

	void shutdown()
	{
		if (system_state)
			system_state->backend.shutdown(&system_state->backend);

		system_state = 0;
	}

	static bool32 begin_frame(float32 delta_time)
	{
		return system_state->backend.begin_frame(&system_state->backend, delta_time);
	}

	static bool32 end_frame(float32 delta_time)
	{
		bool32 r = system_state->backend.end_frame(&system_state->backend, delta_time);
		system_state->backend.frame_count++;
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
		if (system_state)
			system_state->backend.on_resized(&system_state->backend, width, height);
		else
			SHMWARN("Renderer backend does not exist to accept resize!");
	}

	
}