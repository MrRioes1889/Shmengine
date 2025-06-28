#include "RendererFrontend.hpp"
#include "RendererUtils.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "core/FrameData.hpp"
#include "platform/Platform.hpp"
#include "memory/LinearAllocator.hpp"
#include "memory/Freelist.hpp"
#include "utility/math/Transform.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/GeometrySystem.hpp"
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
		Platform::DynamicLibrary renderer_lib;
		Renderer::Module module;
		void* module_context;

		RenderBuffer general_vertex_buffer;
		RenderBuffer general_index_buffer;

		uint32 framebuffer_width;
		uint32 framebuffer_height;

		uint32 window_render_target_count;

		bool32 resizing;
		uint32 frames_since_resize;

		uint8 frame_number;

		RendererConfigFlags::Value flags;
		
	};

	typedef bool32(*FP_create_renderer_module)(Renderer::Module* out_module);

	static SystemState* system_state = 0;

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
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

		uint64 context_size_req = system_state->module.get_context_size_requirement();
		system_state->module_context = Memory::allocate(context_size_req, AllocationTag::RENDERER);

		ModuleConfig backend_config = {};
		backend_config.application_name = sys_config->application_name;

		if (!system_state->module.init(system_state->module_context, backend_config, &system_state->window_render_target_count))
		{
			SHMERROR("Failed to initialize renderer backend!");
			return false;
		}

		uint64 vertex_buffer_size = mebibytes(64);
		if (!renderbuffer_create("s_general_vertex_buffer", RenderBufferType::VERTEX, vertex_buffer_size, true, &system_state->general_vertex_buffer))
		{
			SHMERROR("Error creating vertex buffer");
			return false;
		}
		renderbuffer_bind(&system_state->general_vertex_buffer, 0);

		uint64 index_buffer_size = mebibytes(8);
		if (!renderbuffer_create("s_general_index_buffer", RenderBufferType::INDEX, index_buffer_size, true, &system_state->general_index_buffer))
		{
			SHMERROR("Error creating index buffer");
			return false;
		}
		renderbuffer_bind(&system_state->general_index_buffer, 0);

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

	bool32 flags_enabled(RendererConfigFlags::Value flags)
	{
		return (flags & system_state->flags);
	}

	void set_flags(RendererConfigFlags::Value flags, bool32 enabled)
	{
		system_state->flags = (enabled ? system_state->flags | flags : system_state->flags & ~flags);
		system_state->module.on_config_changed();
	}

	bool32 draw_frame(FrameData* frame_data)
	{
		OPTICK_EVENT();
		Module& backend = system_state->module;
		backend.frame_number++;
		bool32 did_resize = false;

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

	bool32 render_target_create(uint32 attachment_count, const RenderTargetAttachment* attachments, RenderPass* pass, uint32 width, uint32 height, RenderTarget* out_target)
	{
		return system_state->module.render_target_create(attachment_count, attachments, pass, width, height, out_target);
	}

	void render_target_destroy(RenderTarget* target, bool32 free_internal_memory)
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

	bool32 renderpass_create(const RenderPassConfig* config, RenderPass* out_renderpass)
	{

		if (config->render_target_count <= 0)
		{
			SHMERROR("Failed to create renderpass. Target count has to be > 0.");
			return false;
		}

		out_renderpass->name = config->name;
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
				const RenderTargetAttachmentConfig* att_config = &config->target_config.attachment_configs[a];

				att->source = att_config->source;
				att->type = att_config->type;
				att->load_op = att_config->load_op;
				att->store_op = att_config->store_op;
				att->texture = 0;
			}
		}

		return system_state->module.renderpass_create(config, out_renderpass);

	}

	void renderpass_destroy(RenderPass* pass)
	{
		system_state->module.renderpass_destroy(pass);

		for (uint32 i = 0; i < pass->render_targets.capacity; i++)
			render_target_destroy(&pass->render_targets[i], true);
		pass->render_targets.free_data();
		pass->name.free_data();
	}

	bool32 renderpass_begin(RenderPass* pass, RenderTarget* target)
	{
		return system_state->module.renderpass_begin(pass, target);
	}

	bool32 renderpass_end(RenderPass* pass)
	{
		return system_state->module.renderpass_end(pass);
	}

	void texture_create(const void* pixels, Texture* texture)
	{
		system_state->module.texture_create(pixels, texture);
	}

	void texture_create_writable(Texture* texture)
	{
		system_state->module.texture_create_writable(texture);
	}

	void texture_resize(Texture* texture, uint32 width, uint32 height)
	{
		system_state->module.texture_resize(texture, width, height);
	}

	bool32 texture_write_data(Texture* t, uint32 offset, uint32 size, const uint8* pixels)
	{
		return system_state->module.texture_write_data(t, offset, size, pixels);
	}

	bool32 texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory)
	{
		return system_state->module.texture_read_data(t, offset, size, out_memory);
	}

	bool32 texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba)
	{
		return system_state->module.texture_read_pixel(t, x, y, out_rgba);
	}

	void texture_destroy(Texture* texture)
	{
		system_state->module.texture_destroy(texture);
		texture->internal_data.free_data();
	}

	bool32 geometry_load(GeometryData* geometry)
	{	

		bool32 is_reload = geometry->loaded;

		uint64 vertex_buffer_size = geometry->vertex_count * (uint64)geometry->vertex_size;
		uint64 index_buffer_size = geometry->index_count * sizeof(geometry->indices[0]);

		if (!is_reload)
		{		

			if (!renderbuffer_allocate(&system_state->general_vertex_buffer, vertex_buffer_size, &geometry->vertex_buffer_offset))
			{
				SHMERROR("Failed to allocate memory from vertex buffer.");
				return false;
			}

			if (!renderbuffer_load_range(&system_state->general_vertex_buffer, geometry->vertex_buffer_offset, vertex_buffer_size, geometry->vertices.data))
			{
				SHMERROR("Failed to load data into vertex buffer.");
				return false;
			}

			if (index_buffer_size)
			{
				if (!renderbuffer_allocate(&system_state->general_index_buffer, index_buffer_size, &geometry->index_buffer_offset))
				{
					SHMERROR("Failed to allocate memory from index buffer.");
					return false;
				}

				if (!renderbuffer_load_range(&system_state->general_index_buffer, geometry->index_buffer_offset, index_buffer_size, geometry->indices.data))
				{
					SHMERROR("Failed to load data into index buffer.");
					return false;
				}
			}

		}
		else
		{
			return geometry_reload(geometry, 0, 0);
		}	

		geometry->loaded = true;

		return true;

	}

	bool32 geometry_reload(GeometryData* geometry, uint64 old_vertex_buffer_size, uint64 old_index_buffer_size)
	{
		
		bool32 is_reload = geometry->loaded;

		uint64 new_vertex_buffer_size = geometry->vertex_count * (uint64)geometry->vertex_size;
		uint64 new_index_buffer_size = geometry->index_count * sizeof(geometry->indices[0]);

		if (is_reload)
		{

			if (new_vertex_buffer_size > old_vertex_buffer_size)
			{
				if (!renderbuffer_reallocate(&system_state->general_vertex_buffer, new_vertex_buffer_size, geometry->vertex_buffer_offset, &geometry->vertex_buffer_offset))
				{
					SHMERROR("Failed to reallocate memory from vertex buffer.");
					return false;
				}
			}

			if (!renderbuffer_load_range(&system_state->general_vertex_buffer, geometry->vertex_buffer_offset, new_vertex_buffer_size, geometry->vertices.data))
			{
				SHMERROR("Failed to load data into vertex buffer.");
				return false;
			}

			if (new_index_buffer_size)
			{
				if (new_index_buffer_size > old_index_buffer_size)
				{
					if (!renderbuffer_reallocate(&system_state->general_index_buffer, new_index_buffer_size, geometry->index_buffer_offset, &geometry->index_buffer_offset))
					{
						SHMERROR("Failed to allocate memory from index buffer.");
						return false;
					}
				}		

				if (!renderbuffer_load_range(&system_state->general_index_buffer, geometry->index_buffer_offset, new_index_buffer_size, geometry->indices.data))
				{
					SHMERROR("Failed to load data into index buffer.");
					return false;
				}
			}

		}
		else
		{
			return geometry_load(geometry);
		}

		return true;

	}


	void geometry_unload(GeometryData* geometry)
	{

		system_state->module.device_sleep_till_idle();

		renderbuffer_free(&system_state->general_vertex_buffer, geometry->vertex_buffer_offset);
		if (geometry->indices.data)
			renderbuffer_free(&system_state->general_index_buffer, geometry->index_buffer_offset);

		geometry->loaded = false;

	}

	void geometry_draw(GeometryData* geometry)
	{
		OPTICK_EVENT();
		if (!geometry->loaded)
			geometry_load(geometry);

		bool32 includes_indices = geometry->index_count > 0;

		renderbuffer_draw(&system_state->general_vertex_buffer, geometry->vertex_buffer_offset, geometry->vertex_count, includes_indices);
		if (includes_indices)
			renderbuffer_draw(&system_state->general_index_buffer, geometry->index_buffer_offset, geometry->index_count, false);
	}

	bool32 shader_create(Shader* shader, const ShaderConfig* config, const RenderPass* renderpass)
	{

		shader->name = config->name;
		shader->bound_instance_id = Constants::max_u32;
		shader->last_update_frame_number = Constants::max_u8;

		shader->global_texture_maps.init(1, 0, AllocationTag::RENDERER);
		shader->uniforms.init(1, 0, AllocationTag::RENDERER);
		shader->attributes.init(1, 0, AllocationTag::RENDERER);

		shader->uniform_lookup.init(1024, 0);
		shader->uniform_lookup.floodfill(Constants::max_u16);

		shader->global_ubo_size = 0;
		shader->ubo_size = 0;

		shader->push_constant_stride = 128;
		shader->push_constant_size = 0;

		shader->topologies = config->topologies;
		shader->shader_flags = 0;
		if (config->depth_test)
			shader->shader_flags |= ShaderFlags::DEPTH_TEST;
		if (config->depth_write)
			shader->shader_flags |= ShaderFlags::DEPTH_WRITE;

		for (uint32 i = 0; i < RendererConfig::shader_max_instances; ++i)
			shader->instances[i].offset = Constants::max_u64;

		shader->global_uniform_count = 0;
		shader->global_uniform_sampler_count = 0;
		shader->instance_uniform_count = 0;
		shader->instance_uniform_sampler_count = 0;
		shader->local_uniform_count = 0;

		for (uint32 i = 0; i < config->uniforms_count; i++)
		{
			switch (config->uniforms[i].scope)
			{
			case ShaderScope::GLOBAL:
			{
				if (config->uniforms[i].type == ShaderUniformType::SAMPLER)
					shader->global_uniform_sampler_count++;
				else
					shader->global_uniform_count++;
				break;
			}
			case ShaderScope::INSTANCE:
			{
				if (config->uniforms[i].type == ShaderUniformType::SAMPLER)
					shader->instance_uniform_sampler_count++;
				else
					shader->instance_uniform_count++;
				break;
			}
			case ShaderScope::LOCAL:
			{
				shader->local_uniform_count++;
				break;
			}
			}
		}

		return system_state->module.shader_create(shader, config, renderpass);
	}

	void shader_destroy(Shader* s) 
	{
		renderbuffer_destroy(&s->uniform_buffer);
		system_state->module.shader_destroy(s);
	}

	bool32 shader_init(Shader* s) 
	{

		s->instance_count = 0;

		// Make sure the UBO is aligned according to device requirements.
		s->global_ubo_stride = (uint32)get_aligned_pow2(s->global_ubo_size, s->required_ubo_alignment);
		s->ubo_stride = (uint32)get_aligned_pow2(s->ubo_size, s->required_ubo_alignment);

		// Uniform  buffer.
		// TODO: max count should be configurable, or perhaps long term support of buffer resizing.
		uint64 total_buffer_size = s->global_ubo_stride + (s->ubo_stride * RendererConfig::max_material_count);  // global + (locals)
		char u_buffer_name[Constants::max_buffer_name_length];
		CString::print_s(u_buffer_name, Constants::max_buffer_name_length, "%s%s", s->name.c_str(), "_u_buf");
		if (!renderbuffer_create(u_buffer_name, RenderBufferType::UNIFORM, total_buffer_size, true, &s->uniform_buffer))
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

		bool32 res = system_state->module.shader_init(s);
		if (!res)
			renderbuffer_destroy(&s->uniform_buffer);

		return res;
	}

	bool32 shader_use(Shader* s) 
	{
		return system_state->module.shader_use(s);
	}

	bool32 shader_bind_globals(Shader* s) 
	{
		return system_state->module.shader_bind_globals(s);
	}

	bool32 shader_bind_instance(Shader* s, uint32 instance_id) 
	{
		s->bound_instance_id = instance_id;
		s->bound_ubo_offset = (uint32)s->instances[instance_id].offset;

		return system_state->module.shader_bind_instance(s, instance_id);
	}

	bool32 shader_apply_globals(Shader* s) 
	{
		if (s->last_update_frame_number == system_state->frame_number)
			return true;

		s->last_update_frame_number = system_state->frame_number;
		return system_state->module.shader_apply_globals(s);
	}

	bool32 shader_apply_instance(Shader* s) 
	{
		if (s->instance_uniform_count < 1 && s->instance_uniform_sampler_count < 1)
		{
			SHMERROR("This shader does not use instances.");
			return false;
		}

		ShaderInstance* instance = &s->instances[s->bound_instance_id];
		if (instance->last_update_frame_number == system_state->frame_number)
			return true;

		instance->last_update_frame_number = system_state->frame_number;
		return system_state->module.shader_apply_instance(s);
	}

	bool32 shader_acquire_instance_resources(Shader* s, uint32 texture_maps_count, uint32* out_instance_id)
	{

		// TODO: dynamic
		*out_instance_id = Constants::max_u32;
		for (uint32 i = 0; i < 1024; ++i)
		{
			if (s->instances[i].offset == Constants::max_u64)
			{
				*out_instance_id = i;
				break;
			}
		}
		if (*out_instance_id == Constants::max_u32)
		{
			SHMERROR("vulkan_shader_acquire_instance_resources failed to acquire new id");
			return false;
		}

		s->instance_count++;
		ShaderInstance* instance = &s->instances[*out_instance_id];

		if (texture_maps_count)
			instance->instance_texture_maps.init(texture_maps_count, true, AllocationTag::RENDERER);

		// Allocate some space in the UBO - by the stride, not the size.
		uint64 size = s->ubo_stride;
		if (size > 0)
		{
			if (!renderbuffer_allocate(&s->uniform_buffer, size, &instance->offset))
			{
				SHMERROR("vulkan_material_shader_acquire_resources failed to acquire ubo space");
				return false;
			}
		}

		return system_state->module.shader_acquire_instance_resources(s, texture_maps_count, *out_instance_id);
	}

	bool32 shader_release_instance_resources(Shader* s, uint32 instance_id) 
	{
		ShaderInstance* instance = &s->instances[instance_id];

		instance->instance_texture_maps.free_data();

		renderbuffer_free(&s->uniform_buffer, instance->offset);
		instance->offset = Constants::max_u64;

		s->instance_count--;

		return system_state->module.shader_release_instance_resources(s, instance_id);
	}

	bool32 shader_set_uniform(Shader* s, ShaderUniform* uniform, const void* value) 
	{

		if (uniform->type == ShaderUniformType::SAMPLER)
		{
			if (uniform->scope == ShaderScope::GLOBAL)
				s->global_texture_maps[uniform->location] = (TextureMap*)value;
			else
				s->instances[s->bound_instance_id].instance_texture_maps[uniform->location] = (TextureMap*)value;

			return true;
		}

		return system_state->module.shader_set_uniform(s, uniform, value);	
		
	}

	bool32 texture_map_acquire_resources(TextureMap* out_map)
	{
		return system_state->module.texture_map_acquire_resources(out_map);
	}

	void texture_map_release_resources(TextureMap* out_map)
	{
		return system_state->module.texture_map_release_resources(out_map);
	}

	bool32 renderbuffer_create(const char* name, RenderBufferType type, uint64 size, bool32 use_freelist, RenderBuffer* out_buffer)
	{

		out_buffer->name = name;
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

		if (!system_state->module.renderbuffer_create_internal(out_buffer))
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
		system_state->module.renderbuffer_destroy_internal(buffer);
		buffer->name.free_data();
	}

	bool32 renderbuffer_bind(RenderBuffer* buffer, uint64 offset)
	{
		return system_state->module.renderbuffer_bind(buffer, offset);
	}

	bool32 renderbuffer_unbind(RenderBuffer* buffer)
	{
		return system_state->module.renderbuffer_unbind(buffer);
	}

	void* renderbuffer_map_memory(RenderBuffer* buffer, uint64 offset, uint64 size)
	{		
		return system_state->module.renderbuffer_map_memory(buffer, offset, size);
	}

	void renderbuffer_unmap_memory(RenderBuffer* buffer)
	{
		system_state->module.renderbuffer_unmap_memory(buffer);
	}

	bool32 renderbuffer_flush(RenderBuffer* buffer, uint64 offset, uint64 size)
	{
		return system_state->module.renderbuffer_flush(buffer, offset, size);
	}

	bool32 renderbuffer_read(RenderBuffer* buffer, uint64 offset, uint64 size, void* out_memory)
	{
		return system_state->module.renderbuffer_read(buffer, offset, size, out_memory);
	}

	bool32 renderbuffer_resize(RenderBuffer* buffer, uint64 new_total_size)
	{

		if (new_total_size <= buffer->size) {
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

	bool32 renderbuffer_allocate(RenderBuffer* buffer, uint64 size, uint64* out_offset)
	{
		if (!buffer->has_freelist)
		{
			SHMERROR("renderbuffer_allocate - Cannot allocate for a buffer without attached freelist!");
			return false;
		}

		buffer->freelist.allocate(size, out_offset);
		return true;
	}

	bool32 renderbuffer_reallocate(RenderBuffer* buffer, uint64 new_size, uint64 old_offset, uint64* new_offset)
	{
		if (!buffer->has_freelist)
		{
			SHMERROR("renderbuffer_allocate - Cannot allocate for a buffer without attached freelist!");
			return false;
		}

		buffer->freelist.free(old_offset);
		buffer->freelist.allocate(new_size, new_offset);
		return true;
	}

	void renderbuffer_free(RenderBuffer* buffer, uint64 offset)
	{
		if (!buffer->has_freelist)
		{
			SHMERROR("renderbuffer_free - Cannot free data for a buffer without attached freelist!");
			return;
		}

		buffer->freelist.free(offset);
	}

	bool32 renderbuffer_load_range(RenderBuffer* buffer, uint64 offset, uint64 size, const void* data)
	{
		OPTICK_EVENT();
		return system_state->module.renderbuffer_load_range(buffer, offset, size, data);
	}

	bool32 renderbuffer_copy_range(RenderBuffer* source, uint64 source_offset, RenderBuffer* dest, uint64 dest_offset, uint64 size)
	{
		return system_state->module.renderbuffer_copy_range(source, source_offset, dest, dest_offset, size);
	}

	bool32 renderbuffer_draw(RenderBuffer* buffer, uint64 offset, uint32 element_count, bool32 bind_only)
	{
		return system_state->module.renderbuffer_draw(buffer, offset, element_count, bind_only);
	}

	bool8 is_multithreaded()
	{
		return system_state->module.is_multithreaded();
	}
	
}