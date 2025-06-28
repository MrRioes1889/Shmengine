#pragma once

#include "Defines.hpp"
#include "utility/MathTypes.hpp"
#include "renderer/RendererTypes.hpp"
#include "core/Subsystems.hpp"
#include "core/Identifier.hpp"

struct FrameData;
struct Camera;

struct Mesh;
struct Terrain;
struct UIText;
struct Box3D;
struct Line3D;
struct Gizmo3D;

struct RenderViewPacketData
{
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

typedef bool32(*FP_on_create)(RenderView* self);
typedef void (*FP_on_destroy)(RenderView* self);
typedef void (*FP_on_resize)(RenderView* self, uint32 width, uint32 height);
typedef bool32(*FP_on_build_packet)(RenderView* self, FrameData* frame_data, const RenderViewPacketData* packet_data);
typedef void (*FP_on_end_frame)(RenderView* self);
typedef bool32(*FP_on_render)(RenderView* self, FrameData* frame_data, uint32 frame_number, uint64 render_target_index);
typedef bool32(*FP_regenerate_attachment_target)(const RenderView* self, uint32 pass_index, Renderer::RenderTargetAttachment* attachment);

struct RenderViewConfig
{
	const char* name;
	uint16 width;
	uint16 height;

	uint32 renderpass_count;
	Renderer::RenderPassConfig* renderpass_configs;

	const char* custom_shader_name;

	FP_on_create on_create;
	FP_on_destroy on_destroy;
	FP_on_resize on_resize;
	FP_on_build_packet on_build_packet;
	FP_on_end_frame on_end_frame;
	FP_on_render on_render;
	FP_regenerate_attachment_target on_regenerate_attachment_target;
};

struct RenderView
{
	const char* name;
	Id16 id;
	uint16 width;
	uint16 height;
	bool8 enabled;

	Sarray<Renderer::RenderPass> renderpasses;

	Darray<RenderViewGeometryData> geometries;
	Darray<RenderViewInstanceData> instances;
	Darray<RenderViewObjectData> objects;

	const char* custom_shader_name;
	Buffer internal_data;

	FP_on_create on_create;
	FP_on_destroy on_destroy;
	FP_on_resize on_resize;
	FP_on_build_packet on_build_packet;
	FP_on_end_frame on_end_frame;
	FP_on_render on_render;
	FP_regenerate_attachment_target on_regenerate_attachment_target;
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

	SHMAPI Id16 create_view(const RenderViewConfig* config);
	SHMAPI void destroy_view(Id16 view_id);

	SHMAPI RenderView* get(const char* name);
	SHMAPI Id16 get_id(const char* name);

	SHMAPI bool32 build_packet(Id16 view_id, FrameData* frame_data, const RenderViewPacketData* packet_data);

	void on_window_resize(uint32 width, uint32 height);
	bool32 on_render(FrameData* frame_data, uint32 frame_number, uint64 render_target_index);
	void on_end_frame();

	SHMAPI void regenerate_render_targets(Id16 view_id);

	SHMAPI uint32 mesh_draw(Id16 view_id, Mesh* mesh, uint32 shader_id, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum);
	SHMAPI uint32 meshes_draw(Id16 view_id, Mesh* meshes, uint32 mesh_count, uint32 shader_id, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum);
	SHMAPI bool32 skybox_draw(Id16 view_id, Skybox* skybox, uint32 shader_id, FrameData* frame_data);
	SHMAPI uint32 terrain_draw(Id16 view_id, Terrain* terrain, uint32 shader_id, LightingInfo lighting, FrameData* frame_data);
	SHMAPI uint32 terrains_draw(Id16 view_id, Terrain* terrains, uint32 terrains_coun, uint32 shader_id, LightingInfo lighting, FrameData* frame_data);
	SHMAPI bool32 ui_text_draw(Id16 view_id, UIText* text, uint32 shader_id, FrameData* frame_data);
	SHMAPI uint32 box3D_draw(Id16 view_id, Box3D* box, uint32 shader_id, FrameData* frame_data);
	SHMAPI uint32 boxes3D_draw(Id16 view_id, Box3D* boxes, uint32 boxes_count, uint32 shader_id, FrameData* frame_data);
	SHMAPI uint32 line3D_draw(Id16 view_id, Line3D* line, uint32 shader_id, FrameData* frame_data);
	SHMAPI uint32 lines3D_draw(Id16 view_id, Line3D* lines, uint32 lines_count, uint32 shader_id, FrameData* frame_data);
	SHMAPI uint32 gizmo3D_draw(Id16 view_id, Gizmo3D* gizmo, uint32 shader_id, FrameData* frame_data, Camera* camera);

}