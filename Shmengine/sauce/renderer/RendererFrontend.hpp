#pragma once

#include "RendererTypes.hpp"

#include "utility/Math.hpp"
#include "utility/String.hpp"

namespace Renderer
{

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, const char* application_name);
	void system_shutdown();

	void on_resized(uint32 width, uint32 height);

	bool32 draw_frame(RenderData* data);

	SHMAPI void set_view(const Math::Mat4& view, Math::Vec3f camera_position);

	void render_target_create(uint32 attachment_count, Texture* const * attachments, Renderpass* pass, uint32 width, uint32 height, RenderTarget* out_target);
	void render_target_destroy(RenderTarget* target, bool32 free_internal_memory);

	void renderpass_create(Renderpass* out_renderpass, float32 depth, uint32 stencil, bool32 has_prev_pass, bool32 has_next_pass);
	void renderpass_destroy(Renderpass* pass);
	Renderpass* renderpass_get(const char* name);

	void texture_create(const void* pixels, Texture* texture);
	void texture_create_writable(Texture * texture);
	void texture_resize(Texture* texture, uint32 width, uint32 height);
	void texture_write_data(Texture* texture, uint32 offset, uint32 size, const uint8* pixels);
	void texture_destroy(Texture* texture);

	bool32 geometry_create(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices);
	void geometry_destroy(Geometry* geometry);

	bool32 shader_create(Shader* shader, Renderpass* renderpass, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages);
	void shader_destroy(Shader* shader);

	bool32 shader_init(Shader* shader);
	bool32 shader_use(Shader* shader);

	bool32 shader_bind_globals(Shader* shader);
	bool32 shader_bind_instance(Shader* shader, uint32 instance_id);

	bool32 shader_apply_globals(Shader* shader);
	bool32 shader_apply_instance(Shader* shader, bool32 needs_update);

	bool32 shader_acquire_instance_resources(Shader* shader, TextureMap** maps, uint32* out_instance_id);
	bool32 shader_release_instance_resources(Shader* shader, uint32 instance_id);

	bool32 shader_set_uniform(Shader* shader, ShaderUniform* uniform, const void* value);

	bool32 texture_map_acquire_resources(TextureMap* out_map);
	void texture_map_release_resources(TextureMap* out_map);

}


