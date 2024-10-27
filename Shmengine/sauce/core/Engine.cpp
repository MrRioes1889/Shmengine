#include "Engine.hpp"
#include "platform/Platform.hpp"
#include "ApplicationTypes.hpp"
#include "core/Memory.hpp"
#include "memory/LinearAllocator.hpp"
#include "core/Logging.hpp"

#include "core/Clock.hpp"
#include "core/Event.hpp"
#include "core/Input.hpp"
#include "core/Console.hpp"
#include "utility/CString.hpp"

#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/CameraSystem.hpp"
#include "systems/RenderViewSystem.hpp"
#include "systems/JobSystem.hpp"
#include "systems/FontSystem.hpp"

#include "optick.h"

namespace Engine
{

	struct EngineState
	{

		Application* game_inst;
		bool32 is_running;
		bool32 is_suspended;
		uint32 width, height;
		//Clock clock;

		Memory::LinearAllocator systems_allocator;

		void* logging_system_state;
		void* input_system_state;
		void* console_state;
		void* event_system_state;
		void* platform_system_state;
		void* resource_system_state;
		void* shader_system_state;
		void* renderer_system_state;
		void* render_view_system_state;
		void* texture_system_state;
		void* material_system_state;
		void* geometry_system_state;
		void* camera_system_state;
		void* job_system_state;
		void* font_system_state;
	};

	static bool32 initialized = false;
	static EngineState* engine_state;

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data);
	bool32 on_resized(uint16 code, void* sender, void* listener_inst, EventData data);

	void* allocate_subsystem_callback (uint64 size)
	{
		void* ptr = Memory::linear_allocator_allocate(&engine_state->systems_allocator, size);
		Memory::zero_memory(ptr, size);
		return ptr;
	};

	bool32 init_primitive_subsystems(Application* game_inst)
	{

		Memory::SystemConfig mem_config;
		mem_config.total_allocation_size = Gibibytes(1);
		if (!Memory::system_init(mem_config))
		{
			SHMFATAL("Failed to initialize memory subsytem!");
			return false;
		}

		*game_inst = {};
		game_inst->app_state = Memory::allocate(sizeof(EngineState), AllocationTag::ENGINE);
		engine_state = (EngineState*)game_inst->app_state;

		engine_state->game_inst = game_inst;
		engine_state->is_running = true;
		engine_state->is_suspended = false;
		Memory::linear_allocator_create(Mebibytes(64), &engine_state->systems_allocator);

		Platform::init_console();

		if (!Console::system_init(allocate_subsystem_callback, engine_state->console_state))
		{
			SHMFATAL("Failed to initialize console subsystem!");
			return false;
		}

		if (!Log::system_init(allocate_subsystem_callback, engine_state->logging_system_state))
		{
			SHMFATAL("Failed to initialize logging subsytem!");
			return false;
		}

		if (!Input::system_init(allocate_subsystem_callback, engine_state->input_system_state))
		{
			SHMFATAL("Failed to initialize input subsystem!");
			return false;
		}

		if (!Event::system_init(allocate_subsystem_callback, engine_state->event_system_state))
		{
			SHMFATAL("Failed to initialize event subsystem!");
			return false;
		}

		return true;
	}

	bool32 create(Application* game_inst)
	{

		if (initialized)
			return false;

		game_inst->state = Memory::allocate(game_inst->state_size, AllocationTag::ENGINE);

		Event::event_register(SystemEventCode::APPLICATION_QUIT, 0, on_event);
		Event::event_register(SystemEventCode::WINDOW_RESIZED, 0, on_resized);
		Event::event_register(SystemEventCode::OBJECT_HOVER_ID_CHANGED, 0, on_event);

		if (!Platform::system_init(
			allocate_subsystem_callback,
			engine_state->platform_system_state,
			game_inst->config.name,
			game_inst->config.start_pos_x,
			game_inst->config.start_pos_y,
			game_inst->config.start_width,
			game_inst->config.start_height))
		{
			SHMFATAL("ERROR: Failed to startup platform layer!");
			return false;
		}

		ResourceSystem::Config resource_sys_config;
		resource_sys_config.asset_base_path = "../assets/";
		resource_sys_config.max_loader_count = 32;
		if (!ResourceSystem::system_init(allocate_subsystem_callback, engine_state->resource_system_state, resource_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize resource system. Application shutting down..");
			return false;
		}

		ShaderSystem::Config shader_sys_config;
		shader_sys_config.max_shader_count = 1024;
		shader_sys_config.max_uniform_count = 128;
		shader_sys_config.max_global_textures = 31;
		shader_sys_config.max_instance_textures = 31;
		if (!ShaderSystem::system_init(allocate_subsystem_callback, engine_state->shader_system_state, shader_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize shader system. Application shutting down..");
			return false;
		}

		if (!Renderer::system_init(allocate_subsystem_callback, engine_state->renderer_system_state, game_inst->config.name))
		{
			SHMFATAL("ERROR: Failed to initialize renderer. Application shutting down..");
			return false;
		}

		if (!game_inst->boot(game_inst))
		{
			SHMFATAL("Failed to boot application!");
			return false;
		}

		RenderViewSystem::Config render_view_sys_config;
		render_view_sys_config.max_view_count = 251;
		if (!RenderViewSystem::system_init(allocate_subsystem_callback, engine_state->texture_system_state, render_view_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize render view system. Application shutting down..");
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

		JobSystem::Config job_system_config;
		job_system_config.job_thread_count = thread_count;
		if (!JobSystem::system_init(allocate_subsystem_callback, engine_state->job_system_state, job_system_config, job_thread_types))
		{
			SHMFATAL("ERROR: Failed to initialize job system. Application shutting down..");
			return false;
		}

		TextureSystem::Config texture_sys_config;
		texture_sys_config.max_texture_count = 0x10000;
		if (!TextureSystem::system_init(allocate_subsystem_callback, engine_state->texture_system_state, texture_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize texture system. Application shutting down..");
			return false;
		}

		if (!FontSystem::system_init(allocate_subsystem_callback, engine_state->font_system_state, engine_state->game_inst->config.fontsystem_config))
		{
			SHMFATAL("ERROR: Failed to initialize font system. Application shutting down..");
			return false;
		}

		CameraSystem::Config camera_sys_config;
		camera_sys_config.max_camera_count = 61;
		if (!CameraSystem::system_init(allocate_subsystem_callback, engine_state->camera_system_state, camera_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize camera system. Application shutting down..");
			return false;
		}

		for (uint32 i = 0; i < game_inst->config.render_view_configs.capacity; i++)
		{
			if (!RenderViewSystem::create(game_inst->config.render_view_configs[i]))
			{
				SHMFATALV("Failed to create render view: %s", game_inst->config.render_view_configs[i].name);
				return false;
			}
		}

		MaterialSystem::Config material_sys_config;
		material_sys_config.max_material_count = 0x1000;
		if (!MaterialSystem::system_init(allocate_subsystem_callback, engine_state->material_system_state, material_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize material system. Application shutting down..");
			return false;
		}

		GeometrySystem::Config geometry_sys_config;
		geometry_sys_config.max_geometry_count = 0x1000;
		if (!GeometrySystem::system_init(allocate_subsystem_callback, engine_state->geometry_system_state, geometry_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize geometry system. Application shutting down..");
			return false;
		}

		if (!game_inst->init(game_inst))
		{
			SHMFATAL("ERROR: Failed to initialize game instance!");
			return false;
		}

		Renderer::on_resized(engine_state->width, engine_state->height);
		game_inst->on_resize(engine_state->game_inst, engine_state->width, engine_state->height);	

		initialized = true;
		return true;

	}

	bool32 run()
	{

		uint32 frame_count = 0;
		float64 target_frame_seconds = 1.0f / 240.0f;

		Renderer::RenderPacket render_packet = {};

		float64 last_frametime = 0.0;

		while (engine_state->is_running)
		{

			metrics_update_frame();
			last_frametime = metrics_last_frametime();

			OPTICK_FRAME("MainThread");	

			JobSystem::update();

			if (!Platform::pump_messages())
			{
				engine_state->is_running = false;
			}

			Input::frame_start();

			if (!engine_state->is_suspended)
			{

				if (!engine_state->game_inst->update(engine_state->game_inst, last_frametime))
				{
					SHMFATAL("Failed to update application.");
					engine_state->is_running = false;
					break;
				}

				metrics_update_logic();

				render_packet.delta_time = last_frametime;
				if (!engine_state->game_inst->render(engine_state->game_inst, &render_packet, last_frametime))
				{
					SHMFATAL("Failed to render application.");
					engine_state->is_running = false;
					break;
				}		

				Renderer::draw_frame(&render_packet);

				for (uint32 i = 0; i < render_packet.views.capacity; i++)
				{
					render_packet.views[i].view->on_destroy_packet(render_packet.views[i].view, &render_packet.views[i]);
				}

				metrics_update_render();

			}

			Input::frame_end(last_frametime);

			float64 frame_elapsed_time = metrics_mid_frame_time();
			float64 remaining_s = target_frame_seconds - frame_elapsed_time;

			bool32 limit_frames = false;
			if (remaining_s > 0 && limit_frames)
			{
				uint32 remaining_ms = (uint32)(remaining_s * 1000);			
				Platform::sleep(remaining_ms);

				while (frame_elapsed_time < target_frame_seconds)
					frame_elapsed_time = metrics_mid_frame_time();
			}

			frame_count++;			
		}

		engine_state->is_running = false;
		engine_state->game_inst->shutdown(engine_state->game_inst);
	
		GeometrySystem::system_shutdown();
		MaterialSystem::system_shutdown();
		RenderViewSystem::system_shutdown();
		CameraSystem::system_shutdown();
		FontSystem::system_shutdown();	
		TextureSystem::system_shutdown();
		JobSystem::system_shutdown();
		ShaderSystem::system_shutdown();
		Renderer::system_shutdown();	
		ResourceSystem::system_shutdown();
		Platform::system_shutdown();
		Event::system_shutdown();	
		Input::system_shutdown();
		Memory::system_shutdown();
		Log::system_shutdown();	
		Console::system_shutdown();

		return true;

	}

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		switch (code)
		{
		case SystemEventCode::APPLICATION_QUIT:
		{
			SHMINFO("Application Quit event received. Shutting down.");
			engine_state->is_running = false;
			return true;
		}
		}

		return false;
	}

	bool32 on_resized(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		if (code == SystemEventCode::WINDOW_RESIZED)
		{
			uint32 width = data.ui32[0];
			uint32 height = data.ui32[1];

			if (width != engine_state->width || height != engine_state->height)
			{
				engine_state->width = width;
				engine_state->height = height;

				SHMDEBUGV("Window resize occured: %u, %u", width, height);

				if (!width || !height)
				{
					SHMDEBUG("Window minimized. Suspending application.");
					engine_state->is_suspended = true;
					return true;
				}
				else
				{
					if (engine_state->is_suspended)
					{
						SHMDEBUG("Window restores. Continuing application.");
						engine_state->is_suspended = false;
					}
					engine_state->game_inst->on_resize(engine_state->game_inst, width, height);
					Renderer::on_resized(width, height);
				}
				
			}
				
		}

		return false;
	}

}