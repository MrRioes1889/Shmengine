#include "RendererBackend.hpp"

#include "vulkan_renderer/VulkanBackend.hpp"

namespace Renderer
{
	bool32 Renderer::backend_create(BackendType type, Backend* out_backend)
	{

		out_backend->frame_count = 0;

		if (type == RENDERER_BACKEND_TYPE_VULKAN)
		{
			out_backend->init = Vulkan::init;
			out_backend->shutdown = Vulkan::shutdown;
			out_backend->begin_frame = Vulkan::begin_frame;
			out_backend->end_frame = Vulkan::end_frame;
			out_backend->begin_renderpass = Vulkan::begin_renderpass;
			out_backend->end_renderpass = Vulkan::end_renderpass;
			out_backend->on_resized = Vulkan::on_resized;
			out_backend->update_global_world_state = Vulkan::update_global_world_state;
			out_backend->update_global_ui_state = Vulkan::update_global_ui_state;
			out_backend->draw_geometry = Vulkan::draw_geometry;
			out_backend->create_texture = Vulkan::create_texture;
			out_backend->destroy_texture = Vulkan::destroy_texture;
			out_backend->create_material = Vulkan::create_material;
			out_backend->destroy_material = Vulkan::destroy_material;
			out_backend->create_geometry = Vulkan::create_geometry;
			out_backend->destroy_geometry = Vulkan::destroy_geometry;

			return true;
		}

		return false;
	}

	void Renderer::backend_destroy(Backend* backend)
	{
		backend->begin_frame = 0;
		backend->end_frame = 0;
		backend->begin_renderpass = 0;
		backend->end_renderpass = 0;
		backend->shutdown = 0;
		backend->init = 0;
		backend->on_resized = 0;
		backend->update_global_world_state = 0;
		backend->update_global_ui_state = 0;
		backend->draw_geometry = 0;
		backend->create_texture = 0;
		backend->destroy_texture = 0;
		backend->create_material = 0;
		backend->destroy_material = 0;
		backend->create_geometry = 0;
		backend->destroy_geometry = 0;
	}
}
