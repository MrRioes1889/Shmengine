#pragma once

#include "../RendererBackend.hpp"
#include "utility/Math.hpp"
#include "resources/ResourceTypes.hpp"

struct VulkanContext;

namespace Renderer::Vulkan
{
	bool32 init(const char* application_name);
	void shutdown();

	void on_resized(uint32 width, uint32 height);

	bool32 begin_frame(float32 delta_time);
	bool32 end_frame(float32 delta_time);

	bool32 begin_renderpass(uint32 renderpass_id);
	bool32 end_renderpass(uint32 renderpass_id);

	void draw_geometry(const GeometryRenderData& data);

	void create_texture(const void* pixels, Texture* texture);
	void destroy_texture(Texture* texture);

	bool32 create_geometry (Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices);
	void destroy_geometry(Geometry* geometry);

	bool32 shader_create(Shader* shader, uint8 renderpass_id, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages);
	void shader_destroy(Shader* shader);

	bool32 shader_init(Shader* shader);
	bool32 shader_use(Shader* shader);

	bool32 shader_bind_globals(Shader* s);
	bool32 shader_bind_instance(Shader* s, uint32 instance_id);

	bool32 shader_apply_globals(Shader* s);
	bool32 shader_apply_instance(Shader* s);

	bool32 shader_acquire_instance_resources(Shader* s, uint32* out_instance_id);
	bool32 shader_release_instance_resources(Shader* s, uint32 instance_id);

	bool32 shader_set_uniform(Shader* frontend_shader, ShaderUniform* uniform, const void* value);
}