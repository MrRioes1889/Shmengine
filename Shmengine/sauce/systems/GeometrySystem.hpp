#pragma once

#include "utility/MathTypes.hpp"
#include "core/Subsystems.hpp"
#include "core/Identifier.hpp"
#include "containers/Sarray.hpp"
#include "renderer/RendererTypes.hpp"

struct Material;
struct GeometryConfig;
struct GeometryData;

namespace GeometrySystem
{

	struct SystemConfig
	{
		uint32 max_geometry_count;

		inline static const char* default_name = "default";
	};

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI GeometryId create_geometry(GeometryConfig* config, bool8 auto_destroy);
	SHMAPI GeometryId acquire_reference(GeometryId id);
	SHMAPI GeometryData* get_geometry_data(GeometryId id);
	SHMAPI void release(GeometryId id);
	SHMAPI uint32 get_ref_count(GeometryId id);

}