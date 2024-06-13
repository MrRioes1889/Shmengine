#pragma once

#include "RendererTypes.hpp"

#include "utility/Math.hpp"

namespace Renderer
{

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, const char* application_name);
	void system_shutdown();

	void on_resized(uint32 width, uint32 height);

	bool32 draw_frame(RenderData* data);

	SHMAPI void set_view(Math::Mat4 view);

	void create_texture(const void* pixels, Texture* texture);
	void destroy_texture(Texture* texture);

	bool32 create_material(Material* material);
	void destroy_material(Material* material);

	bool32 create_geometry(Geometry* geometry, uint32 vertex_count, const Vertex3D* vertices, uint32 index_count, const uint32* indices);
	void destroy_geometry(Geometry* geometry);

}


