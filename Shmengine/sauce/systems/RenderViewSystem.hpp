#pragma once

#include "Defines.hpp"
#include "utility/MathTypes.hpp"
#include "renderer/RendererTypes.hpp"
#include "memory/LinearAllocator.hpp"
#include "core/Subsystems.hpp"

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
	uint16 width;
	uint16 height;

	Sarray<Renderer::RenderPass> renderpasses;
	Darray<Renderer::ObjectRenderData> geometries;

	const char* custom_shader_name;
	Buffer internal_data;

	bool32(*on_register)(RenderView* self);
	void (*on_unregister)(RenderView* self);
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

	SHMAPI bool32 register_view(RenderView* view, uint32 renderpass_count, Renderer::RenderPassConfig* renderpass_configs);

	SHMAPI RenderView* get(const char* name);

	SHMAPI bool32 build_packet(RenderView* view, Memory::LinearAllocator* frame_allocator, const RenderViewPacketData* packet_data);

	void on_window_resize(uint32 width, uint32 height);
	bool32 on_render(RenderView* view, Memory::LinearAllocator* frame_allocator, uint32 frame_number, uint64 render_target_index);

	SHMAPI void regenerate_render_targets(RenderView* view);

}