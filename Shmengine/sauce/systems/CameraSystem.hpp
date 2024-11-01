#pragma once

#include "Defines.hpp"
#include "renderer/Camera.hpp"
#include "core/Subsystems.hpp"

namespace CameraSystem
{

	struct SystemConfig
	{
		uint32 max_camera_count;

		inline static const char* default_name = "default";
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	Camera* acquire(const char* name);
	void release(const char* name);

	SHMAPI Camera* get_default_camera();

}