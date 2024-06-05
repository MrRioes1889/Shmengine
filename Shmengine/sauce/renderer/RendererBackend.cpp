#include "RendererBackend.hpp"

#include "vulkan_renderer/VulkanBackend.hpp"

namespace Renderer
{
	bool32 Renderer::backend_create(BackendType type, Backend* out_backend)
	{

		out_backend->frame_count = 0;

		if (type == RENDERER_BACKEND_TYPE_VULKAN)
		{
			out_backend->init = vulkan_init;
			out_backend->shutdown = vulkan_shutdown;
			out_backend->begin_frame = vulkan_begin_frame;
			out_backend->end_frame = vulkan_end_frame;
			out_backend->on_resized = vulkan_on_resized;
			out_backend->update_global_state = vulkan_renderer_update_global_state;
			out_backend->update_object = vulkan_renderer_update_object;
			out_backend->create_texture = vulkan_create_texture;
			out_backend->destroy_texture = vulkan_destroy_texture;

			return true;
		}

		return false;
	}

	void Renderer::backend_destroy(Backend* backend)
	{
		backend->begin_frame = 0;
		backend->end_frame = 0;
		backend->shutdown = 0;
		backend->init = 0;
		backend->on_resized = 0;
		backend->update_global_state = 0;
		backend->update_object = 0;
		backend->create_texture = 0;
		backend->destroy_texture = 0;
	}
}
