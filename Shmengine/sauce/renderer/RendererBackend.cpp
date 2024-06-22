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
			out_backend->begin_renderpass = Vulkan::begin_renderpass;
			out_backend->end_renderpass = Vulkan::end_renderpass;
			out_backend->on_resized = Vulkan::on_resized;
			out_backend->draw_geometry = Vulkan::draw_geometry;
			out_backend->create_texture = Vulkan::create_texture;
			out_backend->destroy_texture = Vulkan::destroy_texture;
			out_backend->create_geometry = Vulkan::create_geometry;
			out_backend->destroy_geometry = Vulkan::destroy_geometry;

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

			return true;
		}

		return false;
	}

	void Renderer::backend_destroy(Backend* backend)
	{
		Memory::zero_memory(backend, sizeof(Backend));
	}
}
