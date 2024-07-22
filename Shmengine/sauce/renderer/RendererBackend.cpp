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
			out_backend->renderpass_begin = Vulkan::renderpass_begin;
			out_backend->renderpass_end = Vulkan::renderpass_end;
			out_backend->renderpass_get = Vulkan::renderpass_get;
			out_backend->render_target_create = Vulkan::render_target_create;
			out_backend->render_target_destroy = Vulkan::render_target_destroy;
			out_backend->on_resized = Vulkan::on_resized;
			out_backend->geometry_draw = Vulkan::geometry_draw;
			out_backend->texture_create = Vulkan::texture_create;
			out_backend->texture_create_writable = Vulkan::texture_create_writable;
			out_backend->texture_resize = Vulkan::texture_resize;
			out_backend->texture_write_data = Vulkan::texture_write_data;
			out_backend->texture_destroy = Vulkan::texture_destroy;
			out_backend->geometry_create = Vulkan::geometry_create;
			out_backend->geometry_destroy = Vulkan::geometry_destroy;

			out_backend->shader_create = Vulkan::shader_create;
			out_backend->shader_destroy = Vulkan::shader_destroy;
			out_backend->shader_set_uniform = Vulkan::shader_set_uniform;
			out_backend->shader_init = Vulkan::shader_init;
			out_backend->shader_use = Vulkan::shader_use;
			out_backend->shader_bind_globals = Vulkan::shader_bind_globals;
			out_backend->shader_bind_instance = Vulkan::shader_bind_instance;

			out_backend->shader_apply_globals = Vulkan::shader_apply_globals;
			out_backend->shader_apply_instance = Vulkan::shader_apply_instance;
			out_backend->shader_acquire_instance_resources = Vulkan::shader_acquire_instance_resources;
			out_backend->shader_release_instance_resources = Vulkan::shader_release_instance_resources;

			out_backend->texture_map_acquire_resources = Vulkan::texture_map_acquire_resources;
			out_backend->texture_map_release_resources = Vulkan::texture_map_release_resources;

			out_backend->window_attachment_get = Vulkan::window_attachment_get;
			out_backend->depth_attachment_get = Vulkan::depth_attachment_get;
			out_backend->window_attachment_index_get = Vulkan::window_attachment_index_get;

			out_backend->is_multithreaded = Vulkan::is_multithreaded;

			return true;
		}

		return false;
	}

	void Renderer::backend_destroy(Backend* backend)
	{
		Memory::zero_memory(backend, sizeof(Backend));
	}
}
