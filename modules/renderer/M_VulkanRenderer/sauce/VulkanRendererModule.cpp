#include "VulkanRendererModule.hpp"

#include "renderer/VulkanBackend.hpp"
#include "renderer/VulkanTypes.hpp"

namespace Renderer::Vulkan
{
	uint64 get_context_size_requirement()
	{
		return sizeof(VulkanContext);
	}

	bool8 create_module(Module* out_module)
	{

		out_module->frame_number = 0;

		out_module->get_context_size_requirement = get_context_size_requirement;

		out_module->init = Vulkan::init;
		out_module->shutdown = Vulkan::shutdown;
		out_module->device_sleep_till_idle = Vulkan::vk_device_sleep_till_idle;
		out_module->begin_frame = Vulkan::vk_begin_frame;
		out_module->end_frame = Vulkan::vk_end_frame;
		out_module->renderpass_init = Vulkan::vk_renderpass_init;
		out_module->renderpass_destroy = Vulkan::vk_renderpass_destroy;
		out_module->renderpass_begin = Vulkan::vk_renderpass_begin;
		out_module->renderpass_end = Vulkan::vk_renderpass_end;
		out_module->render_target_init = Vulkan::vk_render_target_create;
		out_module->render_target_destroy = Vulkan::vk_render_target_destroy;
		out_module->on_resized = Vulkan::on_resized;

		out_module->texture_init = Vulkan::vk_texture_init;
		out_module->texture_resize = Vulkan::vk_texture_resize;
		out_module->texture_write_data = Vulkan::vk_texture_write_data;
		out_module->texture_read_data = Vulkan::vk_texture_read_data;
		out_module->texture_read_pixel = Vulkan::vk_texture_read_pixel;
		out_module->texture_destroy = Vulkan::vk_texture_destroy;

		out_module->shader_init = Vulkan::vk_shader_init;
		out_module->shader_destroy = Vulkan::vk_shader_destroy;
		out_module->shader_set_uniform = Vulkan::vk_shader_set_uniform;
		out_module->shader_use = Vulkan::vk_shader_use;
		out_module->shader_bind_globals = Vulkan::vk_shader_bind_globals;
		out_module->shader_bind_instance = Vulkan::vk_shader_bind_instance;

		out_module->shader_apply_globals = Vulkan::vk_shader_apply_globals;
		out_module->shader_apply_instance = Vulkan::vk_shader_apply_instance;
		out_module->shader_acquire_instance = Vulkan::vk_shader_acquire_instance;
		out_module->shader_release_instance = Vulkan::vk_shader_release_instance;

		out_module->texture_sampler_init = Vulkan::vk_texture_sampler_init;
		out_module->texture_sampler_destroy = Vulkan::vk_texture_sampler_destroy;

		out_module->renderbuffer_init = Vulkan::vk_buffer_init;
		out_module->renderbuffer_destroy = Vulkan::vk_buffer_destroy;
		out_module->renderbuffer_bind = Vulkan::vk_buffer_bind;
		out_module->renderbuffer_unbind = Vulkan::vk_buffer_unbind;
		out_module->renderbuffer_map_memory = Vulkan::vk_buffer_map_memory;
		out_module->renderbuffer_unmap_memory = Vulkan::vk_buffer_unmap_memory;
		out_module->renderbuffer_flush = Vulkan::vk_buffer_flush;
		out_module->renderbuffer_read = Vulkan::vk_buffer_read;
		out_module->renderbuffer_resize = Vulkan::vk_buffer_resize;
		out_module->renderbuffer_load_range = Vulkan::vk_buffer_load_range;
		out_module->renderbuffer_copy_range = Vulkan::vk_buffer_copy_range;
		out_module->renderbuffer_draw = Vulkan::vk_buffer_draw;

		out_module->get_window_attachment = Vulkan::vk_get_color_attachment;
		out_module->get_depth_attachment = Vulkan::vk_get_depth_attachment;
		out_module->get_window_attachment_index = Vulkan::vk_get_window_attachment_index;
		out_module->get_window_attachment_count = Vulkan::vk_get_window_attachment_count;
			
		out_module->set_viewport = Vulkan::vk_set_viewport;
		out_module->reset_viewport = Vulkan::vk_reset_viewport;
		out_module->set_scissor = Vulkan::vk_set_scissor;
		out_module->reset_scissor = Vulkan::vk_reset_scissor;

		out_module->is_multithreaded = Vulkan::vk_is_multithreaded;

		return true;

	}

	void destroy_module(Module* module)
	{
		Memory::zero_memory(module, sizeof(Module));
	}

	
}
