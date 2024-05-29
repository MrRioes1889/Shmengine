#pragma once

#include "Defines.hpp"

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

	struct Backend
	{

		uint64 frame_count;

		bool32(*init)(Backend* backend, const char* application_name);
		void (*shutdown)(Backend* backend);

		void (*on_resized)(Backend* backend, uint32 width, uint32 height);

		bool32(*begin_frame)(Backend* backend, float32 delta_time);
		bool32(*end_frame)(Backend* backend, float32 delta_time);
	};

	struct RenderData
	{
		float32 delta_time;
	};
}
