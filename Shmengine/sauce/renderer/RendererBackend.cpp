#include "RendererBackend.hpp"

#include "vulkan_renderer/VulkanBackend.hpp"

namespace Renderer
{
	bool32 Renderer::backend_create(BackendType type, Backend* out_backend)
	{

		out_backend->frame_count = 0;

		if (type == VULKAN)
		{
			out_backend->init = Vulkan::init;
			out_backend->shutdown = Vulkan::shutdown;
			out_backend->begin_frame = Vulkan::begin_frame;
			out_backend->end_frame = Vulkan::end_frame;
			out_backend->renderpass_begin = Vulkan::vk_renderpass_begin;
			out_backend->renderpass_end = Vulkan::vk_renderpass_end;
			out_backend->renderpass_get = Vulkan::vk_renderpass_get;
			out_backend->render_target_create = Vulkan::vk_render_target_create;
			out_backend->render_target_destroy = Vulkan::vk_render_target_destroy;
			out_backend->on_resized = Vulkan::on_resized;
			out_backend->geometry_draw = Vulkan::vk_geometry_draw;
			out_backend->texture_create = Vulkan::vk_texture_create;
			out_backend->texture_create_writable = Vulkan::vk_texture_create_writable;
			out_backend->texture_resize = Vulkan::vk_texture_resize;
			out_backend->texture_write_data = Vulkan::vk_texture_write_data;
			out_backend->texture_destroy = Vulkan::vk_texture_destroy;
			out_backend->geometry_create = Vulkan::vk_geometry_create;
			out_backend->geometry_destroy = Vulkan::vk_geometry_destroy;

			out_backend->shader_create = Vulkan::vk_shader_create;
			out_backend->shader_destroy = Vulkan::vk_shader_destroy;
			out_backend->shader_set_uniform = Vulkan::vk_shader_set_uniform;
			out_backend->shader_init = Vulkan::vk_shader_init;
			out_backend->shader_use = Vulkan::vk_shader_use;
			out_backend->shader_bind_globals = Vulkan::vk_shader_bind_globals;
			out_backend->shader_bind_instance = Vulkan::vk_shader_bind_instance;

			out_backend->shader_apply_globals = Vulkan::vk_shader_apply_globals;
			out_backend->shader_apply_instance = Vulkan::vk_shader_apply_instance;
			out_backend->shader_acquire_instance_resources = Vulkan::vk_shader_acquire_instance_resources;
			out_backend->shader_release_instance_resources = Vulkan::vk_shader_release_instance_resources;

			out_backend->texture_map_acquire_resources = Vulkan::vk_texture_map_acquire_resources;
			out_backend->texture_map_release_resources = Vulkan::vk_texture_map_release_resources;

			out_backend->renderbuffer_create_internal = Vulkan::vk_buffer_create;
			out_backend->renderbuffer_destroy_internal = Vulkan::vk_buffer_destroy;
			out_backend->renderbuffer_bind = Vulkan::vk_buffer_bind;
			out_backend->renderbuffer_unbind = Vulkan::vk_buffer_unbind;
			out_backend->renderbuffer_map_memory = Vulkan::vk_buffer_map_memory;
			out_backend->renderbuffer_unmap_memory = Vulkan::vk_buffer_unmap_memory;
			out_backend->renderbuffer_flush = Vulkan::vk_buffer_flush;
			out_backend->renderbuffer_read = Vulkan::vk_buffer_read;
			out_backend->renderbuffer_resize = Vulkan::vk_buffer_resize;
			out_backend->renderbuffer_load_range = Vulkan::vk_buffer_load_range;
			out_backend->renderbuffer_copy_range = Vulkan::vk_buffer_copy_range;
			out_backend->renderbuffer_draw = Vulkan::vk_buffer_draw;

			out_backend->window_attachment_get = Vulkan::vk_window_attachment_get;
			out_backend->depth_attachment_get = Vulkan::vk_depth_attachment_get;
			out_backend->window_attachment_index_get = Vulkan::vk_window_attachment_index_get;

			out_backend->is_multithreaded = Vulkan::vk_is_multithreaded;

			return true;
		}

		return false;
	}

	void Renderer::backend_destroy(Backend* backend)
	{
		Memory::zero_memory(backend, sizeof(Backend));
	}
}
