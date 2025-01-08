#pragma once

#include "Defines.hpp"
#include "utility/MathTypes.hpp"
#include "renderer/RendererTypes.hpp"
#include "memory/LinearAllocator.hpp"
#include "core/Subsystems.hpp"

enum class RenderViewType
{
	WORLD = 1,
	UI = 2,
	SKYBOX = 3,
	PICK = 4
};

enum class RenderViewViewMatrixSource
{
	SCENE_CAMERA = 1,
	UI_CAMERA = 2,
	LIGHT_CAMERA = 3
};

enum class RenderViewProjMatrixSource
{
	DEFAULT_PERSPECTIVE = 1,
	DEFAULT_ORTHOGRAPHIC = 2
};


struct RenderViewConfig
{
	const char* name;
	const char* custom_shader_name;

	uint16 width;
	uint16 height;
	RenderViewType type;
	RenderViewViewMatrixSource view_matrix_source;
	RenderViewProjMatrixSource proj_matrix_source;

	Sarray<Renderer::RenderPassConfig> pass_configs;
};

struct RenderViewPacketData
{
	uint32 renderpass_id;
	uint32 geometries_count;

	Renderer::ObjectRenderData* geometries;
	LightingInfo lighting;
};

struct RenderView
{
	const char* name;
	uint32 id;
	uint16 width;
	uint16 height;
	RenderViewType type;

	Sarray<Renderer::RenderPass> renderpasses;
	Darray<Renderer::ObjectRenderData> geometries;

	const char* custom_shader_name;
	Buffer internal_data;

	bool32(*on_create)(RenderView* self);
	void (*on_destroy)(RenderView* self);
	void (*on_resize)(RenderView* self, uint32 width, uint32 height);
	bool32(*on_build_packet)(RenderView* self, Memory::LinearAllocator* frame_allocator, const RenderViewPacketData* packet_data);
	void (*on_end_frame)(RenderView* self);
	bool32(*on_render)(RenderView* self, Memory::LinearAllocator* frame_allocator, uint32 frame_number, uint64 render_target_index);
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

	bool32 create(const RenderViewConfig& config);

	SHMAPI RenderView* get(const char* name);

	SHMAPI bool32 build_packet(RenderView* view, Memory::LinearAllocator* frame_allocator, const RenderViewPacketData* packet_data);

	void on_window_resize(uint32 width, uint32 height);
	bool32 on_render(RenderView* view, Memory::LinearAllocator* frame_allocator, uint32 frame_number, uint64 render_target_index);

	void regenerate_render_targets(RenderView* view);

}