#include "RendererFrontend.hpp"
#include "RendererBackend.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "memory/LinearAllocator.hpp"

#include "utility/Math.hpp"

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
			Math::Mat4 projection = Math::mat_perspective(Math::deg_to_rad(45.0f), 1280 / 720.0f, 0.1f, 1000.0f);
			static float32 z = -30.0f;
			//z -= 0.005f;
			Math::Mat4 view = Math::mat_translation({ 0.0f, 0.0f, z });

			system_state->backend.update_global_state(projection, view, VEC3_ZERO, VEC4F_ONE, 0);

			static float32 angle = 0.01f;
			angle += 0.001f;
			Math::Quat rotation = Math::quat_from_axis_angle(VEC3F_UP, angle, false);
			Math::Mat4 model = Math::quat_to_rotation_matrix(rotation, VEC3_ZERO);
			system_state->backend.update_object(model);

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