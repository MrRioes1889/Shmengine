#pragma once

#include "../RendererBackend.hpp"
#include "utility/Math.hpp"
#include "resources/ResourceTypes.hpp"

struct VulkanContext;

namespace Renderer
{
	bool32 vulkan_init(Backend* backend, const char* application_name);
	void vulkan_shutdown(Backend* backend);

	void vulkan_on_resized(Backend* backend, uint32 width, uint32 height);

	bool32 vulkan_begin_frame(Backend* backend, float32 delta_time);
	void vulkan_renderer_update_global_state(Math::Mat4 projection, Math::Mat4 view, Math::Vec3f view_position, Math::Vec4f ambient_colour, int32 mode);
	bool32 vulkan_end_frame(Backend* backend, float32 delta_time);

	void vulkan_renderer_update_object(const GeometryRenderData& data);

	void vulkan_create_texture(const void* pixels, Texture* texture);
	void vulkan_destroy_texture(Texture* texture);

	bool32 vulkan_create_material(Material* material);
	void vulkan_destroy_material(Material* material);
}