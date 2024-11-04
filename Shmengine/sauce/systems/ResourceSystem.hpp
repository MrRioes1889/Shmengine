#pragma once

#include "resources/ResourceTypes.hpp"
#include "core/Subsystems.hpp"

namespace ResourceSystem
{

	struct SystemConfig
	{
		uint32 max_loader_count;
		char asset_base_path[MAX_FILEPATH_LENGTH];
	};

	struct ResourceLoader
	{
		uint32 id;
		ResourceType type;
		const char* custom_type;
		const char* type_path;
		bool32(*load)(ResourceLoader* self, const char* name, void* params, Resource* out_resource);
		void(*unload)(ResourceLoader* self, Resource* resource);
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI bool32 register_loader(ResourceLoader loader);

	SHMAPI bool32 load(const char* name, ResourceType type, void* params, Resource* out_resource);
	SHMAPI bool32 load_custom(const char* name, const char* custom_type, void* params, Resource* out_resource);

	SHMAPI void unload(Resource* resource);

	SHMAPI const char* get_base_path();

}