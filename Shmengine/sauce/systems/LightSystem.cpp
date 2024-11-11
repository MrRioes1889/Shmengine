#include "LightSystem.hpp"

#include "containers/Darray.hpp"

namespace LightSystem
{

	struct SystemState
	{
		DirectionalLight dir_light;
		Darray<PointLight> point_lights;
	};

	static SystemState* system_state = 0;

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->point_lights.init(10, DarrayFlags::NON_RESIZABLE);

		return true;
	}

	void system_shutdown(void* state)
	{
		system_state->point_lights.free_data();
		system_state = 0;
	}

	uint32 add_point_light(PointLight light)
	{
		if (system_state->point_lights.count == system_state->point_lights.capacity)
			return INVALID_ID;

		system_state->point_lights.emplace(light);
		return system_state->point_lights.count - 1;
	}

	PointLight* get_point_light(uint32 index)
	{
		if (index < system_state->point_lights.count)
			return &system_state->point_lights[index];
		else
			return 0;
	}

	bool32 remove_point_light(uint32 index)
	{
		if (index < system_state->point_lights.count)
			system_state->point_lights.remove_at(index);
		return true;
	}

	DirectionalLight* get_directional_light()
	{
		return &system_state->dir_light;
	}

	uint32 get_point_lights_count()
	{
		return system_state->point_lights.count;
	}

	const Darray<PointLight>* get_point_lights()
	{
		return &system_state->point_lights;
	}
}