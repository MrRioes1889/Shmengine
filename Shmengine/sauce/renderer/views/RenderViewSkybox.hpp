#pragma once

#include <Defines.hpp>
#include <renderer/RendererTypes.hpp>
#include <systems/RenderViewSystem.hpp>

struct FrameData;

bool8 render_view_skybox_on_create(RenderView* self);
void render_view_skybox_on_destroy(RenderView* self);
void render_view_skybox_on_resize(RenderView* self, uint32 width, uint32 height);
bool8 render_view_skybox_on_build_packet(RenderView* self, FrameData* frame_data, const RenderViewPacketData* packet_data);
void render_view_skybox_on_end_frame(RenderView* self);
bool8 render_view_skybox_on_render(RenderView* self, FrameData* frame_data, uint32 frame_number, uint64 render_target_index);

