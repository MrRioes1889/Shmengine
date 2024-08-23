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
		Config config;

		Camera default_camera;

		CameraLookup* registered_cameras;
		Hashtable<uint16> registered_camera_table;

	};

	static SystemState* system_state = 0;

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config)
	{
		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		system_state->config = config;

		uint64 camera_array_size = sizeof(CameraLookup) * config.max_camera_count;
		system_state->registered_cameras = (CameraLookup*)allocator_callback(camera_array_size);

		uint64 hashtable_data_size = sizeof(uint16) * config.max_camera_count;
		void* hashtable_data = allocator_callback(hashtable_data_size);
		system_state->registered_camera_table.init(config.max_camera_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

		system_state->registered_camera_table.floodfill(INVALID_ID16);

		for (uint32 i = 0; i < config.max_camera_count; ++i) 
		{
			system_state->registered_cameras[i].id = INVALID_ID16;
			system_state->registered_cameras[i].reference_count = 0;
		}

		system_state->default_camera = Camera();

		return true;
	}

	void system_shutdown()
	{
		system_state = 0;
	}

	Camera* acquire(const char* name)
	{
		
		if (CString::equal_i(name, Config::default_name))
			return get_default_camera();

		uint16& ref_id = system_state->registered_camera_table.get_ref(name);

		if (ref_id == INVALID_ID16)
		{
			for (uint16 i = 0; i < system_state->config.max_camera_count; ++i) {
				if (system_state->registered_cameras[i].id == INVALID_ID16) {
					ref_id = i;
					break;
				}
			}
			if (ref_id == INVALID_ID16) {
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

		if (CString::equal_i(name, Config::default_name))
			return;

		uint16& ref_id = system_state->registered_camera_table.get_ref(name);

		if (ref_id == INVALID_ID16)
			return;

		system_state->registered_cameras[ref_id].reference_count--;
		if (system_state->registered_cameras[ref_id].reference_count <= 0)
		{
			system_state->registered_cameras[ref_id].cam.reset();
			system_state->registered_cameras[ref_id].id = INVALID_ID16;
			ref_id = system_state->registered_cameras[ref_id].id;
		}
		
	}

	Camera* get_default_camera()
	{
		return &system_state->default_camera;
	}

}