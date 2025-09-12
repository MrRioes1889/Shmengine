#pragma once

#include <renderer/RendererTypes.hpp>
#include <utility/Math.hpp>

struct FrameData;
struct VulkanContext;

namespace Renderer::Vulkan
{
	bool8 init(void* context, const ModuleConfig& config, DeviceProperties* out_device_properties);
	void shutdown();

	void vk_device_sleep_till_idle();

	void on_config_changed();
	void on_resized(uint32 width, uint32 height);

	bool8 vk_begin_frame(const FrameData* frame_data);
	bool8 vk_end_frame(const FrameData* frame_data);

	bool8 vk_render_target_create(uint32 attachment_count, const RenderTargetAttachment* attachments, RenderPass* pass, uint32 width, uint32 height, RenderTarget* out_target);
	void vk_render_target_destroy(RenderTarget* target, bool8 free_internal_memory);

	void vk_set_viewport(Math::Vec4f rect);
	void vk_reset_viewport();
	void vk_set_scissor(Math::Rect2Di rect);
	void vk_reset_scissor();

	Texture* vk_get_color_attachment(uint32 index);
	Texture* vk_get_depth_attachment(uint32 attachment_index);
	uint32 vk_get_window_attachment_index();
	uint32 vk_get_window_attachment_count();

	bool8 vk_renderpass_init(const RenderPassConfig* config, RenderPass* out_renderpass);
	void vk_renderpass_destroy(RenderPass* pass);

	bool8 vk_renderpass_begin(RenderPass* renderpass, RenderTarget* render_target);
	bool8 vk_renderpass_end(RenderPass* renderpass);

	bool8 vk_texture_init(Texture* texture);
	void vk_texture_resize(Texture* texture, uint32 width, uint32 height);
	bool8 vk_texture_write_data(Texture* texture, uint32 offset, uint32 size, const uint8* pixels);
	bool8 vk_texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory);
	bool8 vk_texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba);
	void vk_texture_destroy(Texture* texture);

	bool8 vk_shader_init(ShaderConfig* config, Shader* shader);
	void vk_shader_destroy(Shader* shader);

	bool8 vk_shader_use(Shader* shader);

	bool8 vk_shader_bind_globals(Shader* s);
	bool8 vk_shader_bind_instance(Shader* s, ShaderInstanceId instance_id);

	bool8 vk_shader_apply_globals(Shader* s);
	bool8 vk_shader_apply_instance(Shader* s);

	bool8 vk_shader_acquire_instance(Shader* s, ShaderInstanceId instance_id);
	bool8 vk_shader_release_instance(Shader* s, ShaderInstanceId instance_id);

	bool8 vk_shader_set_uniform(Shader* frontend_shader, ShaderUniform* uniform, const void* value);

	bool8 vk_texture_sampler_init(TextureSampler* out_sampler);
	void vk_texture_sampler_destroy(TextureSampler* sampler);

	bool8 vk_buffer_init(RenderBuffer* buffer);
	void vk_buffer_destroy(RenderBuffer* buffer);
	bool8 vk_buffer_resize(RenderBuffer* buffer, uint64 new_size);
	bool8 vk_buffer_bind(RenderBuffer* buffer, uint64 offset);
	bool8 vk_buffer_unbind(RenderBuffer* buffer);
	void* vk_buffer_map_memory(RenderBuffer* buffer, uint64 offset, uint64 size);
	void vk_buffer_unmap_memory(RenderBuffer* buffer);
	bool8 vk_buffer_flush(RenderBuffer* buffer, uint64 offset, uint64 size);
	bool8 vk_buffer_read(RenderBuffer* buffer, uint64 offset, uint64 size, void* out_memory);
	bool8 vk_buffer_load_range(RenderBuffer* buffer, uint64 offset, uint64 size, const void* data);
	bool8 vk_buffer_copy_range(RenderBuffer* source, uint64 source_offset, RenderBuffer* dest, uint64 dest_offset, uint64 size);
	bool8 vk_buffer_draw(RenderBuffer* buffer, uint64 offset, uint32 element_count, bool8 bind_only);

	bool8 vk_is_multithreaded();
}