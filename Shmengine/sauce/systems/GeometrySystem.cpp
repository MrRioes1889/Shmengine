#include "GeometrySystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "core/Memory.hpp"
#include "containers/LinearStorage.hpp"
#include "systems/MaterialSystem.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/Geometry.hpp"

namespace GeometrySystem
{
	struct ReferenceCounter
	{
		uint16 reference_count;
		bool8 auto_destroy;
	};

	struct SystemState
	{
		Sarray<ReferenceCounter> geometry_ref_counters;
		LinearStorage<GeometryData, GeometryId> geometry_storage;
	};

	static SystemState* system_state = 0;

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
    {

		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		uint64 ref_counter_array_size = system_state->geometry_ref_counters.get_external_size_requirement(sys_config->max_geometry_count);
		void* ref_counter_array_data = allocator_callback(allocator, ref_counter_array_size);
		system_state->geometry_ref_counters.init(sys_config->max_geometry_count, 0, AllocationTag::Array, ref_counter_array_data);

        uint64 geometry_storage_size = system_state->geometry_storage.get_external_size_requirement(sys_config->max_geometry_count);
		void* geometry_storage_data = allocator_callback(allocator, geometry_storage_size);
        system_state->geometry_storage.init(sys_config->max_geometry_count, AllocationTag::Array, geometry_storage_data);

        return true;

	}

	void system_shutdown(void* state)
	{
		auto iter = system_state->geometry_storage.get_iterator();
		for (GeometryId geometry_id = iter.get_next(); geometry_id.is_valid(); geometry_id = iter.get_next())
		{
			GeometryData* geometry;
			system_state->geometry_storage.release(geometry_id, &geometry);
			Renderer::destroy_geometry(geometry);
			system_state->geometry_storage.verify_write(geometry_id);
		}
		system_state->geometry_storage.destroy();

		system_state = 0;
	}

	GeometryId create_geometry(GeometryConfig* config, bool8 auto_destroy)
	{
		GeometryId id;
		GeometryData* geometry;
		StorageReturnCode ret_code = system_state->geometry_storage.acquire(&id, &geometry);
		if (ret_code == StorageReturnCode::OutOfMemory)
		{
			SHMERROR("Could not obtain a free slot for geometry.");
			return GeometryId::invalid_value;
		}

		goto_if_log(!Renderer::create_geometry(config, geometry), fail, "Failed to create geometry from config");

		system_state->geometry_ref_counters[id] = { 1, auto_destroy };
		system_state->geometry_storage.verify_write(id);
		return id;

	fail:
		system_state->geometry_storage.revert_write(id);
		return GeometryId::invalid_value;
	}

	GeometryId acquire_reference(GeometryId id)
	{
		if (!system_state->geometry_storage.get_object(id))
		{
			SHMERROR("acquire_by_id - Cannot load invalid geometry id!");
			return GeometryId::invalid_value;
		}

		system_state->geometry_ref_counters[id].reference_count++;
		return id;
	}

	GeometryData* get_geometry_data(GeometryId id)
	{
		return system_state->geometry_storage.get_object(id);
	}

	void release(GeometryId id)
	{
		if (!system_state->geometry_storage.get_object(id))
		{
			SHMFATAL("Failed to release geometry. Could not find id.");
			return;
		}

        ReferenceCounter* ref_counter = &system_state->geometry_ref_counters[id];
        if (ref_counter->reference_count > 0)
			ref_counter->reference_count--;

        if (ref_counter->reference_count == 0 && ref_counter->auto_destroy)
		{
            GeometryData* geometry;
            system_state->geometry_storage.release(id, &geometry);
			Renderer::destroy_geometry(geometry);
            system_state->geometry_storage.verify_write(id);
		}
	}

	uint32 get_ref_count(GeometryId id)
	{
		if (!system_state->geometry_storage.get_object(id))
			return 0;

		return system_state->geometry_ref_counters[id].reference_count;
	}

}