#include "RendererFrontend.hpp"
#include "RendererBackend.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "memory/LinearAllocator.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/ShaderSystem.hpp"

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
		uint32 material_shader_id;
		uint32 ui_shader_id;
	};

	static SystemState* system_state;

#define CRITICAL_INIT(op, msg) \
    if (!(op))				   \
	{						   \
        SHMERROR(msg);           \
        return false;          \
    }

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, const char* application_name)
	{

		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		backend_create(VULKAN, &system_state->backend);
		system_state->backend.frame_count = 0;

		CRITICAL_INIT(system_state->backend.init(application_name), "ERROR: Failed to initialize renderer backend.");

		// Shaders
		Resource config_resource;
		ShaderConfig* config = 0;

		// Builtin material shader.
		CRITICAL_INIT(ResourceSystem::load(Renderer::RendererConfig::builtin_shader_name_material, ResourceType::SHADER, &config_resource), "Failed to load builtin material shader.");
		config = (ShaderConfig*)config_resource.data;
		CRITICAL_INIT(ShaderSystem::create_shader(*config), "Failed to load builtin material shader.");
		ResourceSystem::unload(&config_resource);
		system_state->material_shader_id = ShaderSystem::get_id(Renderer::RendererConfig::builtin_shader_name_material);

		// Builtin UI shader.
		CRITICAL_INIT(ResourceSystem::load(Renderer::RendererConfig::builtin_shader_name_ui, ResourceType::SHADER, &config_resource), "Failed to load builtin UI shader.");
		config = (ShaderConfig*)config_resource.data;
		CRITICAL_INIT(ShaderSystem::create_shader(*config), "Failed to load builtin ui shader.");
		ResourceSystem::unload(&config_resource);
		system_state->ui_shader_id = ShaderSystem::get_id(Renderer::RendererConfig::builtin_shader_name_ui);

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

	static bool32 draw_using_shader(uint32 shader_id, BuiltinRenderpassType::Value renderpass_type, const Math::Mat4& projection, const Math::Mat4& view, uint32 geometry_count, GeometryRenderData* geometries)
	{

		Backend& backend = system_state->backend;

		if (!backend.begin_renderpass(renderpass_type))
		{
			SHMERROR("draw_frame - BuiltinRenderpass::WORLD failed to begin renderpass!");
			return false;
		}

		if (!ShaderSystem::use_shader(shader_id)) {
			SHMERROR("Failed to use material shader. Render frame failed.");
			return false;
		}

		// Apply globals
		if (!MaterialSystem::apply_globals(shader_id, projection, view)) {
			SHMERROR("Failed to use apply globals for material shader. Render frame failed.");
			return false;
		}

		for (uint32 i = 0; i < geometry_count; i++)
		{
			Material* m = 0;
			if (geometries[i].geometry->material) {
				m = geometries[i].geometry->material;
			}
			else {
				m = MaterialSystem::get_default_material();
			}

			// Apply the material
			if (!MaterialSystem::apply_instance(m)) {
				SHMWARNV("Failed to apply material '%s'. Skipping draw.", m->name);
				continue;
			}

			// Apply the locals
			MaterialSystem::apply_local(m, geometries[i].model);

			// Draw it.
			backend.draw_geometry(geometries[i]);
		}

		if (!backend.end_renderpass(BuiltinRenderpassType::WORLD))
		{
			SHMERROR("draw_frame - BuiltinRenderpass::WORLD failed to end renderpass!");
			return false;
		}

		return true;

	}

	bool32 draw_frame(RenderData* data)
	{
		Backend& backend = system_state->backend;

		if (!backend.begin_frame(data->delta_time))
		{
			SHMERROR("draw_frame - Failed to begin frame;");
			return false;
		}

		if (!draw_using_shader(
			system_state->material_shader_id,
			BuiltinRenderpassType::WORLD,
			system_state->world_projection,
			system_state->world_view,
			data->world_geometry_count,
			data->world_geometries))
		{
			SHMERROR("draw_frame - Failed to draw using material shader;");
			return false;
		}

		if (!draw_using_shader(
			system_state->ui_shader_id,
			BuiltinRenderpassType::UI,
			system_state->ui_projection,
			system_state->ui_view,
			data->ui_geometry_count,
			data->ui_geometries))
		{
			SHMERROR("draw_frame - Failed to draw using material shader;");
			return false;
		}

		if (!backend.end_frame(data->delta_time))
		{
			SHMERROR("draw_frame - Failed to begin frame;");
			return false;
		}


		backend.frame_count++;

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

	bool32 create_geometry(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices)
	{
		return system_state->backend.create_geometry(geometry, vertex_size, vertex_count, vertices, index_count, indices);
	}

	void destroy_geometry(Geometry* geometry)
	{
		system_state->backend.destroy_geometry(geometry);
	}

	bool32 get_renderpass_id(const char* name, uint8* out_renderpass_id) 
	{
		// TODO: HACK: Need dynamic renderpasses instead of hardcoding them.
		if (CString::equal_i("Renderpass.Builtin.World", name)) 
		{
			*out_renderpass_id = BuiltinRenderpassType::WORLD;
			return true;
		}
		else if (CString::equal_i("Renderpass.Builtin.UI", name)) 
		{
			*out_renderpass_id = BuiltinRenderpassType::UI;
			return true;
		}

		SHMERRORV("renderer_renderpass_id: No renderpass named '%s'.", name);
		*out_renderpass_id = INVALID_ID8;
		return false;
	}

	bool32 shader_create(Shader* s, uint8 renderpass_id, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages) 
	{
		return system_state->backend.shader_create(s, renderpass_id, stage_count, stage_filenames, stages);
	}

	void shader_destroy(Shader* s) 
	{
		system_state->backend.shader_destroy(s);
	}

	bool32 shader_init(Shader* s) 
	{
		return system_state->backend.shader_init(s);
	}

	bool32 shader_use(Shader* s) 
	{
		return system_state->backend.shader_use(s);
	}

	bool32 shader_bind_globals(Shader* s) 
	{
		return system_state->backend.shader_bind_globals(s);
	}

	bool32 shader_bind_instance(Shader* s, uint32 instance_id) 
	{
		return system_state->backend.shader_bind_instance(s, instance_id);
	}

	bool32 shader_apply_globals(Shader* s) 
	{
		return system_state->backend.shader_apply_globals(s);
	}

	bool32 shader_apply_instance(Shader* s) 
	{
		return system_state->backend.shader_apply_instance(s);
	}

	bool32 shader_acquire_instance_resources(Shader* s, uint32* out_instance_id) 
	{
		return system_state->backend.shader_acquire_instance_resources(s, out_instance_id);
	}

	bool32 shader_release_instance_resources(Shader* s, uint32 instance_id) 
	{
		return system_state->backend.shader_release_instance_resources(s, instance_id);
	}

	bool32 shader_set_uniform(Shader* s, ShaderUniform* uniform, const void* value) 
	{
		return system_state->backend.shader_set_uniform(s, uniform, value);
	}
	
}