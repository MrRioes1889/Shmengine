#include "RendererFrontend.hpp"
#include "RendererUtils.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "core/FrameData.hpp"
#include "platform/Platform.hpp"
#include "memory/LinearAllocator.hpp"
#include "memory/Freelist.hpp"
#include "utility/math/Transform.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/RenderViewSystem.hpp"

#include "optick.h"

// TODO: temporary
#include "utility/CString.hpp"
#include "core/Event.hpp"
// end

namespace Renderer
{

	typedef bool8(*FP_create_renderer_module)(Renderer::Module* out_module);

	SystemState* system_state = 0;

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		char renderer_module_filename[Constants::max_filepath_length];
		CString::print_s(renderer_module_filename, Constants::max_filepath_length, "%s%s%s", Platform::dynamic_library_prefix, sys_config->renderer_module_name, Platform::dynamic_library_ext);
		if (!Platform::load_dynamic_library(sys_config->renderer_module_name, renderer_module_filename, &system_state->renderer_lib))
			return false;

		FP_create_renderer_module create_renderer_module = 0;
		if (!Platform::load_dynamic_library_function(&system_state->renderer_lib, "create_module", (void**)&create_renderer_module))
			return false;

		if (!create_renderer_module(&system_state->module))
			return false;
		system_state->flags = sys_config->flags;

		system_state->flags = RendererConfigFlags::VSYNC; //| RendererConfigFlags::POWER_SAVING;

		system_state->frame_number = 0;
		system_state->module.frame_number = 0;

		system_state->framebuffer_width = 1600;
		system_state->framebuffer_height = 900;
		system_state->resizing = false;
		system_state->frames_since_resize = 0;

		system_state->max_shader_global_textures = sys_config->max_shader_global_textures;
		system_state->max_shader_instance_textures = sys_config->max_shader_instance_textures;
		system_state->max_shader_uniform_count = sys_config->max_shader_uniform_count;

		uint64 context_size_req = system_state->module.get_context_size_requirement();
		system_state->module_context = Memory::allocate(context_size_req, AllocationTag::Renderer);

		ModuleConfig backend_config = {};
		backend_config.application_name = sys_config->application_name;

		if (!system_state->module.init(system_state->module_context, backend_config, &system_state->device_properties))
		{
			SHMERROR("Failed to initialize renderer backend!");
			return false;
		}

		uint64 vertex_buffer_size = mebibytes(64);
		if (!renderbuffer_init("s_general_vertex_buffer", RenderBufferType::VERTEX, vertex_buffer_size, true, &system_state->general_vertex_buffer))
		{
			SHMERROR("Error creating vertex buffer");
			return false;
		}
		renderbuffer_bind(&system_state->general_vertex_buffer, 0);
		renderbuffer_map_memory(&system_state->general_vertex_buffer, 0, system_state->general_vertex_buffer.size);

		uint64 index_buffer_size = mebibytes(8);
		if (!renderbuffer_init("s_general_index_buffer", RenderBufferType::INDEX, index_buffer_size, true, &system_state->general_index_buffer))
		{
			SHMERROR("Error creating index buffer");
			return false;
		}
		renderbuffer_bind(&system_state->general_index_buffer, 0);
		renderbuffer_map_memory(&system_state->general_index_buffer, 0, system_state->general_index_buffer.size);

		return true;
	}

	void system_shutdown(void* state)
	{
		if (!system_state)
			return;

		renderbuffer_destroy(&system_state->general_vertex_buffer);
		renderbuffer_destroy(&system_state->general_index_buffer);

		system_state->module.shutdown();
		if (system_state->module_context)
			Memory::free_memory(system_state->module_context);

		Platform::unload_dynamic_library(&system_state->renderer_lib);
		system_state = 0;
	}

	bool8 flags_enabled(RendererConfigFlags::Value flags)
	{
		return (flags & system_state->flags);
	}

	void set_flags(RendererConfigFlags::Value flags, bool8 enabled)
	{
		system_state->flags = (enabled ? system_state->flags | flags : system_state->flags & ~flags);
		system_state->module.on_config_changed();
	}

	bool8 draw_frame(FrameData* frame_data)
	{
		OPTICK_EVENT();
		Module& backend = system_state->module;
		backend.frame_number++;
		bool8 did_resize = false;

		if (system_state->resizing) {
			system_state->frames_since_resize++;

			// If the required number of frames have passed since the resize, go ahead and perform the actual updates.
			if (system_state->frames_since_resize >= 30) {
				uint32 width = system_state->framebuffer_width;
				uint32 height = system_state->framebuffer_height;
				RenderViewSystem::on_window_resize(width, height);
				system_state->module.on_resized(width, height);
				system_state->frames_since_resize = 0;
				system_state->resizing = false;
				did_resize = true;
			}
			else {
				// Skip rendering the frame and try again next time.
				return true;
			}
		}

		system_state->frame_number++;

		if (!backend.begin_frame(frame_data))
		{
			if (!did_resize)
				SHMERROR("draw_frame - Failed to begin frame;");
			return false;
		}

		uint32 render_target_index = system_state->module.get_window_attachment_index();

		RenderViewSystem::on_render(frame_data, system_state->module.frame_number, render_target_index);

		if (!backend.end_frame(frame_data))
		{
			SHMERROR("draw_frame - Failed to end frame;");
			return false;
		}

		RenderViewSystem::on_end_frame();

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
		system_state->module.on_resized(width, height);
		
	}

	bool8 render_target_create(uint32 attachment_count, const RenderTargetAttachment* attachments, RenderPass* pass, uint32 width, uint32 height, RenderTarget* out_target)
	{
		return system_state->module.render_target_init(attachment_count, attachments, pass, width, height, out_target);
	}

	void render_target_destroy(RenderTarget* target, bool8 free_internal_memory)
	{
		system_state->module.render_target_destroy(target, free_internal_memory);
	}

	void set_viewport(Math::Vec4f rect)
	{
		system_state->module.set_viewport(rect);
	}

	void reset_viewport()
	{
		system_state->module.reset_viewport();
	}

	void set_scissor(Math::Rect2Di rect)
	{
		system_state->module.set_scissor(rect);
	}

	void reset_scissor()
	{
		system_state->module.reset_scissor();
	}

	Texture* get_window_attachment(uint32 index)
	{
		return system_state->module.get_window_attachment(index);
	}

	Texture* get_depth_attachment(uint32 attachment_index)
	{
		return system_state->module.get_depth_attachment(attachment_index);
	}

	uint32 get_window_attachment_index()
	{
		return system_state->module.get_window_attachment_index();
	}

	uint32 get_window_attachment_count()
	{
		return system_state->module.get_window_attachment_count();
	}

	bool8 renderpass_init(const RenderPassConfig* config, RenderPass* out_renderpass)
	{

		if (config->render_target_count <= 0)
		{
			SHMERROR("Failed to create renderpass. Target count has to be > 0.");
			return false;
		}

		out_renderpass->name = config->name;
		out_renderpass->render_targets.init(config->render_target_count, 0, AllocationTag::Renderer);
		out_renderpass->clear_flags = config->clear_flags;
		out_renderpass->clear_color = config->clear_color;
		out_renderpass->dim = config->dim;

		for (uint32 t = 0; t < out_renderpass->render_targets.capacity; t++)
		{
			RenderTarget* target = &out_renderpass->render_targets[t];
			target->attachments.init(config->target_config.attachment_count, 0, AllocationTag::Renderer);

			for (uint32 a = 0; a < target->attachments.capacity; a++)
			{
				RenderTargetAttachment* att = &target->attachments[a];
				const RenderTargetAttachmentConfig* att_config = &config->target_config.attachment_configs[a];

				att->source = att_config->source;
				att->type = att_config->type;
				att->load_op = att_config->load_op;
				att->store_op = att_config->store_op;
				att->texture = 0;
			}
		}

		return system_state->module.renderpass_init(config, out_renderpass);

	}

	void renderpass_destroy(RenderPass* pass)
	{
		system_state->module.renderpass_destroy(pass);

		for (uint32 i = 0; i < pass->render_targets.capacity; i++)
			render_target_destroy(&pass->render_targets[i], true);
		pass->render_targets.free_data();
		pass->name.free_data();
	}

	bool8 renderpass_begin(RenderPass* pass, RenderTarget* target)
	{
		return system_state->module.renderpass_begin(pass, target);
	}

	bool8 renderpass_end(RenderPass* pass)
	{
		return system_state->module.renderpass_end(pass);
	}

	bool8 texture_init(Texture* texture)
	{
		 return system_state->module.texture_init(texture);
	}

	void texture_resize(Texture* texture, uint32 width, uint32 height)
	{
		system_state->module.texture_resize(texture, width, height);
	}

	bool8 texture_write_data(Texture* t, uint32 offset, uint32 size, const uint8* pixels)
	{
		return system_state->module.texture_write_data(t, offset, size, pixels);
	}

	bool8 texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory)
	{
		return system_state->module.texture_read_data(t, offset, size, out_memory);
	}

	bool8 texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba)
	{
		return system_state->module.texture_read_pixel(t, x, y, out_rgba);
	}

	void texture_destroy(Texture* texture)
	{
		system_state->module.texture_destroy(texture);
		texture->internal_data.free_data();
	}

	bool8 geometry_load(GeometryData* geometry)
	{	
		bool8 is_reload = geometry->loaded;

		uint64 vertex_buffer_size = geometry->vertex_count * (uint64)geometry->vertex_size;
		uint64 index_buffer_size = geometry->index_count * sizeof(geometry->indices[0]);

		if (!vertex_buffer_size)
			return false;

		if (!(is_reload ?
			renderbuffer_reallocate(&system_state->general_vertex_buffer, vertex_buffer_size, &geometry->vertex_buffer_alloc_ref) :
			renderbuffer_allocate(&system_state->general_vertex_buffer, vertex_buffer_size, &geometry->vertex_buffer_alloc_ref))
			)
		{
			SHMERROR("Failed to allocate memory from vertex buffer.");
			return false;
		}

		if (!renderbuffer_load_range(&system_state->general_vertex_buffer, geometry->vertex_buffer_alloc_ref.byte_offset, vertex_buffer_size, geometry->vertices.data))
		{
			SHMERROR("Failed to load data into vertex buffer.");
			return false;
		}

		if (index_buffer_size)
		{
			if (!(is_reload ?
				renderbuffer_reallocate(&system_state->general_index_buffer, index_buffer_size, &geometry->index_buffer_alloc_ref) :
				renderbuffer_allocate(&system_state->general_index_buffer, index_buffer_size, &geometry->index_buffer_alloc_ref))
				)
			{
				SHMERROR("Failed to allocate memory from index buffer.");
				return false;
			}

			if (!renderbuffer_load_range(&system_state->general_index_buffer, geometry->index_buffer_alloc_ref.byte_offset, index_buffer_size, geometry->indices.data))
			{
				SHMERROR("Failed to load data into index buffer.");
				return false;
			}
		}

		geometry->loaded = true;

		return true;
	}

	void geometry_unload(GeometryData* geometry)
	{
		if (!geometry->loaded)
			return;

		system_state->module.device_sleep_till_idle();

		renderbuffer_free(&system_state->general_vertex_buffer, &geometry->vertex_buffer_alloc_ref);
		if (geometry->indices.data)
			renderbuffer_free(&system_state->general_index_buffer, &geometry->index_buffer_alloc_ref);

		geometry->loaded = false;
	}

	void geometry_draw(GeometryData* geometry)
	{
		OPTICK_EVENT();
		if (!geometry->loaded)
			geometry_load(geometry);

		bool8 includes_indices = geometry->index_count > 0;

		renderbuffer_draw(&system_state->general_vertex_buffer, geometry->vertex_buffer_alloc_ref.byte_offset, geometry->vertex_count, includes_indices);
		if (includes_indices)
			renderbuffer_draw(&system_state->general_index_buffer, geometry->index_buffer_alloc_ref.byte_offset, geometry->index_count, false);
	}

	bool8 texture_map_init(TextureMapConfig* config, TextureMap* out_map)
	{
        out_map->filter_minify = config->filter_minify;
        out_map->filter_magnify = config->filter_magnify;
        out_map->repeat_u = config->repeat_u;
        out_map->repeat_v = config->repeat_v;
        out_map->repeat_w = config->repeat_w;

		return system_state->module.texture_map_init(out_map);
	}

	void texture_map_destroy(TextureMap* map)
	{
		return system_state->module.texture_map_destroy(map);
	}

	bool8 renderbuffer_init(const char* name, RenderBufferType type, uint64 size, bool8 use_freelist, RenderBuffer* out_buffer)
	{
		SHMASSERT_MSG(size < Constants::max_u32, "Renderer types use offsets and sizes with 4 byte integers. Change in typedef if needed.");
		out_buffer->name = name;
		out_buffer->size = size;
		out_buffer->type = type;
		out_buffer->mapped_memory = 0;
		out_buffer->has_freelist = use_freelist;

		if (out_buffer->has_freelist)
		{
			AllocatorPageSize freelist_page_size = AllocatorPageSize::TINY;
			uint32 freelist_nodes_count = Freelist::get_max_node_count_by_data_size(out_buffer->size, freelist_page_size);
			if (freelist_nodes_count > 10000)
				freelist_nodes_count = 10000;

			uint64 freelist_nodes_size = Freelist::get_required_nodes_array_memory_size_by_node_count(freelist_nodes_count);
			out_buffer->freelist_data.init(freelist_nodes_size, 0, AllocationTag::Renderer);
			out_buffer->freelist.init(size, out_buffer->freelist_data.data, freelist_page_size, freelist_nodes_count);
		}

		if (!system_state->module.renderbuffer_init(out_buffer))
		{
			SHMFATAL("Failed to create backend part of renderbuffer!");
			renderbuffer_destroy(out_buffer);
			return false;
		}

		return true;
	}

	void renderbuffer_destroy(RenderBuffer* buffer)
	{	
		renderbuffer_unmap_memory(buffer);
		buffer->freelist.destroy();
		buffer->freelist_data.free_data();
		system_state->module.renderbuffer_destroy(buffer);
		buffer->name.free_data();
	}

	bool8 renderbuffer_resize(RenderBuffer* buffer, uint64 new_total_size)
	{
		if (new_total_size <= buffer->size) 
		{
			SHMERROR("renderer_renderbuffer_resize - New size has to be larger than current one.");
			return false;
		}

		if (!system_state->module.renderbuffer_resize(buffer, new_total_size))
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

	bool8 renderbuffer_allocate(RenderBuffer* buffer, uint64 size, RenderBufferAllocationReference* alloc)
	{
		if (!buffer->has_freelist)
		{
			SHMERROR("renderbuffer_allocate - Cannot allocate for a buffer without attached freelist!");
			return false;
		}

		AllocationReference alloc64;
		if (!buffer->freelist.allocate(size, &alloc64))
			return false;

		*alloc = alloc64;
		return true;
	}

	bool8 renderbuffer_reallocate(RenderBuffer* buffer, uint64 new_size, RenderBufferAllocationReference* alloc)
	{
		if (!buffer->has_freelist)
		{
			SHMERROR("renderbuffer_allocate - Cannot allocate for a buffer without attached freelist!");
			return false;
		}

		buffer->freelist.free(alloc->byte_offset);
		AllocationReference alloc64;
		if (!buffer->freelist.allocate(new_size, &alloc64))
			return false;

		*alloc = alloc64;
		return true;
	}

	void renderbuffer_free(RenderBuffer* buffer, RenderBufferAllocationReference* alloc)
	{
		if (!buffer->has_freelist)
		{
			SHMERROR("renderbuffer_free - Cannot free data for a buffer without attached freelist!");
			return;
		}

		buffer->freelist.free(alloc->byte_offset);
		alloc->byte_size = 0;
		alloc->byte_offset = 0;
	}

	bool8 renderbuffer_bind(RenderBuffer* buffer, uint64 offset)
	{
		return system_state->module.renderbuffer_bind(buffer, offset);
	}

	bool8 renderbuffer_unbind(RenderBuffer* buffer)
	{
		return system_state->module.renderbuffer_unbind(buffer);
	}

	void renderbuffer_map_memory(RenderBuffer* buffer, uint64 offset, uint64 size)
	{		
		buffer->mapped_memory = system_state->module.renderbuffer_map_memory(buffer, offset, size);
	}

	void renderbuffer_unmap_memory(RenderBuffer* buffer)
	{
		system_state->module.renderbuffer_unmap_memory(buffer);
		buffer->mapped_memory = 0;
	}

	bool8 renderbuffer_flush(RenderBuffer* buffer, uint64 offset, uint64 size)
	{
		return system_state->module.renderbuffer_flush(buffer, offset, size);
	}

	bool8 renderbuffer_read(RenderBuffer* buffer, uint64 offset, uint64 size, void* out_memory)
	{
		if (buffer->mapped_memory)
		{
			uint8* ptr = (uint8*)buffer->mapped_memory + offset;
			Memory::copy_memory(ptr, out_memory, size);
			return true;
		}

		return system_state->module.renderbuffer_read(buffer, offset, size, out_memory);
	}

	bool8 renderbuffer_load_range(RenderBuffer* buffer, uint64 offset, uint64 size, const void* data)
	{
		if (buffer->mapped_memory)
		{					
			uint8* ptr = (uint8*)buffer->mapped_memory + offset;
			Memory::copy_memory(data, ptr, size);
			return true;
		}

		return system_state->module.renderbuffer_load_range(buffer, offset, size, data);
	}

	bool8 renderbuffer_copy_range(RenderBuffer* source, uint64 source_offset, RenderBuffer* dest, uint64 dest_offset, uint64 size)
	{
		return system_state->module.renderbuffer_copy_range(source, source_offset, dest, dest_offset, size);
	}

	bool8 renderbuffer_draw(RenderBuffer* buffer, uint64 offset, uint32 element_count, bool8 bind_only)
	{
		return system_state->module.renderbuffer_draw(buffer, offset, element_count, bind_only);
	}

	bool8 is_multithreaded()
	{
		return system_state->module.is_multithreaded();
	}
	
}