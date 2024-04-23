#pragma once

#include "../RendererBackend.hpp"

namespace Renderer
{
	bool32 vulkan_init(Backend* backend, const char* application_name, Platform::PlatformState* plat_state);
	void vulkan_shutdown(Backend* backend);

	void vulkan_on_resized(Backend* backend, uint32 width, uint32 height);

	bool32 vulkan_begin_frame(Backend* backend, float32 delta_time);
	bool32 vulkan_end_frame(Backend* backend, float32 delta_time);
}