#include "Engine.hpp"
#include "platform/Platform.hpp"
#include "ApplicationTypes.hpp"
#include "core/Memory.hpp"
#include "core/FrameData.hpp"
#include "memory/LinearAllocator.hpp"
#include "core/Logging.hpp"

#include "core/Clock.hpp"
#include "core/Event.hpp"
#include "core/Input.hpp"
#include "core/Console.hpp"
#include "utility/CString.hpp"

#include "renderer/RendererFrontend.hpp"

#include "core/Subsystems.hpp"

#include "optick.h"

namespace Engine
{

	struct EngineState
	{

		Application* app_inst;
		bool32 is_running;
		bool32 is_suspended;
		uint32 width, height;
		//Clock clock;

		Memory::LinearAllocator systems_allocator;

		FrameData frame_data;
		Memory::LinearAllocator frame_allocator;

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
	static EngineState* engine_state = 0;

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data);
	bool32 on_resized(uint16 code, void* sender, void* listener_inst, EventData data);

	static void destroy_application_config(ApplicationConfig* config);

	void* allocate_subsystem_callback (void* allocator, uint64 size)
	{
		Memory::LinearAllocator* lin_allocator = (Memory::LinearAllocator*)allocator;
		void* ptr = lin_allocator->allocate(size);
		return ptr;
	};

	bool32 init(Application* app_inst)
	{

		if (initialized)
			return false;

		app_inst->stage = ApplicationStage::UNINITIALIZED;

		Memory::SystemConfig mem_config;
		mem_config.total_allocation_size = Gibibytes(1);
		if (!Memory::system_init(mem_config))
		{
			SHMFATAL("Failed to initialize memory subsytem!");
			return false;
		}

		app_inst->engine_state = Memory::allocate(sizeof(EngineState), AllocationTag::ENGINE);
		engine_state = (EngineState*)app_inst->engine_state;

		engine_state->app_inst = app_inst;
		engine_state->is_running = true;
		engine_state->is_suspended = false;

		if (!SubsystemManager::init_basic(&app_inst->config))
		{
			SHMFATAL("Failed to initialize subsystem manager!");
			return false;
		}

		app_inst->stage = ApplicationStage::BOOTING;
		if (!app_inst->boot(app_inst))
		{
			SHMFATAL("Failed to boot application!");
			return false;
		}
		app_inst->stage = ApplicationStage::BOOT_COMPLETE;

		if (!SubsystemManager::init_advanced(&app_inst->config))
		{
			SHMFATAL("Failed to initialize subsystem manager!");
			return false;
		}

		if (app_inst->config.state_size)
			app_inst->state = Memory::allocate(app_inst->config.state_size, AllocationTag::APPLICATION);

		engine_state->frame_data.frame_allocator = &engine_state->frame_allocator;
		void* f_data = Memory::allocate(app_inst->config.frame_allocator_size, AllocationTag::ENGINE);
		engine_state->frame_data.frame_allocator->init(app_inst->config.frame_allocator_size, f_data);

		if (app_inst->config.app_frame_data_size)
			engine_state->frame_data.app_data = Memory::allocate(app_inst->config.app_frame_data_size, AllocationTag::APPLICATION);

		destroy_application_config(&app_inst->config);

		app_inst->stage = ApplicationStage::INITIALIZING;
		if (!app_inst->init(app_inst->state))
		{
			SHMFATAL("ERROR: Failed to initialize game instance!");
			return false;
		}
		app_inst->stage = ApplicationStage::INITIALIZED;	

		Renderer::on_resized(engine_state->width, engine_state->height);
		app_inst->on_resize(engine_state->width, engine_state->height);	

		initialized = true;
		return true;

	}

	void destroy_application_config(ApplicationConfig* config)
	{
		config->bitmap_font_configs.free_data();
		config->truetype_font_configs.free_data();

		for (uint32 i = 0; i < config->render_view_configs.capacity; i++)
		{
			Renderer::RenderViewConfig* view_config = &config->render_view_configs[i];
			for (uint32 j = 0; j < view_config->pass_configs.capacity; j++)
			{
				Renderer::RenderpassConfig* pass_config = &view_config->pass_configs[j];
				pass_config->target_config.attachment_configs.free_data();
			}
			view_config->pass_configs.free_data();
		}
		config->render_view_configs.free_data();
	}

	bool32 run(Application* app_inst)
	{

		uint32 frame_count = 0;
		float64 target_frame_seconds = 1.0f / 240.0f;
		app_inst->stage = ApplicationStage::RUNNING;

		Renderer::RenderPacket render_packet = {};

		float64 last_frametime = 0.0;

		while (engine_state->is_running)
		{
			metrics_update_frame();
			last_frametime = metrics_last_frametime();

			engine_state->frame_data.delta_time = last_frametime;
			engine_state->frame_data.total_time += last_frametime;

			OPTICK_FRAME("MainThread");

			engine_state->frame_allocator.free_all_data();

			SubsystemManager::update(&engine_state->frame_data);

			if (!Platform::pump_messages())
			{
				engine_state->is_running = false;
			}

			Input::frame_start();

			if (!engine_state->is_suspended)
			{

				if (!engine_state->app_inst->update(&engine_state->frame_data))
				{
					SHMFATAL("Failed to update application.");
					engine_state->is_running = false;
					break;
				}

				metrics_update_logic();

				if (!engine_state->app_inst->render(&render_packet, &engine_state->frame_data))
				{
					SHMFATAL("Failed to render application.");
					engine_state->is_running = false;
					break;
				}		

				Renderer::draw_frame(&render_packet, &engine_state->frame_data);

				for (uint32 i = 0; i < render_packet.views.capacity; i++)
				{
					render_packet.views[i].view->on_destroy_packet(render_packet.views[i].view, &render_packet.views[i]);
				}

				metrics_update_render();

			}

			Input::frame_end(&engine_state->frame_data);

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

		app_inst->stage = ApplicationStage::SHUTTING_DOWN;
		engine_state->is_running = false;
		engine_state->app_inst->shutdown();	

		SubsystemManager::shutdown_advanced();

		app_inst->on_module_unload();
		Platform::unload_dynamic_library(&app_inst->application_lib);
		Platform::unload_dynamic_library(&app_inst->renderer_lib);

		SubsystemManager::shutdown_basic();

		app_inst->stage = ApplicationStage::UNINITIALIZED;		

		return true;

	}

	void on_event_system_initialized()
	{
		Event::event_register(SystemEventCode::APPLICATION_QUIT, 0, on_event);
		Event::event_register(SystemEventCode::WINDOW_RESIZED, 0, on_resized);
		Event::event_register(SystemEventCode::OBJECT_HOVER_ID_CHANGED, 0, on_event);
	}

	float64 get_frame_delta_time()
	{
		return engine_state->frame_data.delta_time;
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
					engine_state->app_inst->on_resize(width, height);
					Renderer::on_resized(width, height);
				}
				
			}
				
		}

		return false;
	}

}