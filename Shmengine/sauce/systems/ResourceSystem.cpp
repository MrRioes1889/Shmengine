#include "ResourceSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"

#include "resources/loaders/GenericLoader.hpp"
#include "resources/loaders/ImageLoader.hpp"
#include "resources/loaders/MaterialLoader.hpp"


namespace ResourceSystem
{

	struct SystemState
	{
		SystemConfig config;
	};

	static SystemState* system_state = 0;

    bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
    {

        SystemConfig* sys_config = (SystemConfig*)config;
        system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

        system_state->config = *sys_config;

        SHMINFOV("Resource system initialized with base path: %s", sys_config->asset_base_path);

        return true;

    }

    void system_shutdown(void* state)
    {
        system_state = 0;
    }

    const char* get_base_path()
    {
        return system_state->config.asset_base_path;
    }

}