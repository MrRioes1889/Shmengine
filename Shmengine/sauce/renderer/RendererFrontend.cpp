#include "RendererFrontend.hpp"
#include "RendererBackend.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "memory/LinearAllocator.hpp"
#include "memory/Freelist.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/CameraSystem.hpp"
#include "systems/RenderViewSystem.hpp"

// TODO: temporary
#include "utility/CString.hpp"
#include "core/Event.hpp"
// end

namespace Renderer
{
	struct SystemState
	{
		Renderer::Backend backend;

		uint32 material_shader_id;
		uint32 ui_shader_id;
		uint32 skybox_shader_id;

		uint32 framebuffer_width;
		uint32 framebuffer_height;

		uint32 window_render_target_count;
		Renderpass* world_renderpass;
		Renderpass* ui_renderpass;
		Renderpass* skybox_renderpass;

		bool32 resizing;
		uint32 frames_since_resize;
		
	};

	static SystemState* system_state;

#define CRITICAL_INIT(op, msg) \
    if (!(op))				   \
	{						   \
        SHMERROR(msg);           \
        return false;          \
    }

	static void regenerate_render_targets();

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, const char* application_name)
	{

		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		backend_create(VULKAN, &system_state->backend);
		system_state->backend.frame_count = 0;

		system_state->framebuffer_width = 1280;
		system_state->framebuffer_height = 720;
		system_state->resizing = false;
		system_state->frames_since_resize = 0;

		BackendConfig config = {};
		config.application_name = application_name;
		config.on_render_target_refresh_required = regenerate_render_targets;

		const uint32 pass_config_count = 3;
		config.pass_config_count = pass_config_count;
		const char* world_renderpass_name = "Renderpass.Builtin.World";
		const char* ui_renderpass_name = "Renderpass.Builtin.UI";
		const char* skybox_renderpass_name = "Renderpass.Builtin.Skybox";
		RenderpassConfig pass_configs[pass_config_count];

		pass_configs[0].name = skybox_renderpass_name;
		pass_configs[0].prev_name = 0;
		pass_configs[0].next_name = world_renderpass_name;
		pass_configs[0].dim = { system_state->framebuffer_width, system_state->framebuffer_height };
		pass_configs[0].offset = { 0, 0 };
		pass_configs[0].clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
		pass_configs[0].clear_flags = RenderpassClearFlags::COLOR_BUFFER;

		pass_configs[1].name = world_renderpass_name;
		pass_configs[1].prev_name = skybox_renderpass_name;
		pass_configs[1].next_name = ui_renderpass_name;
		pass_configs[1].dim = { system_state->framebuffer_width, system_state->framebuffer_height };
		pass_configs[1].offset = { 0, 0 };
		pass_configs[1].clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
		pass_configs[1].clear_flags = RenderpassClearFlags::DEPTH_BUFFER | RenderpassClearFlags::STENCIL_BUFFER;

		pass_configs[2].name = ui_renderpass_name;
		pass_configs[2].prev_name = world_renderpass_name;
		pass_configs[2].next_name = 0;
		pass_configs[2].dim = { system_state->framebuffer_width, system_state->framebuffer_height };
		pass_configs[2].offset = { 0, 0 };
		pass_configs[2].clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
		pass_configs[2].clear_flags = RenderpassClearFlags::NONE;

		config.pass_configs = pass_configs;

		CRITICAL_INIT(system_state->backend.init(config, &system_state->window_render_target_count), "ERROR: Failed to initialize renderer backend.");

		system_state->skybox_renderpass = Renderer::renderpass_get(skybox_renderpass_name);
		system_state->skybox_renderpass->render_targets.init(system_state->window_render_target_count, 0);

		system_state->world_renderpass = Renderer::renderpass_get(world_renderpass_name);
		system_state->world_renderpass->render_targets.init(system_state->window_render_target_count, 0);

		system_state->ui_renderpass = Renderer::renderpass_get(ui_renderpass_name);
		system_state->ui_renderpass->render_targets.init(system_state->window_render_target_count, 0);

		regenerate_render_targets();

		system_state->skybox_renderpass->dim = { system_state->framebuffer_width, system_state->framebuffer_height };
		system_state->world_renderpass->dim = { system_state->framebuffer_width, system_state->framebuffer_height };
		system_state->ui_renderpass->dim = { system_state->framebuffer_width, system_state->framebuffer_height };

		// Shaders
		Resource config_resource;
		ShaderConfig* shader_config = 0;

		// Builtin skybox shader.
		CRITICAL_INIT(ResourceSystem::load(Renderer::RendererConfig::builtin_shader_name_skybox, ResourceType::SHADER, 0, &config_resource), "Failed to load builtin skybox shader.");
		shader_config = (ShaderConfig*)config_resource.data;
		CRITICAL_INIT(ShaderSystem::create_shader(*shader_config), "Failed to load builtin skybox shader.");
		ResourceSystem::unload(&config_resource);
		system_state->skybox_shader_id = ShaderSystem::get_id(Renderer::RendererConfig::builtin_shader_name_skybox);

		// Builtin material shader.
		CRITICAL_INIT(ResourceSystem::load(Renderer::RendererConfig::builtin_shader_name_world, ResourceType::SHADER, 0, &config_resource), "Failed to load builtin material shader.");
		shader_config = (ShaderConfig*)config_resource.data;
		CRITICAL_INIT(ShaderSystem::create_shader(*shader_config), "Failed to load builtin material shader.");
		ResourceSystem::unload(&config_resource);
		system_state->material_shader_id = ShaderSystem::get_id(Renderer::RendererConfig::builtin_shader_name_world);

		// Builtin UI shader.
		CRITICAL_INIT(ResourceSystem::load(Renderer::RendererConfig::builtin_shader_name_ui, ResourceType::SHADER, 0, &config_resource), "Failed to load builtin UI shader.");
		shader_config = (ShaderConfig*)config_resource.data;
		CRITICAL_INIT(ShaderSystem::create_shader(*shader_config), "Failed to load builtin ui shader.");
		ResourceSystem::unload(&config_resource);
		system_state->ui_shader_id = ShaderSystem::get_id(Renderer::RendererConfig::builtin_shader_name_ui);

		return true;
	}

	void system_shutdown()
	{
		if (!system_state)
			return;

		for (uint32 i = 0; i < system_state->window_render_target_count; i++)
		{
			system_state->backend.render_target_destroy(&system_state->skybox_renderpass->render_targets[i], true);
			system_state->backend.render_target_destroy(&system_state->world_renderpass->render_targets[i], true);
			system_state->backend.render_target_destroy(&system_state->ui_renderpass->render_targets[i], true);
		}

		system_state->backend.shutdown();	
		system_state = 0;
	}

	bool32 draw_frame(RenderPacket* data)
	{
		Backend& backend = system_state->backend;
		backend.frame_count++;
		bool32 did_resize = false;


		if (system_state->resizing) {
			system_state->frames_since_resize++;

			// If the required number of frames have passed since the resize, go ahead and perform the actual updates.
			if (system_state->frames_since_resize >= 30) {
				uint32 width = system_state->framebuffer_width;
				uint32 height = system_state->framebuffer_height;
				RenderViewSystem::on_window_resize(width, height);
				system_state->backend.on_resized(width, height);			

				system_state->frames_since_resize = 0;
				system_state->resizing = false;
				did_resize = true;
			}
			else {
				// Skip rendering the frame and try again next time.
				return true;
			}
		}

		if (!backend.begin_frame(data->delta_time))
		{
			if (!did_resize)
				SHMERROR("draw_frame - Failed to begin frame;");
			return false;
		}

		uint32 render_target_index = system_state->backend.window_attachment_index_get();

		for (uint32 i = 0; i < data->views.capacity; i++)
		{
			if (!RenderViewSystem::on_render(data->views[i].view, data->views[i], system_state->backend.frame_count, render_target_index))
			{
				SHMERRORV("Error rendering view index: %u", i);
				return false;
			}
		}

		if (!backend.end_frame(data->delta_time))
		{
			SHMERROR("draw_frame - Failed to end frame;");
			return false;
		}

		return true;
	}

	void on_resized(uint32 width, uint32 height)
	{
		if (!system_state)
		{
			SHMWARN("Renderer backend does not exist to accept resize!");
			return;
		}

		system_state->resizing = true;
		system_state->framebuffer_width = width;
		system_state->framebuffer_height = height;
		system_state->backend.on_resized(width, height);
		
	}

	void render_target_create(uint32 attachment_count, Texture* const* attachments, Renderpass* pass, uint32 width, uint32 height, RenderTarget* out_target)
	{
		system_state->backend.render_target_create(attachment_count, attachments, pass, width, height, out_target);
	}

	void render_target_destroy(RenderTarget* target, bool32 free_internal_memory)
	{
		system_state->backend.render_target_destroy(target, free_internal_memory);
	}

	void renderpass_create(Renderpass* out_renderpass, float32 depth, uint32 stencil, bool32 has_prev_pass, bool32 has_next_pass)
	{
		system_state->backend.renderpass_create(out_renderpass, depth, stencil, has_prev_pass, has_next_pass);
	}

	void renderpass_destroy(Renderpass* pass)
	{
		system_state->backend.renderpass_destroy(pass);
	}

	Renderpass* renderpass_get(const char* name)
	{
		return system_state->backend.renderpass_get(name);
	}

	bool32 renderpass_begin(Renderpass* pass, RenderTarget* target)
	{
		return system_state->backend.renderpass_begin(pass, target);
	}

	bool32 renderpass_end(Renderpass* pass)
	{
		return system_state->backend.renderpass_end(pass);
	}

	void texture_create(const void* pixels, Texture* texture)
	{
		system_state->backend.texture_create(pixels, texture);
	}

	void texture_create_writable(Texture* texture)
	{
		system_state->backend.texture_create_writable(texture);
	}

	void texture_resize(Texture* texture, uint32 width, uint32 height)
	{
		system_state->backend.texture_resize(texture, width, height);
	}

	void texture_write_data(Texture* texture, uint32 offset, uint32 size, const uint8* pixels)
	{
		system_state->backend.texture_write_data(texture, offset, size, pixels);
	}

	void texture_destroy(Texture* texture)
	{
		system_state->backend.texture_destroy(texture);
	}

	bool32 geometry_create(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices)
	{
		return system_state->backend.geometry_create(geometry, vertex_size, vertex_count, vertices, index_count, indices);
	}

	void geometry_destroy(Geometry* geometry)
	{
		system_state->backend.geometry_destroy(geometry);
	}

	void geometry_draw(const GeometryRenderData& data)
	{
		system_state->backend.geometry_draw(data);
	}

	bool32 shader_create(Shader* s, const ShaderConfig& config, Renderpass* renderpass, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages)
	{
		return system_state->backend.shader_create(s, config, renderpass, stage_count, stage_filenames, stages);
	}

	void shader_destroy(Shader* s) 
	{
		renderbuffer_unmap_memory(&s->uniform_buffer, 0, s->uniform_buffer.size);
		renderbuffer_destroy(&s->uniform_buffer);
		system_state->backend.shader_destroy(s);
	}

	bool32 shader_init(Shader* s) 
	{

		// Make sure the UBO is aligned according to device requirements.
		s->global_ubo_stride = (uint32)get_aligned_pow2(s->global_ubo_size, s->required_ubo_alignment);
		s->ubo_stride = (uint32)get_aligned_pow2(s->ubo_size, s->required_ubo_alignment);

		// Uniform  buffer.
		// TODO: max count should be configurable, or perhaps long term support of buffer resizing.
		uint64 total_buffer_size = s->global_ubo_stride + (s->ubo_stride * RendererConfig::max_material_count);  // global + (locals)
		if (!renderbuffer_create(RenderbufferType::UNIFORM, total_buffer_size, true, &s->uniform_buffer))
		{
			SHMERROR("Vulkan buffer creation failed for object shader.");
			return false;
		}
		renderbuffer_bind(&s->uniform_buffer, 0);

		// Allocate space for the global UBO, whcih should occupy the _stride_ space, _not_ the actual size used.
		if (!renderbuffer_allocate(&s->uniform_buffer, s->global_ubo_stride, &s->global_ubo_offset))
		{
			renderbuffer_destroy(&s->uniform_buffer);
			SHMERROR("Failed to allocate space for the uniform buffer!");
			return false;
		}

		bool32 res = system_state->backend.shader_init(s);
		if (!res)
			renderbuffer_destroy(&s->uniform_buffer);

		return res;
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

	bool32 shader_apply_instance(Shader* s, bool32 needs_update) 
	{
		return system_state->backend.shader_apply_instance(s, needs_update);
	}

	bool32 shader_acquire_instance_resources(Shader* s, TextureMap** maps, uint32* out_instance_id)
	{
		return system_state->backend.shader_acquire_instance_resources(s, maps, out_instance_id);
	}

	bool32 shader_release_instance_resources(Shader* s, uint32 instance_id) 
	{
		return system_state->backend.shader_release_instance_resources(s, instance_id);
	}

	bool32 shader_set_uniform(Shader* s, ShaderUniform* uniform, const void* value) 
	{
		return system_state->backend.shader_set_uniform(s, uniform, value);
	}

	bool32 texture_map_acquire_resources(TextureMap* out_map)
	{
		return system_state->backend.texture_map_acquire_resources(out_map);
	}

	void texture_map_release_resources(TextureMap* out_map)
	{
		return system_state->backend.texture_map_release_resources(out_map);
	}

	bool32 renderbuffer_create(RenderbufferType type, uint64 size, bool32 use_freelist, Renderbuffer* out_buffer)
	{

		out_buffer->size = size;
		out_buffer->type = type;
		out_buffer->has_freelist = use_freelist;

		if (out_buffer->has_freelist)
		{
			AllocatorPageSize freelist_page_size = AllocatorPageSize::TINY;
			uint32 freelist_nodes_count = Freelist::get_max_node_count_by_data_size(out_buffer->size, freelist_page_size);
			if (freelist_nodes_count > 10000)
				freelist_nodes_count = 10000;

			uint64 freelist_nodes_size = Freelist::get_required_nodes_array_memory_size_by_node_count(freelist_nodes_count);
			out_buffer->freelist_data.init(freelist_nodes_size, 0, AllocationTag::RENDERER);
			out_buffer->freelist.init(size, out_buffer->freelist_data.data, freelist_page_size, freelist_nodes_count);
		}

		if (!system_state->backend.renderbuffer_create_internal(out_buffer))
		{
			SHMFATAL("Failed to create backend part of renderbuffer!");
			renderbuffer_destroy(out_buffer);
			return false;
		}

		return true;
	}

	void renderbuffer_destroy(Renderbuffer* buffer)
	{	
		buffer->freelist.destroy();
		buffer->freelist_data.free_data();
		system_state->backend.renderbuffer_destroy_internal(buffer);
	}

	bool32 renderbuffer_bind(Renderbuffer* buffer, uint64 offset)
	{
		return system_state->backend.renderbuffer_bind(buffer, offset);
	}

	bool32 renderbuffer_unbind(Renderbuffer* buffer)
	{
		return system_state->backend.renderbuffer_unbind(buffer);
	}

	void* renderbuffer_map_memory(Renderbuffer* buffer, uint64 offset, uint64 size)
	{
		return system_state->backend.renderbuffer_map_memory(buffer, offset, size);
	}

	void renderbuffer_unmap_memory(Renderbuffer* buffer, uint64 offset, uint64 size)
	{
		system_state->backend.renderbuffer_unmap_memory(buffer, offset, size);
	}

	bool32 renderbuffer_flush(Renderbuffer* buffer, uint64 offset, uint64 size)
	{
		return system_state->backend.renderbuffer_flush(buffer, offset, size);
	}

	bool32 renderbuffer_read(Renderbuffer* buffer, uint64 offset, uint64 size, void** out_memory)
	{
		return system_state->backend.renderbuffer_read(buffer, offset, size, out_memory);
	}

	bool32 renderbuffer_resize(Renderbuffer* buffer, uint64 new_total_size)
	{

		if (new_total_size <= buffer->size) {
			SHMERROR("renderer_renderbuffer_resize - New size has to be larger than current one.");
			return false;
		}

		if (!system_state->backend.renderbuffer_resize(buffer, new_total_size))
		{
			SHMERROR("renderer_renderbuffer_resize - Failed to resize internal renderbuffer.");
			return false;
		}

		if (buffer->has_freelist)
		{
			AllocatorPageSize freelist_page_size = AllocatorPageSize::TINY;
			uint32 freelist_nodes_count = Freelist::get_max_node_count_by_data_size(new_total_size, freelist_page_size);
			if (freelist_nodes_count > 10000)
				freelist_nodes_count = 10000;

			uint64 freelist_nodes_size = Freelist::get_required_nodes_array_memory_size_by_node_count(freelist_nodes_count);
			if (freelist_nodes_count != buffer->freelist.max_nodes_count)
				buffer->freelist_data.resize(freelist_nodes_count * sizeof(Freelist::Node));

			buffer->freelist.resize(new_total_size, buffer->freelist_data.data, freelist_nodes_count);
		}

		buffer->size = new_total_size;
		return true;

	}

	bool32 renderbuffer_allocate(Renderbuffer* buffer, uint64 size, uint64* out_offset)
	{
		if (!buffer->has_freelist)
		{
			SHMERROR("renderbuffer_allocate - Cannot allocate for a buffer without attached freelist!");
			return false;
		}

		buffer->freelist.allocate(size, out_offset);
		return true;
	}

	void renderbuffer_free(Renderbuffer* buffer, uint64 offset)
	{
		if (!buffer->has_freelist)
		{
			SHMERROR("renderbuffer_free - Cannot free data for a buffer without attached freelist!");
			return;
		}

		buffer->freelist.free(offset);
	}

	bool32 renderbuffer_load_range(Renderbuffer* buffer, uint64 offset, uint64 size, const void* data)
	{
		return system_state->backend.renderbuffer_load_range(buffer, offset, size, data);
	}

	bool32 renderbuffer_copy_range(Renderbuffer* source, uint64 source_offset, Renderbuffer* dest, uint64 dest_offset, uint64 size)
	{
		return system_state->backend.renderbuffer_copy_range(source, source_offset, dest, dest_offset, size);
	}

	bool32 renderbuffer_draw(Renderbuffer* buffer, uint64 offset, uint32 element_count, bool32 bind_only)
	{
		return system_state->backend.renderbuffer_draw(buffer, offset, element_count, bind_only);
	}

	bool8 is_multithreaded()
	{
		return system_state->backend.is_multithreaded();
	}

	static void regenerate_render_targets()
	{
		for (uint32 i = 0; i < system_state->window_render_target_count; i++) 
		{
			// Destroy the old first if they exist.
			system_state->backend.render_target_destroy(&system_state->skybox_renderpass->render_targets[i], false);
			system_state->backend.render_target_destroy(&system_state->world_renderpass->render_targets[i], false);
			system_state->backend.render_target_destroy(&system_state->ui_renderpass->render_targets[i], false);

			Texture* window_target_texture = system_state->backend.window_attachment_get(i);
			Texture* depth_target_texture = system_state->backend.depth_attachment_get();

			// Skybox render targets
			Texture* skybox_attachments[1] = { window_target_texture };
			system_state->backend.render_target_create(
				1,
				skybox_attachments,
				system_state->skybox_renderpass,
				system_state->framebuffer_width,
				system_state->framebuffer_height,
				&system_state->skybox_renderpass->render_targets[i]);

			// World render targets.
			Texture* attachments[2] = { window_target_texture, depth_target_texture };
			system_state->backend.render_target_create(
				2,
				attachments,
				system_state->world_renderpass,
				system_state->framebuffer_width,
				system_state->framebuffer_height,
				&system_state->world_renderpass->render_targets[i]);

			// UI render targets
			Texture* ui_attachments[1] = { window_target_texture };
			system_state->backend.render_target_create(
				1,
				ui_attachments,
				system_state->ui_renderpass,
				system_state->framebuffer_width,
				system_state->framebuffer_height,
				&system_state->ui_renderpass->render_targets[i]);

		}
	}
	
}