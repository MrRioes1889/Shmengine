#include "RendererFrontend.hpp"
#include "RendererBackend.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "memory/LinearAllocator.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/MaterialSystem.hpp"

// TODO: temporary
#include "utility/CString.hpp"
#include "core/Event.hpp"
// end

namespace Renderer
{
	struct SystemState
	{
		Renderer::Backend backend;

		Math::Mat4 world_projection;
		Math::Mat4 world_view;
		Math::Mat4 ui_projection;
		Math::Mat4 ui_view;

		float32 near_clip;
		float32 far_clip;
	};

	static SystemState* system_state;


	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, const char* application_name)
	{

		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		backend_create(VULKAN, &system_state->backend);
		system_state->backend.frame_count = 0;

		if (!system_state->backend.init(application_name))
		{
			SHMFATAL("ERROR: Failed to initialize renderer backend.");
			return false;
		}

		system_state->near_clip = 0.1f;
		system_state->far_clip = 1000.0f;
		system_state->world_projection = Math::mat_perspective(Math::deg_to_rad(45.0f), 1280 / 720.0f, system_state->near_clip, system_state->far_clip);
		system_state->world_view = Math::mat_translation({0.0f, 0.0f, -30.0f});

		system_state->ui_projection = Math::mat_orthographic(0.0f, 1280.0f, 720.0f, 0.0f, -100.0f, 100.0f);
		system_state->ui_view = Math::mat_inverse(MAT4_IDENTITY);

		return true;
	}

	void system_shutdown()
	{
		if (system_state)
		{
			system_state->backend.shutdown();
		}		

		system_state = 0;
	}

	bool32 draw_frame(RenderData* data)
	{
		Backend& backend = system_state->backend;

		if (backend.begin_frame(data->delta_time))
		{

			if (!backend.begin_renderpass(RenderpassType::WORLD))
			{
				SHMERROR("draw_frame - BuiltinRenderpass::WORLD failed to begin renderpass!");
				return false;
			}

			backend.update_global_world_state(system_state->world_projection, system_state->world_view, VEC3_ZERO, VEC4F_ONE, 0);		

			for (uint32 i = 0; i < data->world_geometry_count; i++)
			{
				backend.draw_geometry(data->world_geometries[i]);
			}

			if (!backend.end_renderpass(RenderpassType::WORLD))
			{
				SHMERROR("draw_frame - BuiltinRenderpass::WORLD failed to end renderpass!");
				return false;
			}

			if (!backend.begin_renderpass(RenderpassType::UI))
			{
				SHMERROR("draw_frame - BuiltinRenderpass::UI failed to begin renderpass!");
				return false;
			}

			backend.update_global_ui_state(system_state->ui_projection, system_state->ui_view, 0);

			for (uint32 i = 0; i < data->ui_geometry_count; i++)
			{
				backend.draw_geometry(data->ui_geometries[i]);
			}

			if (!backend.end_renderpass(RenderpassType::UI))
			{
				SHMERROR("draw_frame - BuiltinRenderpass::UI failed to end renderpass!");
				return false;
			}

			if (!backend.end_frame(data->delta_time))
			{
				SHMERROR("ERROR: Failed to finish drawing frame. Application shutting down...");
				return false;
			};
			backend.frame_count++;

		}

		return true;
	}

	void on_resized(uint32 width, uint32 height)
	{
		if (system_state)
		{
			system_state->world_projection = Math::mat_perspective(Math::deg_to_rad(45.0f), width / (float32)height, system_state->near_clip, system_state->far_clip);
			system_state->ui_projection = Math::mat_orthographic(0.0f, (float32)width, (float32)height, 0.0f, -100.0f, 100.0f);
			system_state->backend.on_resized(width, height);
		}		
		else
			SHMWARN("Renderer backend does not exist to accept resize!");
	}

	void set_view(Math::Mat4 view)
	{
		system_state->world_view = view;
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

	bool32 create_geometry(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices)
	{
		return system_state->backend.create_geometry(geometry, vertex_size, vertex_count, vertices, index_count, indices);
	}

	void destroy_geometry(Geometry* geometry)
	{
		system_state->backend.destroy_geometry(geometry);
	}
	
}