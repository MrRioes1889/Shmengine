#pragma once

#include <renderer/RendererTypes.hpp>
#include <utility/Math.hpp>
#include <resources/ResourceTypes.hpp>

struct FrameData;
struct VulkanContext;

namespace Renderer::Vulkan
{
	bool32 init(void* context, const ModuleConfig& config, uint32* out_window_render_target_count);
	void shutdown();

	void on_config_changed();
	void on_resized(uint32 width, uint32 height);

	bool32 begin_frame(const FrameData* frame_data);
	bool32 end_frame(const FrameData* frame_data);

	bool32 vk_render_target_create(uint32 attachment_count, const RenderTargetAttachment* attachments, Renderpass* pass, uint32 width, uint32 height, RenderTarget* out_target);
	void vk_render_target_destroy(RenderTarget* target, bool32 free_internal_memory);

	void vk_set_viewport(Math::Vec4f rect);
	void vk_reset_viewport();
	void vk_set_scissor(Math::Rect2Di rect);
	void vk_reset_scissor();

	Texture* vk_get_color_attachment(uint32 index);
	Texture* vk_get_depth_attachment(uint32 attachment_index);
	uint32 vk_get_window_attachment_index();
	uint32 vk_get_window_attachment_count();

	bool32 vk_renderpass_create(const RenderpassConfig* config, Renderpass* out_renderpass);
	void vk_renderpass_destroy(Renderpass* pass);

	bool32 vk_renderpass_begin(Renderpass* renderpass, RenderTarget* render_target);
	bool32 vk_renderpass_end(Renderpass* renderpass);

	void vk_texture_create(const void* pixels, Texture* texture);
	void vk_texture_create_writable(Texture* texture);
	void vk_texture_resize(Texture* texture, uint32 width, uint32 height);
	bool32 vk_texture_write_data(Texture* texture, uint32 offset, uint32 size, const uint8* pixels);
	bool32 vk_texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory);
	bool32 vk_texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba);
	void vk_texture_destroy(Texture* texture);

	bool32 vk_geometry_create(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices);
	void vk_geometry_destroy(Geometry* geometry);

	void vk_geometry_draw(const GeometryRenderData& data);

	bool32 vk_shader_create(Shader* shader, const ShaderConfig* config, const Renderpass* renderpass, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages);
	void vk_shader_destroy(Shader* shader);

	bool32 vk_shader_init(Shader* shader);
	bool32 vk_shader_use(Shader* shader);

	bool32 vk_shader_bind_globals(Shader* s);
	bool32 vk_shader_bind_instance(Shader* s, uint32 instance_id);

	bool32 vk_shader_apply_globals(Shader* s);
	bool32 vk_shader_apply_instance(Shader* s, bool32 needs_update);

	bool32 vk_shader_acquire_instance_resources(Shader* s, TextureMap** maps, uint32* out_instance_id);
	bool32 vk_shader_release_instance_resources(Shader* s, uint32 instance_id);

	bool32 vk_shader_set_uniform(Shader* frontend_shader, ShaderUniform* uniform, const void* value);

	bool32 vk_texture_map_acquire_resources(TextureMap* out_map);
	void vk_texture_map_release_resources(TextureMap* map);

	bool32 vk_buffer_create(Renderbuffer* buffer);
	void vk_buffer_destroy(Renderbuffer* buffer);
	bool32 vk_buffer_resize(Renderbuffer* buffer, uint64 new_size);
	bool32 vk_buffer_bind(Renderbuffer* buffer, uint64 offset);
	bool32 vk_buffer_unbind(Renderbuffer* buffer);
	void* vk_buffer_map_memory(Renderbuffer* buffer, uint64 offset, uint64 size);
	void vk_buffer_unmap_memory(Renderbuffer* buffer);
	bool32 vk_buffer_flush(Renderbuffer* buffer, uint64 offset, uint64 size);
	bool32 vk_buffer_read(Renderbuffer* buffer, uint64 offset, uint64 size, void* out_memory);
	bool32 vk_buffer_load_range(Renderbuffer* buffer, uint64 offset, uint64 size, const void* data);
	bool32 vk_buffer_copy_range(Renderbuffer* source, uint64 source_offset, Renderbuffer* dest, uint64 dest_offset, uint64 size);
	bool32 vk_buffer_draw(Renderbuffer* buffer, uint64 offset, uint32 element_count, bool32 bind_only);

	bool8 vk_is_multithreaded();
}