#pragma once

#include "../RendererBackend.hpp"
#include "utility/Math.hpp"
#include "resources/ResourceTypes.hpp"

struct VulkanContext;

namespace Renderer::Vulkan
{
	bool32 init(Backend* backend, const char* application_name);
	void shutdown(Backend* backend);

	void on_resized(Backend* backend, uint32 width, uint32 height);

	bool32 begin_frame(Backend* backend, float32 delta_time);
	void update_global_state(Math::Mat4 projection, Math::Mat4 view, Math::Vec3f view_position, Math::Vec4f ambient_colour, int32 mode);
	bool32 end_frame(Backend* backend, float32 delta_time);

	void draw_geometry(const GeometryRenderData& data);

	void create_texture(const void* pixels, Texture* texture);
	void destroy_texture(Texture* texture);

	bool32 create_material(Material* material);
	void destroy_material(Material* material);

	bool32 create_geometry (Geometry* geometry, uint32 vertex_count, const Vert3* vertices, uint32 index_count, const uint32* indices);
	void destroy_geometry(Geometry* geometry);
}