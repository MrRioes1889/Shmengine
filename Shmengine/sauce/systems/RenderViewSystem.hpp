#pragma once

#include "Defines.hpp"
#include "utility/MathTypes.hpp"
#include "renderer/RendererTypes.hpp"
#include "core/Subsystems.hpp"

struct FrameData;

struct Mesh;
struct Terrain;
struct UIText;
struct Box3D;

struct RenderViewPacketData
{
	uint32 renderpass_id;
	uint32 geometries_pushed_count;
	uint32 instances_pushed_count;
	uint32 objects_pushed_count;
};

struct RenderViewObjectData
{
	Math::Mat4 model;
	UniqueId unique_id;
	LightingInfo lighting;
};

struct RenderViewInstanceData
{
	uint32 shader_instance_id;
	uint32 texture_maps_count;
	uint32 shader_id;

	void* instance_properties;
	TextureMap** texture_maps;
};

struct RenderViewGeometryData
{
	uint32 shader_id;
	uint32 shader_instance_id;
	uint32 object_index;
	bool8 has_transparency;
	GeometryData* geometry_data;
};

struct RenderView
{
	const char* name;
	uint16 width;
	uint16 height;

	Sarray<Renderer::RenderPass> renderpasses;

	Darray<RenderViewGeometryData> geometries;
	Darray<RenderViewInstanceData> instances;
	Darray<RenderViewObjectData> objects;

	const char* custom_shader_name;
	Buffer internal_data;

	bool32(*on_register)(RenderView* self);
	void (*on_unregister)(RenderView* self);
	void (*on_resize)(RenderView* self, uint32 width, uint32 height);
	bool32(*on_build_packet)(RenderView* self, FrameData* frame_data, const RenderViewPacketData* packet_data);
	void (*on_end_frame)(RenderView* self);
	bool32(*on_render)(RenderView* self, FrameData* frame_data, uint32 frame_number, uint64 render_target_index);
	bool32(*regenerate_attachment_target)(const RenderView* self, uint32 pass_index, Renderer::RenderTargetAttachment* attachment);
};

namespace RenderViewSystem
{

	struct SystemConfig
	{
		uint32 max_view_count;

		inline static const char* default_name = "default";
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI bool32 register_view(RenderView* view, uint32 renderpass_count, Renderer::RenderPassConfig* renderpass_configs);

	SHMAPI RenderView* get(const char* name);

	SHMAPI bool32 build_packet(RenderView* view, FrameData* frame_data, const RenderViewPacketData* packet_data);

	void on_window_resize(uint32 width, uint32 height);
	bool32 on_render(RenderView* view, FrameData* frame_data, uint32 frame_number, uint64 render_target_index);
	void on_end_frame(RenderView* view);

	SHMAPI void regenerate_render_targets(RenderView* view);

	SHMAPI uint32 mesh_draw(RenderView* view, Mesh* mesh, uint32 renderpass_id, uint32 shader_id, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum);
	SHMAPI uint32 meshes_draw(RenderView* view, Mesh* meshes, uint32 mesh_count, uint32 renderpass_id, uint32 shader_id, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum);
	SHMAPI bool32 skybox_draw(RenderView* view, Skybox* skybox, uint32 renderpass_id, uint32 shader_id, FrameData* frame_data);
	SHMAPI uint32 terrain_draw(RenderView* view, Terrain* terrain, uint32 renderpass_id, uint32 shader_id, LightingInfo lighting, FrameData* frame_data);
	SHMAPI uint32 terrains_draw(RenderView* view, Terrain* terrains, uint32 terrains_count, uint32 renderpass_id, uint32 shader_id, LightingInfo lighting, FrameData* frame_data);
	SHMAPI bool32 ui_text_draw(RenderView* view, UIText* text, uint32 renderpass_id, uint32 shader_id, FrameData* frame_data);
	SHMAPI uint32 box3D_draw(RenderView* view, Box3D* box, uint32 renderpass_id, uint32 shader_id, FrameData* frame_data);
	SHMAPI uint32 boxes3D_draw(RenderView* view, Box3D* boxes, uint32 boxes_count, uint32 renderpass_id, uint32 shader_id, FrameData* frame_data);

}