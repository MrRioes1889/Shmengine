#pragma once

#include "Defines.hpp"

#include "resources/ResourceTypes.hpp"
#include "utility/Math.hpp"

namespace Platform
{
	struct PlatformState;
}

namespace Renderer
{

	enum BackendType
	{
		RENDERER_BACKEND_TYPE_VULKAN,
		RENDERER_BACKEND_TYPE_OPENGL,
		RENDERER_BACKEND_TYPE_DIRECTX,
	};

	struct GlobalUniformObject
	{
		Math::Mat4 projection;
		Math::Mat4 view;

		// NOTE: Padding added to comply with some Nvidia standard about global uniform object having to be 256 bytes large.
		uint8 padding[128];
	};
	static_assert(sizeof(GlobalUniformObject) == 256);

	struct MaterialUniformObject
	{
		Math::Vec4f diffuse_color;

		uint8 padding[256 - sizeof(diffuse_color)];
	};
	static_assert(sizeof(MaterialUniformObject) == 256);

	struct GeometryRenderData
	{
		Math::Mat4 model;
		Geometry* geometry;
	};

	struct Vert3
	{
		Math::Vec3f position;
		Math::Vec2f tex_coordinates;
	};

	struct Backend
	{

		uint64 frame_count;

		bool32(*init)(Backend* backend, const char* application_name);
		void (*shutdown)(Backend* backend);

		void (*on_resized)(Backend* backend, uint32 width, uint32 height);

		bool32(*begin_frame)(Backend* backend, float32 delta_time);
		void(*update_global_state)(Math::Mat4 projection, Math::Mat4 view, Math::Vec3f view_position, Math::Vec4f ambient_colour, int32 mode);
		bool32(*end_frame)(Backend* backend, float32 delta_time);

		void (*draw_geometry)(const GeometryRenderData& data);

		void (*create_texture)(const void* pixels, Texture* texture);
		void (*destroy_texture)(Texture* texture);

		bool32 (*create_material)(Material* material);
		void (*destroy_material)(Material* material);

		bool32(*create_geometry)(Geometry* geometry, uint32 vertex_count, const Vert3* vertices, uint32 index_count, const uint32* indices);
		void (*destroy_geometry)(Geometry* geometry);
	};

	struct RenderData
	{
		float32 delta_time;

		uint32 geometry_count;
		GeometryRenderData* geometries;
	};
}
