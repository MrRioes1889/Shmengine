#pragma once

#include "RendererTypes.hpp"

#include "utility/Math.hpp"
#include "utility/String.hpp"
#include "core/Subsystems.hpp"

struct FrameData;

namespace Renderer
{

	struct SystemConfig
	{
		const char* application_name;
		RendererConfigFlags::Value flags;
		Module renderer_module;
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);	

	void on_resized(uint32 width, uint32 height);

	bool32 draw_frame(RenderPacket* data, const FrameData* frame_data);

	bool32 render_target_create(uint32 attachment_count, const RenderTargetAttachment* attachments, RenderPass* pass, uint32 width, uint32 height, RenderTarget* out_target);
	void render_target_destroy(RenderTarget* target, bool32 free_internal_memory);

	void set_viewport(Math::Vec4f rect);
	void reset_viewport();

	void set_scissor(Math::Rect2Di rect);
	void reset_scissor();

	Texture* get_window_attachment(uint32 index);
	Texture* get_depth_attachment(uint32 attachment_index);
	uint32 get_window_attachment_index();
	SHMAPI uint32 get_window_attachment_count();

	bool32 renderpass_create(const RenderPassConfig* config, RenderPass* out_renderpass);
	void renderpass_destroy(RenderPass* pass);

	bool32 renderpass_begin(RenderPass* pass, RenderTarget* target);
	bool32 renderpass_end(RenderPass* pass);

	void texture_create(const void* pixels, Texture* texture);
	void texture_create_writable(Texture * texture);
	void texture_resize(Texture* texture, uint32 width, uint32 height);
	bool32 texture_write_data(Texture* t, uint32 offset, uint32 size, const uint8* pixels);
	bool32 texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory);
	bool32 texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba);
	void texture_destroy(Texture* texture);

	bool32 geometry_load(GeometryData* geometry);
	bool32 geometry_reload(GeometryData* geometry);
	void geometry_unload(GeometryData* geometry);

	void geometry_draw(GeometryData* geometry);

	bool32 shader_create(Shader* shader, const ShaderConfig* config, const RenderPass* renderpass, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages);
	void shader_destroy(Shader* shader);

	bool32 shader_init(Shader* shader);
	bool32 shader_use(Shader* shader);

	bool32 shader_bind_globals(Shader* shader);
	bool32 shader_bind_instance(Shader* shader, uint32 instance_id);

	bool32 shader_apply_globals(Shader* shader);
	bool32 shader_apply_instance(Shader* shader, bool32 needs_update);

	bool32 shader_acquire_instance_resources(Shader* shader, uint32 maps_count, TextureMap** maps, uint32* out_instance_id);
	bool32 shader_release_instance_resources(Shader* shader, uint32 instance_id);

	bool32 shader_set_uniform(Shader* shader, ShaderUniform* uniform, const void* value);

	bool32 texture_map_acquire_resources(TextureMap* out_map);
	void texture_map_release_resources(TextureMap* out_map);

	SHMAPI bool32 renderbuffer_create(const char* name, RenderBufferType type, uint64 size, bool32 use_freelist, RenderBuffer* out_buffer);
	SHMAPI void renderbuffer_destroy(RenderBuffer* buffer);
	SHMAPI bool32 renderbuffer_bind(RenderBuffer* buffer, uint64 offset);
	SHMAPI bool32 renderbuffer_unbind(RenderBuffer* buffer);
	SHMAPI void* renderbuffer_map_memory(RenderBuffer* buffer, uint64 offset, uint64 size);
	SHMAPI void renderbuffer_unmap_memory(RenderBuffer* buffer);
	SHMAPI bool32 renderbuffer_flush(RenderBuffer* buffer, uint64 offset, uint64 size);
	SHMAPI bool32 renderbuffer_read(RenderBuffer* buffer, uint64 offset, uint64 size, void* out_memory);
	SHMAPI bool32 renderbuffer_resize(RenderBuffer* buffer, uint64 new_total_size);
	SHMAPI bool32 renderbuffer_allocate(RenderBuffer* buffer, uint64 size, uint64* out_offset);
	SHMAPI bool32 renderbuffer_reallocate(RenderBuffer* buffer, uint64 new_size, uint64 old_offset, uint64* new_offset);
	SHMAPI void renderbuffer_free(RenderBuffer* buffer, uint64 offset);
	SHMAPI bool32 renderbuffer_load_range(RenderBuffer* buffer, uint64 offset, uint64 size, const void* data);
	SHMAPI bool32 renderbuffer_copy_range(RenderBuffer* source, uint64 source_offset, RenderBuffer* dest, uint64 dest_offset, uint64 size);
	SHMAPI bool32 renderbuffer_draw(RenderBuffer* buffer, uint64 offset, uint32 element_count, bool32 bind_only);

	bool8 is_multithreaded();

}


