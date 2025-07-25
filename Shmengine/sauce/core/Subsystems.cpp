#include "Subsystems.hpp"

#include "Logging.hpp"

#include "Memory.hpp"
#include "ApplicationTypes.hpp"
#include "Console.hpp"
#include "Event.hpp"
#include "Input.hpp"
#include "FrameData.hpp"
#include "platform/Platform.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/FontSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/JobSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/RenderViewSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/TextureSystem.hpp"

#include "optick.h"

namespace SubsystemManager
{

	struct Subsystem
	{
		uint64 state_size;
		void* state;

		FP_system_init init;
		FP_system_shutdown shutdown;
		FP_system_update update;
	};

	namespace SubsystemType
	{
		enum
		{
			MEMORY,
			CONSOLE,
			LOGGING,
			INPUT,
			EVENT,
			PLATFORM,

			RENDERER,
			SHADER_SYSTEM,
			JOB_SYSTEM,
			RENDERVIEW_SYSTEM,
			TEXTURE_SYSTEM,
			FONT_SYSTEM,	
			MATERIAL_SYSTEM,
			GEOMETRY_SYSTEM,
			KNOWN_TYPES_COUNT,

			MAX_TYPES_COUNT = 128
		};
		typedef uint8 Value;
	}

	struct ManagerState
	{
		Memory::LinearAllocator allocator;
		Subsystem subsystems[SubsystemType::MAX_TYPES_COUNT];
	};

	static ManagerState manager_state = {};

	static bool8 register_system(SubsystemType::Value type, FP_system_init init_callback, FP_system_shutdown shutdown_callback, FP_system_update update_callback, void* config);
	static bool8 register_known_systems_pre_boot();
	static bool8 register_known_systems_post_boot(const ApplicationConfig* app_config);

	static void* allocate_system(void* allocator, uint64 size)
	{
		Memory::LinearAllocator* lin_allocator = (Memory::LinearAllocator*)allocator;
		return lin_allocator->allocate(size);
	}

	bool8 init_basic()
	{
		Memory::SystemConfig mem_config;
		mem_config.total_allocation_size = gibibytes(1);

		if (!register_system(SubsystemType::MEMORY, Memory::system_init, Memory::system_shutdown, 0, &mem_config))
		{
			SHMFATAL("Failed to register memory subsystem!");
			return false;
		}

		uint64 allocator_size = mebibytes(64);
		manager_state.allocator.init(allocator_size);

		return register_known_systems_pre_boot();
	}

	bool8 init_advanced(const ApplicationConfig* app_config)
	{
		return register_known_systems_post_boot(app_config);
	}

	void shutdown_advanced()
	{
		for (int32 i = SubsystemType::GEOMETRY_SYSTEM; i > SubsystemType::EVENT; i--)
			manager_state.subsystems[i].shutdown(manager_state.subsystems[i].state);
	}

	void shutdown_basic()
	{
		for (int32 i = SubsystemType::EVENT; i >= 0; i--)
			manager_state.subsystems[i].shutdown(manager_state.subsystems[i].state);

		manager_state.allocator.destroy();
	}

	bool8 update(const FrameData* frame_data)
	{
		OPTICK_EVENT();
		for (uint32 i = 0; i < SubsystemType::MAX_TYPES_COUNT; ++i) {
			Subsystem* s = &manager_state.subsystems[i];
			if (s->update) {
				if (!s->update(s->state, frame_data)) {
					SHMERRORV("System update failed for type: %u", i);
				}
			}
		}
		return true;
	}

	static bool8 register_system(SubsystemType::Value type, FP_system_init init_callback, FP_system_shutdown shutdown_callback, FP_system_update update_callback, void* config)
	{

		Subsystem sys;
		sys.init = init_callback;
		sys.shutdown = shutdown_callback;
		sys.update = update_callback;

		if (sys.init)
		{
			if (!sys.init(allocate_system, &manager_state.allocator, config))
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

		manager_state.subsystems[type] = sys;
		return true;

	}

	static bool8 register_known_systems_pre_boot()
	{

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

		if (!register_system(SubsystemType::PLATFORM, Platform::system_init, Platform::system_shutdown, 0, 0))
		{
			SHMFATAL("Failed to register console subsystem!");
			return false;
		}

		return true;

	}

	static bool8 register_known_systems_post_boot(const ApplicationConfig* app_config)
	{

		Renderer::SystemConfig renderer_sys_config;
		renderer_sys_config.application_name = app_config->name;
		renderer_sys_config.flags = 0;
		renderer_sys_config.renderer_module_name = app_config->renderer_module_name;

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

		JobSystem::JobTypeFlags::Value job_thread_types[max_thread_count];
		for (uint32 i = 0; i < max_thread_count; i++)
			job_thread_types[i] = JobSystem::JobTypeFlags::General;

		if (thread_count == 1 || !Renderer::is_multithreaded())
		{
			job_thread_types[0] |= (JobSystem::JobTypeFlags::GPUResource | JobSystem::JobTypeFlags::ResourceLoad);
		}
		else
		{
			job_thread_types[0] |= JobSystem::JobTypeFlags::GPUResource;
			job_thread_types[1] |= JobSystem::JobTypeFlags::ResourceLoad;
		}

		JobSystem::SystemConfig job_system_config;
		job_system_config.job_thread_count = thread_count;
		job_system_config.type_flags = job_thread_types;

		if (!register_system(SubsystemType::JOB_SYSTEM, JobSystem::system_init, JobSystem::system_shutdown, JobSystem::update, &job_system_config))
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

		TextureSystem::SystemConfig texture_sys_config;
		texture_sys_config.max_texture_count = 0x10000;

		if (!register_system(SubsystemType::TEXTURE_SYSTEM, TextureSystem::system_init, TextureSystem::system_shutdown, 0, &texture_sys_config))
		{
			SHMFATAL("Failed to register console subsystem!");
			return false;
		}

		FontSystem::SystemConfig font_sys_config;
		font_sys_config.max_font_count = 31;

		if (!register_system(SubsystemType::FONT_SYSTEM, FontSystem::system_init, FontSystem::system_shutdown, 0, &font_sys_config))
		{
			SHMFATAL("Failed to register console subsystem!");
			return false;
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

}
