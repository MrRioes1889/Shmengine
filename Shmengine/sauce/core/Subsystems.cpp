#include "Subsystems.hpp"

#include "Logging.hpp"

#include "Memory.hpp"
#include "ApplicationTypes.hpp"
#include "Console.hpp"
#include "Event.hpp"
#include "Input.hpp"
#include "platform/Platform.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/CameraSystem.hpp"
#include "systems/FontSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/JobSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/RenderViewSystem.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/TextureSystem.hpp"

static void* allocate_system(void* allocator, uint64 size)
{
	Memory::LinearAllocator* lin_allocator = (Memory::LinearAllocator*)allocator;
	return lin_allocator->allocate(size);
}

bool32 SubsystemManager::init(ApplicationConfig* app_config)
{
	allocator.init(Mebibytes(64));

	return register_known_systems_pre_boot(app_config);
}

bool32 SubsystemManager::post_boot_init(ApplicationConfig* app_config)
{
	return register_known_systems_post_boot(app_config);
}

void SubsystemManager::shutdown()
{
	shutdown_known_systems();
}

bool32 SubsystemManager::update(float64 delta_time)
{
	for (uint32 i = 0; i < SubsystemType::MAX_TYPES_COUNT; ++i) {
		Subsystem* s = &subsystems[i];
		if (s->update) {
			if (!s->update(s->state, delta_time)) {
				SHMERRORV("System update failed for type: %u", i);
			}
		}
	}
	return true;
}

bool32 SubsystemManager::register_system(SubsystemType::Value type, FP_system_init init_callback, FP_system_shutdown shutdown_callback, FP_system_update update_callback, void* config)
{
	
    Subsystem sys;
    sys.init = init_callback;
    sys.shutdown = shutdown_callback;
    sys.update = update_callback;

    if (sys.init) 
	{
        if (!sys.init(allocate_system, &allocator, config)) 
		{
            SHMERROR("Failed to register system - init call failed.");
            return false;
        }
    }
	else if (type != SubsystemType::MEMORY)
	{
		SHMERROR("Initialize is required for types except K_SYSTEM_TYPE_MEMORY.");
		return false;
	}

    subsystems[type] = sys;
    return true;

}

bool32 SubsystemManager::register_known_systems_pre_boot(ApplicationConfig* app_config)
{
	
	if (!register_system(SubsystemType::MEMORY, 0, Memory::system_shutdown, 0, 0))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	if (!register_system(SubsystemType::CONSOLE, Console::system_init, Console::system_shutdown, 0, 0))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	if (!register_system(SubsystemType::LOGGING, Log::system_init, Log::system_shutdown, 0, 0))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	if (!register_system(SubsystemType::INPUT, Input::system_init, Input::system_shutdown, 0, 0))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	if (!register_system(SubsystemType::EVENT, Event::system_init, Event::system_shutdown, 0, 0))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	Platform::SystemConfig platform_sys_config;
	platform_sys_config.application_name = app_config->name;
	platform_sys_config.x = app_config->start_pos_x;
	platform_sys_config.y = app_config->start_pos_y;
	platform_sys_config.width = app_config->start_width;
	platform_sys_config.height = app_config->start_height;

	if (!register_system(SubsystemType::PLATFORM, Platform::system_init, Platform::system_shutdown, 0, &platform_sys_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	ResourceSystem::SystemConfig resource_sys_config;
	CString::copy(MAX_FILEPATH_LENGTH, resource_sys_config.asset_base_path, Platform::get_root_dir());
	CString::append(MAX_FILEPATH_LENGTH, resource_sys_config.asset_base_path, "../../../assets/");
	
	if (!register_system(SubsystemType::RESOURCE_SYSTEM, ResourceSystem::system_init, ResourceSystem::system_shutdown, 0, &resource_sys_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	Renderer::SystemConfig renderer_sys_config;
	renderer_sys_config.application_name = app_config->name;
	renderer_sys_config.flags = 0;
	renderer_sys_config.renderer_module = app_config->renderer_module;
	
	if (!register_system(SubsystemType::RENDERER, Renderer::system_init, Renderer::system_shutdown, 0, &renderer_sys_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	ShaderSystem::SystemConfig shader_sys_config;
	shader_sys_config.max_shader_count = 1024;
	shader_sys_config.max_uniform_count = 128;
	shader_sys_config.max_global_textures = 31;
	shader_sys_config.max_instance_textures = 31;

	if (!register_system(SubsystemType::SHADER_SYSTEM, ShaderSystem::system_init, ShaderSystem::system_shutdown, 0, &shader_sys_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	const int32 max_thread_count = 15;
	int32 thread_count = Platform::get_processor_count() - 1;
	if (thread_count < 1)
	{
		SHMFATAL("Platform reported no additional free threads other than the main one. At least 2 threads needed for job system!");
		return false;
	}
	thread_count = clamp(thread_count, 1, max_thread_count);

	uint32 job_thread_types[max_thread_count];
	for (uint32 i = 0; i < max_thread_count; i++)
		job_thread_types[i] = JobSystem::JobType::GENERAL;

	if (thread_count == 1 || !Renderer::is_multithreaded())
	{
		job_thread_types[0] |= (JobSystem::JobType::GPU_RESOURCE | JobSystem::JobType::RESOURCE_LOAD);
	}
	else
	{
		job_thread_types[0] |= JobSystem::JobType::GPU_RESOURCE;
		job_thread_types[1] |= JobSystem::JobType::RESOURCE_LOAD;
	}

	JobSystem::SystemConfig job_system_config;
	job_system_config.job_thread_count = thread_count;
	job_system_config.type_masks = job_thread_types;

	if (!register_system(SubsystemType::JOB_SYSTEM, JobSystem::system_init, JobSystem::system_shutdown, JobSystem::update, &job_system_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}
	
	return true;

}

bool32 SubsystemManager::register_known_systems_post_boot(ApplicationConfig* app_config)
{

	TextureSystem::SystemConfig texture_sys_config;
	texture_sys_config.max_texture_count = 0x10000;

	if (!register_system(SubsystemType::TEXTURE_SYSTEM, TextureSystem::system_init, TextureSystem::system_shutdown, 0, &texture_sys_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	if (!register_system(SubsystemType::FONT_SYSTEM, FontSystem::system_init, FontSystem::system_shutdown, 0, &app_config->fontsystem_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	CameraSystem::SystemConfig camera_sys_config;
	camera_sys_config.max_camera_count = 61;

	if (!register_system(SubsystemType::CAMERA_SYSTEM, CameraSystem::system_init, CameraSystem::system_shutdown, 0, &camera_sys_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	RenderViewSystem::SystemConfig render_view_sys_config;
	render_view_sys_config.max_view_count = 251;

	if (!register_system(SubsystemType::RENDERVIEW_SYSTEM, RenderViewSystem::system_init, RenderViewSystem::system_shutdown, 0, &render_view_sys_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	for (uint32 i = 0; i < app_config->render_view_configs.capacity; i++)
	{
		if (!RenderViewSystem::create(app_config->render_view_configs[i]))
		{
			SHMFATALV("Failed to create render view: %s", app_config->render_view_configs[i].name);
			return false;
		}
	}

	MaterialSystem::SystemConfig material_sys_config;
	material_sys_config.max_material_count = 0x1000;

	if (!register_system(SubsystemType::MATERIAL_SYSTEM, MaterialSystem::system_init, MaterialSystem::system_shutdown, 0, &material_sys_config))
	{
		SHMFATAL("Failed to register console subsystem!");
		return false;
	}

	GeometrySystem::SystemConfig geometry_sys_config;
	geometry_sys_config.max_geometry_count = 0x1000;
	
	if (!register_system(SubsystemType::GEOMETRY_SYSTEM, GeometrySystem::system_init, GeometrySystem::system_shutdown, 0, &geometry_sys_config))
	{
		SHMFATAL("Failed to register geometry subsystem!");
		return false;
	}

	return true;
}

void SubsystemManager::shutdown_known_systems()
{

	for (int32 i = SubsystemType::KNOWN_TYPES_COUNT - 1; i >= 0; i--)
		subsystems[i].shutdown(subsystems[i].state);

}
