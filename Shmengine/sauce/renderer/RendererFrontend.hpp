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

	SHMAPI void set_view(Math::Mat4 view);

	void create_texture(const void* pixels, Texture* texture);
	void destroy_texture(Texture* texture);

	bool32 create_geometry(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices);
	void destroy_geometry(Geometry* geometry);

	bool32 get_renderpass_id(const char* name, uint8* out_renderpass_id);

	bool32 shader_create(Shader* shader, uint8 renderpass_id, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages);
	void shader_destroy(Shader* shader);

	bool32 shader_init(Shader* shader);
	bool32 shader_use(Shader* shader);

	bool32 shader_bind_globals(Shader* shader);
	bool32 shader_bind_instance(Shader* shader, uint32 instance_id);

	bool32 shader_apply_globals(Shader* shader);
	bool32 shader_apply_instance(Shader* shader);

	bool32 shader_acquire_instance_resources(Shader* shader, uint32* out_instance_id);
	bool32 shader_release_instance_resources(Shader* shader, uint32 instance_id);

	bool32 shader_set_uniform(Shader* shader, ShaderUniform* uniform, const void* value);

}


