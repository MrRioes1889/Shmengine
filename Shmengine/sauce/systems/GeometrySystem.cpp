#include "GeometrySystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "core/Memory.hpp"
#include "systems/MaterialSystem.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/Geometry.hpp"

namespace GeometrySystem
{
	struct GeometryReference
	{
		GeometryId id;
		uint16 reference_count;
		bool8 auto_unload;
		GeometryData geometry;
	};

	struct SystemState
	{
		Sarray<GeometryReference> geometries;
	};

	static void destroy_geometry_reference(GeometryReference* ref);

	static SystemState* system_state = 0;

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
    {

		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

        uint64 geometry_array_size = system_state->geometries.get_external_size_requirement(sys_config->max_geometry_count);
		void* geometry_array_data = allocator_callback(allocator, geometry_array_size);
        system_state->geometries.init(sys_config->max_geometry_count, 0, AllocationTag::Array, geometry_array_data);

        // Invalidate all geometries in the array.
        for (uint32 i = 0; i < system_state->geometries.capacity; ++i)
            system_state->geometries[i].id.invalidate();

        return true;

	}

	void system_shutdown(void* state)
	{
		for (uint32 i = 0; i < system_state->geometries.capacity; i++)
		{
			if (system_state->geometries[i].id.is_valid())
				destroy_geometry_reference(&system_state->geometries[i]);
		}

		system_state = 0;
	}

	GeometryId acquire_by_id(GeometryId id)
	{
		if (!id.is_valid() || system_state->geometries[id].id != id)
		{
			SHMERROR("acquire_by_id - Cannot load invalid geometry id!");
			return GeometryId::invalid_value;
		}

		system_state->geometries[id].reference_count++;
		return id;
	}

	GeometryId acquire_from_config(GeometryConfig* config, bool8 auto_unload)
	{
		GeometryId id = GeometryId::invalid_value;
		for (uint16 i = 0; i < system_state->geometries.capacity; i++)
		{
			if (!system_state->geometries[i].id.is_valid())
			{
				id = i;
				break;
			}
		}

		if (!id.is_valid())
		{
			SHMERROR("Could not obtain a free slot for geometry.");
			return GeometryId::invalid_value;
		}

		GeometryData* g = &system_state->geometries[id].geometry;

		if (!Renderer::create_geometry(config, g))
		{
			SHMERROR("Failed to create geometry from config.");
			return GeometryId::invalid_value;
		}

		system_state->geometries[id].reference_count = 1;
		system_state->geometries[id].auto_unload = auto_unload;
		system_state->geometries[id].id = id;
		return id;
	}

	GeometryData* get_geometry_data(GeometryId id)
	{
		if (!id.is_valid() || system_state->geometries[id].id != id)
			return 0;
		else
			return &system_state->geometries[id].geometry;
	}

	void release(GeometryId id)
	{
		if (!id.is_valid() || system_state->geometries[id].id != id)
		{
			SHMFATAL("Geometry id mismatch. Something went horribly wrong...");
			return;
		}
		GeometryReference* ref = &system_state->geometries[id];

		if (ref->reference_count > 0)
			ref->reference_count--;

		if (ref->reference_count < 1 && ref->auto_unload)
			destroy_geometry_reference(ref);
	}

	uint32 get_ref_count(GeometryId id)
	{
		if (!id.is_valid() || system_state->geometries[id].id != id)
			return 0;

		GeometryReference* ref = &system_state->geometries[id];
		return ref->reference_count;
	}

	static void destroy_geometry_reference(GeometryReference* ref)
	{
		Renderer::destroy_geometry(&ref->geometry);
		ref->reference_count = 0;
		ref->id = GeometryId::invalid_value;
		ref->auto_unload = false;
	}
}