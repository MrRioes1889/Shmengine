#pragma once

#include <Defines.hpp>
#include <renderer/RendererTypes.hpp>
#include <systems/RenderViewSystem.hpp>

bool32 render_view_ui_on_register(RenderView* self);
void render_view_ui_on_unregister(RenderView* self);
void render_view_ui_on_resize(RenderView* self, uint32 width, uint32 height);
bool32 render_view_ui_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, const RenderViewPacketData* packet_data);
void render_view_ui_on_end_frame(RenderView* self);
bool32 render_view_ui_on_render(RenderView* self, Memory::LinearAllocator* frame_allocator, uint32 frame_number, uint64 render_target_index);

