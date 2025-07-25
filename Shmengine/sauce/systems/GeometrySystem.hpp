#pragma once

#include "utility/MathTypes.hpp"
#include "core/Subsystems.hpp"
#include "core/Identifier.hpp"
#include "containers/Sarray.hpp"

struct Material;
struct GeometryConfig;
struct GeometryData;

namespace GeometrySystem
{
	typedef Id16 GeometryId;

	struct SystemConfig
	{
		uint32 max_geometry_count;

		inline static const char* default_name = "default";
	};

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI GeometryId acquire_by_id(GeometryId id);
	SHMAPI GeometryId acquire_from_config(GeometryConfig* config, bool8 auto_unload);
	SHMAPI GeometryData* get_geometry_data(GeometryId id);
	SHMAPI void release(GeometryId id);
	SHMAPI uint32 get_ref_count(GeometryId id);

}