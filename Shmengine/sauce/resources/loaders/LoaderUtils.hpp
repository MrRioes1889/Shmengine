#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "resources/ResourceTypes.hpp"

namespace ResourceSystem
{
	struct ResourceLoader;

	bool32 resource_unload(ResourceLoader* loader, Resource* resource, AllocationTag tag);
}
