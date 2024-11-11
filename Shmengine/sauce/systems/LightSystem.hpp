#pragma once

#include "Defines.hpp"
#include "core/Subsystems.hpp"
#include "utility/MathTypes.hpp"

template <typename T>
struct Darray;

namespace LightSystem
{
	
	struct DirectionalLight 
	{
		Math::Vec4f color;
		Math::Vec4f direction;
	};

	struct PointLight {
		Math::Vec4f color;
		Math::Vec4f position;
		// Usually 1, make sure denominator never gets smaller than 1
		float32 constant_f;
		// Reduces light intensity linearly
		float32 linear;
		// Makes the light fall off slower at longer distances.
		float32 quadratic;
		float32 padding;
	};

	struct SystemConfig
	{

	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI uint32 add_point_light(PointLight light);
	SHMAPI PointLight* get_point_light(uint32 index);
	SHMAPI bool32 remove_point_light(uint32 index);

	SHMAPI DirectionalLight* get_directional_light();

	SHMAPI uint32 get_point_lights_count();
	SHMAPI const Darray<PointLight>* get_point_lights();

}