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

#include "core/Subsystems.hpp"

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

		SubsystemManager subsystem_manager;
	};

	static bool32 initialized = false;
	static EngineState* engine_state;

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data);
	bool32 on_resized(uint16 code, void* sender, void* listener_inst, EventData data);

	void* allocate_subsystem_callback (void* allocator, uint64 size)
	{
		Memory::LinearAllocator* lin_allocator = (Memory::LinearAllocator*)allocator;
		void* ptr = lin_allocator->allocate(size);
		return ptr;
	};

	bool32 init(Application* game_inst)
	{

		if (initialized)
			return false;

		game_inst->stage = ApplicationStage::UNINITIALIZED;

		Memory::SystemConfig mem_config;
		mem_config.total_allocation_size = Gibibytes(1);
		if (!Memory::system_init(mem_config))
		{
			SHMFATAL("Failed to initialize memory subsytem!");
			return false;
		}

		game_inst->engine_state = Memory::allocate(sizeof(EngineState), AllocationTag::ENGINE);
		engine_state = (EngineState*)game_inst->engine_state;

		engine_state->game_inst = game_inst;
		engine_state->is_running = true;
		engine_state->is_suspended = false;

		if (!engine_state->subsystem_manager.init(&game_inst->config))
		{
			SHMFATAL("Failed to initialize subsystem manager!");
			return false;
		}

		game_inst->stage = ApplicationStage::BOOTING;
		if (!game_inst->boot(game_inst))
		{
			SHMFATAL("Failed to boot application!");
			return false;
		}
		game_inst->stage = ApplicationStage::BOOT_COMPLETE;

		if (!engine_state->subsystem_manager.post_boot_init(&game_inst->config))
		{
			SHMFATAL("Failed to initialize subsystem manager!");
			return false;
		}

		game_inst->stage = ApplicationStage::INITIALIZING;
		if (!game_inst->init(game_inst))
		{
			SHMFATAL("ERROR: Failed to initialize game instance!");
			return false;
		}
		game_inst->stage = ApplicationStage::INITIALIZED;

		Renderer::on_resized(engine_state->width, engine_state->height);
		game_inst->on_resize(engine_state->game_inst, engine_state->width, engine_state->height);	

		initialized = true;
		return true;

	}

	bool32 run(Application* game_inst)
	{

		uint32 frame_count = 0;
		float64 target_frame_seconds = 1.0f / 240.0f;
		game_inst->stage = ApplicationStage::RUNNING;

		Renderer::RenderPacket render_packet = {};

		float64 last_frametime = 0.0;

		while (engine_state->is_running)
		{

			metrics_update_frame();
			last_frametime = metrics_last_frametime();

			OPTICK_FRAME("MainThread");	

			engine_state->subsystem_manager.update(last_frametime);

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

		game_inst->stage = ApplicationStage::SHUTTING_DOWN;
		engine_state->is_running = false;
		engine_state->game_inst->shutdown(engine_state->game_inst);	

		engine_state->subsystem_manager.shutdown();
		game_inst->stage = ApplicationStage::UNINITIALIZED;

		return true;

	}

	void on_event_system_initialized()
	{
		Event::event_register(SystemEventCode::APPLICATION_QUIT, 0, on_event);
		Event::event_register(SystemEventCode::WINDOW_RESIZED, 0, on_resized);
		Event::event_register(SystemEventCode::OBJECT_HOVER_ID_CHANGED, 0, on_event);
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