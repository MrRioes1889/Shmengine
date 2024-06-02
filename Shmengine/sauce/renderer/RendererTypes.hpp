#pragma once

#include "Defines.hpp"

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

	struct Backend
	{

		uint64 frame_count;

		bool32(*init)(Backend* backend, const char* application_name);
		void (*shutdown)(Backend* backend);

		void (*on_resized)(Backend* backend, uint32 width, uint32 height);

		bool32(*begin_frame)(Backend* backend, float32 delta_time);
		void(*update_global_state)(Math::Mat4 projection, Math::Mat4 view, Math::Vec3f view_position, Math::Vec4f ambient_colour, int32 mode);
		bool32(*end_frame)(Backend* backend, float32 delta_time);

		void (*update_object)(Math::Mat4 model);
	};

	struct RenderData
	{
		float32 delta_time;
	};
}
