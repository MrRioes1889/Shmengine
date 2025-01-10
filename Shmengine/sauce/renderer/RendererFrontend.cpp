#include "RendererFrontend.hpp"
#include "RendererUtils.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "core/FrameData.hpp"
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

#include "resources/Mesh.hpp"
#include "resources/Skybox.hpp"
#include "resources/Terrain.hpp"
#include "resources/UIText.hpp"
#include "resources/Terrain.hpp"
#include "resources/Box3D.hpp"
#include "resources/Scene.hpp"

#include "optick.h"

// TODO: temporary
#include "utility/CString.hpp"
#include "core/Event.hpp"
// end

namespace Renderer
{
	struct SystemState
	{
		Renderer::Module module;
		void* module_context;

		RenderBuffer general_vertex_buffer;
		RenderBuffer general_index_buffer;

		uint32 framebuffer_width;
		uint32 framebuffer_height;

		uint32 window_render_target_count;

		bool32 resizing;
		uint32 frames_since_resize;

		RendererConfigFlags::Value flags;
		
	};

	static SystemState* system_state = 0;

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{

		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->flags = sys_config->flags;

		system_state->flags = RendererConfigFlags::VSYNC; //| RendererConfigFlags::POWER_SAVING;

		system_state->module.frame_number = 0;
		system_state->module = sys_config->renderer_module;

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

		uint64 vertex_buffer_size = Mebibytes(64);
		if (!renderbuffer_create("s_general_vertex_buffer", RenderBufferType::VERTEX, vertex_buffer_size, true, &system_state->general_vertex_buffer))
		{
			SHMERROR("Error creating vertex buffer");
			return false;
		}
		renderbuffer_bind(&system_state->general_vertex_buffer, 0);

		uint64 index_buffer_size = Mebibytes(8);
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

	bool32 draw_frame(RenderPacket* data, const FrameData* frame_data)
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

		if (!backend.begin_frame(frame_data))
		{
			if (!did_resize)
				SHMERROR("draw_frame - Failed to begin frame;");
			return false;
		}

		uint32 render_target_index = system_state->module.get_window_attachment_index();

		for (uint32 i = 0; i < data->views.capacity; i++)
		{
			if (!RenderViewSystem::on_render(data->views[i], frame_data->frame_allocator, system_state->module.frame_number, render_target_index))
			{
				SHMERRORV("Error rendering view index: %u", i);
				return false;
			}
		}

		if (!backend.end_frame(frame_data))
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
			target->attachments.init(config->target_config.attachment_configs.capacity, 0, AllocationTag::RENDERER);

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
		shader->bound_instance_id = INVALID_ID;
		shader->renderer_frame_number = INVALID_ID64;

		shader->global_texture_maps.init(1, 0, AllocationTag::RENDERER);
		shader->uniforms.init(1, 0, AllocationTag::RENDERER);
		shader->attributes.init(1, 0, AllocationTag::RENDERER);

		shader->uniform_lookup.init(1024, 0);
		shader->uniform_lookup.floodfill(INVALID_ID16);

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

		return system_state->module.shader_create(shader, config, renderpass);
	}

	void shader_destroy(Shader* s) 
	{
		renderbuffer_destroy(&s->uniform_buffer);
		system_state->module.shader_destroy(s);
	}

	bool32 shader_init(Shader* s) 
	{

		// Make sure the UBO is aligned according to device requirements.
		s->global_ubo_stride = (uint32)get_aligned_pow2(s->global_ubo_size, s->required_ubo_alignment);
		s->ubo_stride = (uint32)get_aligned_pow2(s->ubo_size, s->required_ubo_alignment);

		// Uniform  buffer.
		// TODO: max count should be configurable, or perhaps long term support of buffer resizing.
		uint64 total_buffer_size = s->global_ubo_stride + (s->ubo_stride * RendererConfig::max_material_count);  // global + (locals)
		char u_buffer_name[max_buffer_name_length];
		CString::print_s(u_buffer_name, max_buffer_name_length, "%s%s", s->name.c_str(), "_u_buf");
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
		return system_state->module.shader_bind_instance(s, instance_id);
	}

	bool32 shader_apply_globals(Shader* s) 
	{
		return system_state->module.shader_apply_globals(s);
	}

	bool32 shader_apply_instance(Shader* s, bool32 needs_update) 
	{
		return system_state->module.shader_apply_instance(s, needs_update);
	}

	bool32 shader_acquire_instance_resources(Shader* s, uint32 maps_count, TextureMap** maps, uint32* out_instance_id)
	{
		return system_state->module.shader_acquire_instance_resources(s, maps_count, maps, out_instance_id);
	}

	bool32 shader_release_instance_resources(Shader* s, uint32 instance_id) 
	{
		return system_state->module.shader_release_instance_resources(s, instance_id);
	}

	bool32 shader_set_uniform(Shader* s, ShaderUniform* uniform, const void* value) 
	{
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

	uint32 mesh_draw(Mesh* mesh, RenderView* view, uint32 renderpass_id, uint32 shader_id, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum)
	{
		return meshes_draw(mesh, 1, view, renderpass_id, shader_id, lighting, frame_data, frustum);
	}

	uint32 meshes_draw(Mesh* meshes, uint32 mesh_count, RenderView* view, uint32 renderpass_id, uint32 shader_id, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries = 0;
		packet_data.geometries_count = 0;
		packet_data.lighting = lighting;
		packet_data.renderpass_id = renderpass_id;

		for (uint32 i = 0; i < mesh_count; i++)
		{
			Mesh* m = &meshes[i];
			if (m->generation == INVALID_ID8)
				continue;

			Math::Mat4 model = Math::transform_get_world(m->transform);
			for (uint32 j = 0; j < m->geometries.count; j++)
			{
				MeshGeometry* g = &m->geometries[j];

				bool32 in_frustum = true;
				if (frustum)
				{
					Math::Vec3f extents_max = Math::vec_mul_mat(g->g_data->extents.max, model);
					Math::Vec3f center = Math::vec_mul_mat(g->g_data->center, model);
					Math::Vec3f half_extents = { Math::abs(extents_max.x - center.x), Math::abs(extents_max.y - center.y), Math::abs(extents_max.z - center.z) };

					in_frustum = Math::frustum_intersects_aabb(*frustum, center, half_extents);
				}

				if (in_frustum)
				{
					Renderer::ObjectRenderData* render_data = (Renderer::ObjectRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::ObjectRenderData));
					render_data->model = model;
					render_data->shader_id = shader_id;
					render_data->get_instance_render_data = MaterialSystem::material_get_instance_render_data;
					render_data->render_object = g->material;
					render_data->geometry_data = g->g_data;
					render_data->has_transparency = (g->material->maps[0].texture->flags & TextureFlags::HAS_TRANSPARENCY);
					render_data->unique_id = m->unique_id;

					packet_data.geometries_count++;
					if (!packet_data.geometries)
						packet_data.geometries = render_data;
				}
			}
		}

		if (packet_data.geometries_count)
			RenderViewSystem::build_packet(view, frame_data->frame_allocator, &packet_data);

		return packet_data.geometries_count;
	}

	bool32 skybox_draw(Skybox* skybox, RenderView* view, uint32 renderpass_id, uint32 shader_id, FrameData* frame_data)
	{
		Renderer::ObjectRenderData* render_data = (Renderer::ObjectRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::ObjectRenderData));
		render_data->model = {};
		render_data->shader_id = shader_id;
		render_data->get_instance_render_data = skybox_get_instance_render_data;
		render_data->render_object = skybox;
		render_data->geometry_data = skybox->geometry;
		render_data->has_transparency = 0;
		render_data->unique_id = skybox->unique_id;

		RenderViewPacketData packet_data = {};
		packet_data.geometries = render_data;
		packet_data.geometries_count = 1;
		packet_data.renderpass_id = renderpass_id;

		return RenderViewSystem::build_packet(view, frame_data->frame_allocator, &packet_data);
	}

	uint32 terrain_draw(Terrain* terrain, RenderView* view, uint32 renderpass_id, uint32 shader_id, LightingInfo lighting, FrameData* frame_data)
	{
		return terrains_draw(terrain, 1, view, renderpass_id, shader_id, lighting, frame_data);
	}

	uint32 terrains_draw(Terrain* terrains, uint32 terrains_count, RenderView* view, uint32 renderpass_id, uint32 shader_id, LightingInfo lighting, FrameData* frame_data)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries = 0;
		packet_data.geometries_count = 0;
		packet_data.lighting = lighting;
		packet_data.renderpass_id = renderpass_id;

		for (uint32 i = 0; i < terrains_count; i++)
		{
			Terrain* t = &terrains[i];

			Renderer::ObjectRenderData* render_data = (Renderer::ObjectRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::ObjectRenderData));

			render_data->model = Math::transform_get_world(t->xform);
			render_data->shader_id = shader_id;
			render_data->get_instance_render_data = terrain_get_instance_render_data;
			render_data->render_object = t;
			render_data->geometry_data = &t->geometry;
			render_data->has_transparency = 0;
			render_data->unique_id = t->unique_id;

			packet_data.geometries_count++;
			if (!packet_data.geometries)
				packet_data.geometries = render_data;
		}

		if (packet_data.geometries_count)
			RenderViewSystem::build_packet(view, frame_data->frame_allocator, &packet_data);

		return packet_data.geometries_count;
	}

	bool32 ui_text_draw(UIText* text, RenderView* view, uint32 renderpass_id, uint32 shader_id, FrameData* frame_data)
	{
		Renderer::ObjectRenderData* render_data = (Renderer::ObjectRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::ObjectRenderData));
		render_data->model = Math::transform_get_world(text->transform);
		render_data->shader_id = shader_id;
		render_data->get_instance_render_data = ui_text_get_instance_render_data;
		render_data->render_object = text;
		render_data->geometry_data = &text->geometry;
		render_data->has_transparency = true;
		render_data->unique_id = text->unique_id;

		RenderViewPacketData packet_data = {};
		packet_data.geometries = render_data;
		packet_data.geometries_count = 1;
		packet_data.renderpass_id = renderpass_id;

		return RenderViewSystem::build_packet(view, frame_data->frame_allocator, &packet_data);
	}

	uint32 box3D_draw(Box3D* box, RenderView* view, uint32 renderpass_id, uint32 shader_id, FrameData* frame_data)
	{
		return boxes3D_draw(box, 1, view, renderpass_id, shader_id, frame_data);
	}

	uint32 boxes3D_draw(Box3D* boxes, uint32 boxes_count, RenderView* view, uint32 renderpass_id, uint32 shader_id, FrameData* frame_data)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries = 0;
		packet_data.geometries_count = 0;
		packet_data.renderpass_id = renderpass_id;

		for (uint32 i = 0; i < boxes_count; i++)
		{
			Box3D* box = &boxes[i];

			Renderer::ObjectRenderData* render_data = (Renderer::ObjectRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::ObjectRenderData));

			render_data->model = Math::transform_get_world(box->xform);
			render_data->shader_id = shader_id;
			render_data->get_instance_render_data = 0;
			render_data->render_object = box;
			render_data->geometry_data = &box->geometry;
			render_data->has_transparency = 0;
			render_data->unique_id = box->unique_id;

			packet_data.geometries_count++;
			if (!packet_data.geometries)
				packet_data.geometries = render_data;
		}

		if (packet_data.geometries_count)
			RenderViewSystem::build_packet(view, frame_data->frame_allocator, &packet_data);

		return packet_data.geometries_count;
	}

	bool32 scene_draw(Scene* scene, RenderView* skybox_view, RenderView* world_view, const Math::Frustum* camera_frustum, FrameData* frame_data)
	{

		if (scene->state != SceneState::LOADED)
			return false;

		uint32 skybox_shader_id = ShaderSystem::get_skybox_shader_id();

		if (scene->skybox.state >= SkyboxState::INITIALIZED)
			frame_data->drawn_geometry_count += Renderer::skybox_draw(&scene->skybox, skybox_view, 0, skybox_shader_id, frame_data);

		LightingInfo lighting =
		{
			.dir_light = scene->dir_lights.count > 0 ? &scene->dir_lights[0] : 0,
			.p_lights_count = scene->p_lights.count,
			.p_lights = scene->p_lights.data
		};

		uint32 terrain_shader_id = ShaderSystem::get_terrain_shader_id();
		frame_data->drawn_geometry_count += Renderer::terrains_draw(scene->terrains.data, scene->terrains.count, world_view, 0, terrain_shader_id, lighting, frame_data);

		uint32 material_shader_id = ShaderSystem::get_material_shader_id();
		frame_data->drawn_geometry_count += Renderer::meshes_draw(scene->meshes.data, scene->meshes.count, world_view, 0, material_shader_id, lighting, frame_data, camera_frustum);

		uint32 color3D_shader_id = ShaderSystem::get_color3D_shader_id();
		frame_data->drawn_geometry_count += Renderer::boxes3D_draw(scene->p_light_boxes.data, scene->p_light_boxes.count, world_view, 0, material_shader_id, frame_data);

		return true;
	}

	bool8 is_multithreaded()
	{
		return system_state->module.is_multithreaded();
	}
	
}