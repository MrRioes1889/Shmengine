#pragma once

#include "Defines.hpp"
#pragma once
#include "renderer/Camera.hpp"

namespace CameraSystem
{

	struct Config
	{
		uint32 max_camera_count;

		inline static const char* default_name = "default";
	};

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config);
	void system_shutdown();

	Camera* acquire(const char* name);
	void release(const char* name);

	SHMAPI Camera* get_default_camera();

}