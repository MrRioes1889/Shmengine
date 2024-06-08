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

		// TODO: temporary
		Material* test_material;
		// end
	};

	static SystemState* system_state;

	bool32 event_on_debug_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		const char* names[3] = {
			"cobblestone",
			"paving",
			"paving2"
		};
		
		static int32 choice = 2;
		const char* old_name = names[choice];
		choice++;
		choice %= 3;

		system_state->test_material->diffuse_map.texture = TextureSystem::acquire(names[choice], true);
		if (!system_state->test_material->diffuse_map.texture)
		{
			SHMWARN("event_on_debug_event no texture! using default.");
			system_state->test_material->diffuse_map.texture = TextureSystem::get_default_texture();
		}
		TextureSystem::release(old_name);
		return true;
	}

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, const char* application_name)
	{

		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		Event::event_register(EVENT_CODE_DEBUG0, system_state, event_on_debug_event);

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
			Event::event_unregister(EVENT_CODE_DEBUG0, system_state, event_on_debug_event);

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

			// TODO: temporary
			if (!system_state->test_material)
			{
				system_state->test_material = MaterialSystem::acquire("test_material");
				if (!system_state->test_material)
				{
					SHMWARN("Automatic material load failed, falling back to manual default material!");

					MaterialSystem::MaterialConfig config;
					String::copy(Material::max_name_length, config.name, "test_material");
					config.auto_release = false;
					config.diffuse_color = VEC4F_ONE;
					String::copy(Texture::max_name_length, config.diffuse_map_name, TextureSystem::Config::default_diffuse_name);
					system_state->test_material = MaterialSystem::acquire_from_config(config);
				}
			}
			// end

			GeometryRenderData geometry_data = {};
			geometry_data.material = system_state->test_material;

			static float32 angle = 0.01f;
			angle += 0.001f;
			Math::Quat rotation = Math::quat_from_axis_angle(VEC3F_BACK, angle, false);
			geometry_data.model = Math::quat_to_rotation_matrix(rotation, VEC3_ZERO);

			system_state->backend.update_object(geometry_data);

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
	
}