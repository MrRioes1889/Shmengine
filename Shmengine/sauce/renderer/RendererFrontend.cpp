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

#include "optick.h"

// TODO: temporary
#include "utility/CString.hpp"
#include "core/Event.hpp"
// end

namespace Renderer
{
	struct SystemState
	{
		Renderer::Backend backend;

		uint32 framebuffer_width;
		uint32 framebuffer_height;

		uint32 window_render_target_count;

		bool32 resizing;
		uint32 frames_since_resize;
		
	};

	static SystemState* system_state;

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

		if (!system_state->backend.init(config, &system_state->window_render_target_count))
		{
			SHMERROR("Failed to initialize renderer backend!");
			return false;
		}

		return true;
	}

	void system_shutdown()
	{
		if (!system_state)
			return;

		system_state->backend.shutdown();	
		system_state = 0;
	}

	bool32 draw_frame(RenderPacket* data)
	{
		OPTICK_EVENT();
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

		uint32 render_target_index = system_state->backend.get_window_attachment_index();

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

	bool32 render_target_create(uint32 attachment_count, const RenderTargetAttachment* attachments, Renderpass* pass, uint32 width, uint32 height, RenderTarget* out_target)
	{
		return system_state->backend.render_target_create(attachment_count, attachments, pass, width, height, out_target);
	}

	void render_target_destroy(RenderTarget* target, bool32 free_internal_memory)
	{
		system_state->backend.render_target_destroy(target, free_internal_memory);
	}

	void set_viewport(Math::Vec4f rect)
	{
		system_state->backend.set_viewport(rect);
	}

	void reset_viewport()
	{
		system_state->backend.reset_viewport();
	}

	void set_scissor(Math::Rect2Di rect)
	{
		system_state->backend.set_scissor(rect);
	}

	void reset_scissor()
	{
		system_state->backend.reset_scissor();
	}

	Texture* get_window_attachment(uint32 index)
	{
		return system_state->backend.get_window_attachment(index);
	}

	Texture* get_depth_attachment(uint32 attachment_index)
	{
		return system_state->backend.get_depth_attachment(attachment_index);
	}

	uint32 get_window_attachment_index()
	{
		return system_state->backend.get_window_attachment_index();
	}

	uint32 get_window_attachment_count()
	{
		return system_state->backend.get_window_attachment_count();
	}

	bool32 renderpass_create(const RenderpassConfig* config, Renderpass* out_renderpass)
	{

		if (config->render_target_count <= 0)
		{
			SHMERROR("Failed to create renderpass. Target count has to be > 0.");
			return false;
		}

		out_renderpass->render_targets.init(config->render_target_count, 0, AllocationTag::RENDERER);
		out_renderpass->clear_flags = config->clear_flags;
		out_renderpass->clear_color = config->clear_color;
		out_renderpass->dim = config->dim;

		for (uint32 t = 0; t < out_renderpass->render_targets.capacity; t++)
		{
			RenderTarget* target = &out_renderpass->render_targets[t];
			target->attachments.init(config->target_config.attachment_count, 0, AllocationTag::RENDERER);

			for (uint32 a = 0; a < target->attachments.capacity; a++)
			{
				RenderTargetAttachment* att = &target->attachments[a];
				RenderTargetAttachmentConfig* att_config = &config->target_config.attachment_configs[a];

				att->source = att_config->source;
				att->type = att_config->type;
				att->load_op = att_config->load_op;
				att->store_op = att_config->store_op;
				att->texture = 0;
			}
		}

		return system_state->backend.renderpass_create(config, out_renderpass);

	}

	void renderpass_destroy(Renderpass* pass)
	{
		for (uint32 i = 0; i < pass->render_targets.capacity; i++)
			render_target_destroy(&pass->render_targets[i], true);
		pass->render_targets.free_data();

		system_state->backend.renderpass_destroy(pass);
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

	bool32 texture_write_data(Texture* t, uint32 offset, uint32 size, const uint8* pixels)
	{
		return system_state->backend.texture_write_data(t, offset, size, pixels);
	}

	bool32 texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory)
	{
		return system_state->backend.texture_read_data(t, offset, size, out_memory);
	}

	bool32 texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba)
	{
		return system_state->backend.texture_read_pixel(t, x, y, out_rgba);
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

	bool32 shader_create(Shader* shader, const ShaderConfig* config, const Renderpass* renderpass, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages)
	{
		return system_state->backend.shader_create(shader, config, renderpass, stage_count, stage_filenames, stages);
	}

	void shader_destroy(Shader* s) 
	{
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
		renderbuffer_unmap_memory(buffer);
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

	void renderbuffer_unmap_memory(Renderbuffer* buffer)
	{
		system_state->backend.renderbuffer_unmap_memory(buffer);
	}

	bool32 renderbuffer_flush(Renderbuffer* buffer, uint64 offset, uint64 size)
	{
		return system_state->backend.renderbuffer_flush(buffer, offset, size);
	}

	bool32 renderbuffer_read(Renderbuffer* buffer, uint64 offset, uint64 size, void* out_memory)
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
		OPTICK_EVENT();
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
	
}