#pragma once

#include "Defines.hpp"

struct Application;
struct FrameData;

namespace Engine
{

	SHMAPI bool32 init(Application* app_inst);
	SHMAPI bool32 run(Application* app_inst);

	SHMAPI float64 get_frame_delta_time();
	SHMAPI const char* get_application_name();

}

