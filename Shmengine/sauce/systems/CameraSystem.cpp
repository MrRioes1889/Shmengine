#include "CameraSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "containers/Hashtable.hpp"
#include "utility/Math.hpp"

namespace CameraSystem
{

	struct CameraLookup
	{
		uint16 id;
		uint16 reference_count;
		Camera cam;
	};

	struct SystemState
	{
		SystemConfig config;

		Camera default_camera;

		CameraLookup* registered_cameras;
		Hashtable<uint16> registered_camera_table;

	};

	static SystemState* system_state = 0;

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{

		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->config = *(SystemConfig*)config;

		uint64 camera_array_size = sizeof(CameraLookup) * system_state->config.max_camera_count;
		system_state->registered_cameras = (CameraLookup*)allocator_callback(allocator, camera_array_size);

		uint64 hashtable_data_size = sizeof(uint16) * system_state->config.max_camera_count;
		void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
		system_state->registered_camera_table.init(system_state->config.max_camera_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

		system_state->registered_camera_table.floodfill(Constants::max_u16);

		for (uint32 i = 0; i < system_state->config.max_camera_count; ++i)
		{
			system_state->registered_cameras[i].id = Constants::max_u16;
			system_state->registered_cameras[i].reference_count = 0;
		}

		system_state->default_camera = Camera();

		return true;
	}

	void system_shutdown(void* state)
	{
		system_state = 0;
	}

	Camera* acquire(const char* name)
	{
		
		if (CString::equal_i(name, SystemConfig::default_name))
			return get_default_camera();

		uint16& ref_id = system_state->registered_camera_table.get_ref(name);

		if (ref_id == Constants::max_u16)
		{
			for (uint16 i = 0; i < system_state->config.max_camera_count; ++i) {
				if (system_state->registered_cameras[i].id == Constants::max_u16) {
					ref_id = i;
					break;
				}
			}
			if (ref_id == Constants::max_u16) {
				SHMERROR("camera_system_acquire failed to acquire new slot. Adjust camera system config to allow more. Null returned.");
				return 0;
			}

			// Create/register the new camera.
			SHMTRACEV("Creating new camera named '%s'...", name);
			system_state->registered_cameras[ref_id].cam = Camera();
			system_state->registered_cameras[ref_id].id = ref_id;
		}
		system_state->registered_cameras[ref_id].reference_count++;
		return &system_state->registered_cameras[ref_id].cam;

	}

	void release(const char* name)
	{

		if (CString::equal_i(name, SystemConfig::default_name))
			return;

		uint16& ref_id = system_state->registered_camera_table.get_ref(name);

		if (ref_id == Constants::max_u16)
			return;

		system_state->registered_cameras[ref_id].reference_count--;
		if (system_state->registered_cameras[ref_id].reference_count <= 0)
		{
			system_state->registered_cameras[ref_id].cam.reset();
			system_state->registered_cameras[ref_id].id = Constants::max_u16;
			ref_id = system_state->registered_cameras[ref_id].id;
		}
		
	}

	Camera* get_default_camera()
	{
		return &system_state->default_camera;
	}

}