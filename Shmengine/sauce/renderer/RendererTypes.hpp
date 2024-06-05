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

	struct ObjectUniformObject
	{
		Math::Vec4f diffuse_color;

		uint8 padding[48];
	};
	static_assert(sizeof(ObjectUniformObject) == 64);

	struct GeometryRenderData
	{
		uint32 object_id;
		Math::Mat4 model;
		Texture* textures[16];		
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

		void (*update_object)(const GeometryRenderData& data);

		void (*create_texture)(const char* name, uint32 width, uint32 height, uint32 channel_count, const void* pixels, bool32 has_transparency, Texture* out_texture);
		void (*destroy_texture)(Texture* texture);
	};

	struct RenderData
	{
		float32 delta_time;
	};
}
