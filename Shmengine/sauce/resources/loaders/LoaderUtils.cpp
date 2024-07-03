#include "LoaderUtils.hpp"

#include "systems/ResourceSystem.hpp"
#include "core/Logging.hpp"

namespace ResourceSystem
{

    bool32 resource_unload(ResourceLoader* loader, Resource* resource)
    {
        if (resource->loader_id != loader->id)
        {
            SHMWARN("resource_unload - Cannot unload resource since it seems to belong to another loader!");
            return false;
        }

        if (resource->data)
            Memory::free_memory(resource->data, true, resource->allocation_tag);

            
        resource->full_path.free_data();
        resource->data = 0;
        resource->data_size = 0;
        resource->loader_id = INVALID_ID;
        return true;
    }

}
