#include "LoaderUtils.hpp"

#include "systems/ResourceSystem.hpp"
#include "core/Logging.hpp"

namespace ResourceSystem
{

    bool32 resource_unload(ResourceLoader* loader, Resource* resource, AllocationTag tag)
    {
        if (resource->loader_id != loader->id)
        {
            SHMWARN("resource_unload - Cannot unload resource since it seems to belong to another loader!");
            return false;
        }

        if (resource->data)
            Memory::free_memory(resource->data, true, AllocationTag::MAIN);
        if (resource->full_path)
            Memory::free_memory(resource->full_path, true, AllocationTag::MAIN);

        resource->data = 0;
        resource->data_size = 0;
        resource->full_path = 0;
        resource->loader_id = INVALID_OBJECT_ID;
        return true;
    }

}
