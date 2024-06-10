#include "RendererFrontend.hpp"
#include "RendererBackend.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "memory/LinearAllocator.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/MaterialSystem.hpp"

// TODO: temporary
#include "utility/String.hpp"
#include "core/Event.hpp"
// end

namespace Renderer
{
	struct SystemState
	{
		Renderer::Backend backend;

		Math::Mat4 projection;
		Math::Mat4 view;

		float32 near_clip;
		float32 far_clip;
	};

	static SystemState* system_state;


	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, const char* application_name)
	{

		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		backend_create(RENDERER_BACKEND_TYPE_VULKAN, &system_state->backend);
		system_state->backend.frame_count = 0;

		if (!system_state->backend.init(&system_state->backend, application_name))
		{
			SHMFATAL("ERROR: Failed to initialize renderer backend.");
			return false;
		}

		system_state->near_clip = 0.1f;
		system_state->far_clip = 1000.0f;
		system_state->projection = Math::mat_perspective(Math::deg_to_rad(45.0f), 1280 / 720.0f, system_state->near_clip, system_state->far_clip);
		system_state->view = Math::mat_translation({0.0f, 0.0f, -30.0f});

		return true;
	}

	void system_shutdown()
	{
		if (system_state)
		{
			system_state->backend.shutdown(&system_state->backend);
		}		

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

			system_state->backend.update_global_state(system_state->projection, system_state->view, VEC3_ZERO, VEC4F_ONE, 0);		

			for (uint32 i = 0; i < data->geometry_count; i++)
			{
				system_state->backend.draw_geometry(data->geometries[i]);
			}

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
		{
			system_state->projection = Math::mat_perspective(Math::deg_to_rad(45.0f), width / (float32)height, system_state->near_clip, system_state->far_clip);
			system_state->backend.on_resized(&system_state->backend, width, height);
		}		
		else
			SHMWARN("Renderer backend does not exist to accept resize!");
	}

	void set_view(Math::Mat4 view)
	{
		system_state->view = view;
	}

	void create_texture(const void* pixels, Texture* texture)
	{
		system_state->backend.create_texture(pixels, texture);
	}

	void destroy_texture(Texture* texture)
	{
		system_state->backend.destroy_texture(texture);
	}

	bool32 create_material(Material* material)
	{
		return system_state->backend.create_material(material);
	}

	void destroy_material(Material* material)
	{
		system_state->backend.destroy_material(material);
	}

	bool32 create_geometry(Geometry* geometry, uint32 vertex_count, const Vert3* vertices, uint32 index_count, const uint32* indices)
	{
		return system_state->backend.create_geometry(geometry, vertex_count, vertices, index_count, indices);
	}

	void destroy_geometry(Geometry* geometry)
	{
		system_state->backend.destroy_geometry(geometry);
	}
	
}