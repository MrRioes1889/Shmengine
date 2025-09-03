#pragma once

#include "RendererTypes.hpp"

#include "utility/Math.hpp"
#include "utility/String.hpp"
#include "core/Subsystems.hpp"

struct FrameData;
struct Shader;
struct ShaderConfig;
struct Skybox;
struct Terrain;
struct Box3D;
struct Scene;

namespace Renderer
{
	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);	

	void on_resized(uint32 width, uint32 height);

	bool8 draw_frame(FrameData* frame_data);

	bool8 render_target_create(uint32 attachment_count, const RenderTargetAttachment* attachments, RenderPass* pass, uint32 width, uint32 height, RenderTarget* out_target);
	void render_target_destroy(RenderTarget* target, bool8 free_internal_memory);

	void set_viewport(Math::Vec4f rect);
	void reset_viewport();
	void set_scissor(Math::Rect2Di rect);
	void reset_scissor();

	Texture* get_window_attachment(uint32 index);
	Texture* get_depth_attachment(uint32 attachment_index);
	uint32 get_window_attachment_index();
	SHMAPI uint32 get_window_attachment_count();

	SHMAPI bool8 renderpass_init(const RenderPassConfig* config, RenderPass* out_renderpass);
	SHMAPI void renderpass_destroy(RenderPass* pass);
	SHMAPI bool8 renderpass_begin(RenderPass* pass, RenderTarget* target);
	SHMAPI bool8 renderpass_end(RenderPass* pass);

	SHMAPI bool8 texture_init(Texture* texture);
	SHMAPI void texture_resize(Texture* texture, uint32 width, uint32 height);
	SHMAPI bool8 texture_write_data(Texture* t, uint32 offset, uint32 size, const uint8* pixels);
	SHMAPI bool8 texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory);
	SHMAPI bool8 texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba);
	SHMAPI void texture_destroy(Texture* texture);

	SHMAPI bool8 geometry_init(GeometryConfig* config, GeometryData* g);
	SHMAPI void geometry_destroy(GeometryData* g);
	SHMAPI bool8 geometry_load(GeometryData* geometry);
	SHMAPI void geometry_unload(GeometryData* geometry);
	SHMAPI void geometry_draw(GeometryData* geometry);

	SHMAPI bool8 shader_init(ShaderConfig* config, Shader* out_shader);
	SHMAPI bool8 shader_init_from_resource(const char* name, RenderPass* renderpass, Shader* out_shader);
	SHMAPI void shader_destroy(Shader* shader);
	SHMAPI bool8 shader_use(Shader* shader);
	SHMAPI bool8 shader_bind_globals(Shader* shader);
	SHMAPI bool8 shader_bind_instance(Shader* shader, ShaderInstanceId instance_id);
	SHMAPI bool8 shader_apply_globals(Shader* shader);
	SHMAPI bool8 shader_apply_instance(Shader* shader);
	SHMAPI ShaderInstanceId shader_acquire_instance(Shader* shader);
	SHMAPI bool8 shader_release_instance(Shader* shader, ShaderInstanceId instance_id);
	SHMAPI ShaderUniformId shader_get_uniform_index(Shader* shader, const char* uniform_name);
	SHMAPI bool8 shader_set_uniform(Shader* shader, ShaderUniformId uniform_id, const void* value);

	bool8 texture_map_init(TextureMapConfig* config, TextureMap* out_map);
	void texture_map_destroy(TextureMap* map);

	SHMAPI bool8 material_init(MaterialConfig* config, Material* out_material);
	SHMAPI bool8 material_init_from_resource_async(const char* name, Material* out_material);
	SHMAPI bool8 material_destroy(Material* material);

	SHMAPI bool8 mesh_init(MeshConfig* config, Mesh* out_mesh);
	SHMAPI bool8 mesh_init_from_resource_async(const char* name, Mesh* out_mesh);
	SHMAPI bool8 mesh_destroy(Mesh* mesh);

	SHMAPI bool8 renderbuffer_init(const char* name, RenderBufferType type, uint64 size, bool8 use_freelist, RenderBuffer* out_buffer);
	SHMAPI void renderbuffer_destroy(RenderBuffer* buffer);
	SHMAPI bool8 renderbuffer_bind(RenderBuffer* buffer, uint64 offset);
	SHMAPI bool8 renderbuffer_unbind(RenderBuffer* buffer);
	SHMAPI void* renderbuffer_map_memory(RenderBuffer* buffer, uint64 offset, uint64 size);
	SHMAPI void renderbuffer_unmap_memory(RenderBuffer* buffer);
	SHMAPI bool8 renderbuffer_flush(RenderBuffer* buffer, uint64 offset, uint64 size);
	SHMAPI bool8 renderbuffer_read(RenderBuffer* buffer, uint64 offset, uint64 size, void* out_memory);
	SHMAPI bool8 renderbuffer_resize(RenderBuffer* buffer, uint64 new_total_size);
	SHMAPI bool8 renderbuffer_allocate(RenderBuffer* buffer, uint64 size, RenderBufferAllocationReference* alloc);
	SHMAPI bool8 renderbuffer_reallocate(RenderBuffer* buffer, uint64 new_size, RenderBufferAllocationReference* alloc);
	SHMAPI void renderbuffer_free(RenderBuffer* buffer, RenderBufferAllocationReference* alloc);
	SHMAPI bool8 renderbuffer_load_range(RenderBuffer* buffer, uint64 offset, uint64 size, const void* data);
	SHMAPI bool8 renderbuffer_copy_range(RenderBuffer* source, uint64 source_offset, RenderBuffer* dest, uint64 dest_offset, uint64 size);
	SHMAPI bool8 renderbuffer_draw(RenderBuffer* buffer, uint64 offset, uint32 element_count, bool8 bind_only);

	bool8 is_multithreaded();
}


