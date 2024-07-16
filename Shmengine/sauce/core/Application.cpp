#include "Application.hpp"
#include "platform/Platform.hpp"
#include "ApplicationTypes.hpp"
#include "core/Memory.hpp"
#include "memory/LinearAllocator.hpp"
#include "core/Logging.hpp"

#include "core/Clock.hpp"
#include "core/Event.hpp"
#include "core/Input.hpp"
#include "utility/CString.hpp"

#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/CameraSystem.hpp"
#include "systems/RenderViewSystem.hpp"

// TODO: temp
#include "utility/Math.hpp"
#include "utility/math/Transform.hpp"
#include "utility/String.hpp"

#include "renderer/RendererGeometry.hpp"
// end

namespace Application
{

	struct ApplicationState
	{

		Game* game_inst;
		bool32 is_running;
		bool32 is_suspended;
		uint32 width, height;
		float32 last_time;
		Clock clock;

		Memory::LinearAllocator systems_allocator;

		void* logging_system_state;
		void* input_system_state;
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

		Skybox skybox;

		Darray<Mesh> world_meshes;
		Darray<Mesh> ui_meshes;
	};

	static bool32 initialized = false;
	static ApplicationState* app_state;

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data);
	bool32 on_key(uint16 code, void* sender, void* listener_inst, EventData data);
	bool32 on_resized(uint16 code, void* sender, void* listener_inst, EventData data);

	bool32 event_on_debug_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		const char* names[3] = {
			"cobblestone",
			"paving",
			"paving2"
		};

		static int32 choice = 2;
		const char* old_name = names[choice];
		choice++;
		choice %= 3;

		Geometry* g = app_state->world_meshes[0].geometries[0];
		if (g)
		{
			g->material = MaterialSystem::acquire(names[choice]);
			if (!g->material)
			{
				SHMWARNV("event_on_debug_event - Failed to acquire material '%s'! Using default.", names[choice]);
				g->material = MaterialSystem::get_default_material();
			}

			MaterialSystem::release(old_name);
		}
		
		return true;
	}

	void* allocate_subsystem_callback (uint64 size)
	{
		void* ptr = Memory::linear_allocator_allocate(&app_state->systems_allocator, size);
		Memory::zero_memory(ptr, size);
		return ptr;
	};

	bool32 init_primitive_subsystems(Game* game_inst)
	{

		Memory::SystemConfig mem_config;
		mem_config.total_allocation_size = Gibibytes(1);
		if (!Memory::system_init(mem_config))
		{
			SHMFATAL("Failed to initialize memory subsytem!");
			return false;
		}

		*game_inst = {};
		game_inst->app_state = Memory::allocate(sizeof(ApplicationState), true, AllocationTag::MAIN);
		app_state = (ApplicationState*)game_inst->app_state;

		app_state->game_inst = game_inst;
		app_state->is_running = true;
		app_state->is_suspended = false;
		Memory::linear_allocator_create(Mebibytes(64), &app_state->systems_allocator);

		Platform::init_console();

		if (!Log::system_init(allocate_subsystem_callback, app_state->logging_system_state))
		{
			SHMFATAL("Failed to initialize logging subsytem!");
			return false;
		}

		if (!Input::system_init(allocate_subsystem_callback, app_state->input_system_state))
		{
			SHMFATAL("Failed to initialize input subsystem!");
			return false;
		}

		if (!Event::system_init(allocate_subsystem_callback, app_state->event_system_state))
		{
			SHMFATAL("Failed to initialize event subsystem!");
			return false;
		}

		return true;
	}

	bool32 create(Game* game_inst)
	{

		if (initialized)
			return false;

		game_inst->state = Memory::allocate(game_inst->state_size, true, AllocationTag::MAIN);

		Event::event_register(SystemEventCode::APPLICATION_QUIT, 0, on_event);
		Event::event_register(SystemEventCode::KEY_PRESSED, 0, on_key);
		Event::event_register(SystemEventCode::KEY_RELEASED, 0, on_key);
		Event::event_register(SystemEventCode::WINDOW_RESIZED, 0, on_resized);
		// TODO: temporary
		Event::event_register(SystemEventCode::DEBUG0, 0, event_on_debug_event);
		// end

		if (!Platform::system_init(
			allocate_subsystem_callback,
			app_state->platform_system_state,
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
		resource_sys_config.asset_base_path = "D:/dev/Shmengine/assets/";
		resource_sys_config.max_loader_count = 32;
		if (!ResourceSystem::system_init(allocate_subsystem_callback, app_state->resource_system_state, resource_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize resource system. Application shutting down..");
			return false;
		}

		ShaderSystem::Config shader_sys_config;
		shader_sys_config.max_shader_count = 1024;
		shader_sys_config.max_uniform_count = 128;
		shader_sys_config.max_global_textures = 31;
		shader_sys_config.max_instance_textures = 31;
		if (!ShaderSystem::system_init(allocate_subsystem_callback, app_state->shader_system_state, shader_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize shader system. Application shutting down..");
			return false;
		}

		if (!Renderer::system_init(allocate_subsystem_callback, app_state->renderer_system_state, game_inst->config.name))
		{
			SHMFATAL("ERROR: Failed to initialize renderer. Application shutting down..");
			return false;
		}

		RenderViewSystem::Config render_view_sys_config;
		render_view_sys_config.max_view_count = 251;
		if (!RenderViewSystem::system_init(allocate_subsystem_callback, app_state->texture_system_state, render_view_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize render view system. Application shutting down..");
			return false;
		}

		TextureSystem::Config texture_sys_config;
		texture_sys_config.max_texture_count = 0x10000;
		if (!TextureSystem::system_init(allocate_subsystem_callback, app_state->texture_system_state, texture_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize texture system. Application shutting down..");
			return false;
		}

		MaterialSystem::Config material_sys_config;
		material_sys_config.max_material_count = 0x1000;
		if (!MaterialSystem::system_init(allocate_subsystem_callback, app_state->material_system_state, material_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize material system. Application shutting down..");
			return false;
		}

		GeometrySystem::Config geometry_sys_config;
		geometry_sys_config.max_geometry_count = 0x1000;
		if (!GeometrySystem::system_init(allocate_subsystem_callback, app_state->geometry_system_state, geometry_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize geometry system. Application shutting down..");
			return false;
		}

		CameraSystem::Config camera_sys_config;
		camera_sys_config.max_camera_count = 61;
		if (!CameraSystem::system_init(allocate_subsystem_callback, app_state->camera_system_state, camera_sys_config))
		{
			SHMFATAL("ERROR: Failed to initialize camera system. Application shutting down..");
			return false;
		}

		Renderer::RenderViewConfig skybox_view_config = {};
		skybox_view_config.type = Renderer::RenderViewType::SKYBOX;
		skybox_view_config.width = 0;
		skybox_view_config.height = 0;
		skybox_view_config.name = "skybox";
		skybox_view_config.pass_configs.init(1, 0, AllocationTag::MAIN);
		skybox_view_config.pass_configs[0].name = "Renderpass.Builtin.Skybox";
		skybox_view_config.view_matrix_source = Renderer::RenderViewViewMatrixSource::SCENE_CAMERA;
		if (!RenderViewSystem::create(skybox_view_config))
		{
			SHMFATAL("Failed to create render view!");
			return false;
		}

		Renderer::RenderViewConfig opaque_view_config = {};
		opaque_view_config.type = Renderer::RenderViewType::WORLD;
		opaque_view_config.width = 0;
		opaque_view_config.height = 0;
		opaque_view_config.name = "world_opaque";
		opaque_view_config.pass_configs.init(1, 0, AllocationTag::MAIN);
		opaque_view_config.pass_configs[0].name = "Renderpass.Builtin.World";
		opaque_view_config.view_matrix_source = Renderer::RenderViewViewMatrixSource::SCENE_CAMERA;
		if (!RenderViewSystem::create(opaque_view_config))
		{
			SHMFATAL("Failed to create render view!");
			return false;
		}

		Renderer::RenderViewConfig ui_view_config = {};
		ui_view_config.type = Renderer::RenderViewType::UI;
		ui_view_config.width = 0;
		ui_view_config.height = 0;
		ui_view_config.name = "ui";
		ui_view_config.pass_configs.init(1, 0, AllocationTag::MAIN);
		ui_view_config.pass_configs[0].name = "Renderpass.Builtin.UI";
		ui_view_config.view_matrix_source = Renderer::RenderViewViewMatrixSource::SCENE_CAMERA;
		if (!RenderViewSystem::create(ui_view_config))
		{
			SHMFATAL("Failed to create render view!");
			return false;
		}

		// TODO: temporary

		// Skybox
		TextureMap& cube_map = app_state->skybox.cubemap;
		cube_map.filter_minify = TextureFilter::LINEAR;
		cube_map.filter_magnify = TextureFilter::LINEAR;
		cube_map.repeat_u = cube_map.repeat_v = cube_map.repeat_w = TextureRepeat::CLAMP_TO_EDGE;
		cube_map.use = TextureUse::MAP_CUBEMAP;
		if (!Renderer::texture_map_acquire_resources(&cube_map))
		{
			SHMFATAL("Failed to acquire renderer resources for skybox cube map!");
			return false;
		}
		cube_map.texture = TextureSystem::acquire_cube("skybox", true);
		
		GeometrySystem::GeometryConfig skybox_cube_config = {};
		Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, "skybox_cube", 0, skybox_cube_config);
		skybox_cube_config.material_name[0] = 0;
		app_state->skybox.g = GeometrySystem::acquire_from_config(skybox_cube_config, true);
		app_state->skybox.renderer_frame_number = INVALID_ID64;
		Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);
		TextureMap* maps[1] = { &app_state->skybox.cubemap };
		if (!Renderer::shader_acquire_instance_resources(skybox_shader, maps, &app_state->skybox.instance_id))
		{
			SHMFATAL("Failed to acquire shader instance resources for skybox cube map!");
			return false;
		}

		// Meshes
		app_state->world_meshes.init(5, DarrayFlag::NON_RESIZABLE, AllocationTag::MAIN);

		Mesh* cube_mesh = app_state->world_meshes.push({});
		cube_mesh->geometries.init(1, 0, AllocationTag::MAIN);
		GeometrySystem::GeometryConfig g_config = {};
		Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, "test_cube", "test_material", g_config);
		cube_mesh->geometries.push(GeometrySystem::acquire_from_config(g_config, true));
		cube_mesh->transform = Math::transform_create();

		Mesh* cube_mesh2 = app_state->world_meshes.push({});
		cube_mesh2->geometries.init(1, 0, AllocationTag::MAIN);
		GeometrySystem::GeometryConfig g_config2 = {};
		Renderer::generate_cube_config(5.0f, 5.0f, 5.0f, 1.0f, 1.0f, "test_cube_2", "test_material", g_config2);
		cube_mesh2->geometries.push(GeometrySystem::acquire_from_config(g_config2, true));
		cube_mesh2->transform = Math::transform_from_position({ 10.0f, 0.0f, 1.0f });

		Mesh* cube_mesh3 = app_state->world_meshes.push({});
		cube_mesh3->geometries.init(1, 0, AllocationTag::MAIN);
		GeometrySystem::GeometryConfig g_config3 = {};
		Renderer::generate_cube_config(2.0f, 2.0f, 2.0f, 1.0f, 1.0f, "test_cube_3", "test_material", g_config3);	
		cube_mesh3->geometries.push(GeometrySystem::acquire_from_config(g_config3, true));
		cube_mesh3->transform = Math::transform_from_position({ 15.0f, 0.0f, 1.0f });

		app_state->world_meshes[1].transform.parent = &app_state->world_meshes[0].transform;
		app_state->world_meshes[2].transform.parent = &app_state->world_meshes[0].transform;

		
		Resource car_mesh_res = {};
		if (!ResourceSystem::load("falcon", ResourceType::MESH, 0, &car_mesh_res))
			SHMERROR("Failed to load car mesh!");
		{
			Mesh* car_mesh = app_state->world_meshes.push({});
			GeometrySystem::GeometryConfig* car_g_configs = (GeometrySystem::GeometryConfig*)car_mesh_res.data;
			car_mesh->geometries.init(car_mesh_res.data_size, 0, AllocationTag::MAIN);
			for (uint32 i = 0; i < car_mesh->geometries.size; i++)
			{
				car_mesh->geometries.push(GeometrySystem::acquire_from_config(car_g_configs[i], true));
			}
			car_mesh->transform = Math::transform_from_position({ 20.0f, 0.0f, 1.0f });
			ResourceSystem::unload(&car_mesh_res);
		}
			
		Resource sponza_mesh_res = {};
		if (!ResourceSystem::load("sponza", ResourceType::MESH, 0, &sponza_mesh_res))
			SHMERROR("Failed to load sponza mesh!");
		else
		{
			Mesh* sponza_mesh = app_state->world_meshes.push({});
			GeometrySystem::GeometryConfig* sponza_g_configs = (GeometrySystem::GeometryConfig*)sponza_mesh_res.data;
			sponza_mesh->geometries.init(sponza_mesh_res.data_size, 0, AllocationTag::MAIN);
			for (uint32 i = 0; i < sponza_mesh->geometries.size; i++)
			{
				sponza_mesh->geometries.push(GeometrySystem::acquire_from_config(sponza_g_configs[i], true));
			}
			{
				sponza_mesh->transform = Math::transform_from_position_rotation_scale({15.0f, 0.0f, 1.0f}, QUAT_IDENTITY, {0.1f, 0.1f, 0.1f});
			}
			ResourceSystem::unload(&sponza_mesh_res);
		}	

		// Load up some test UI geometry.
		GeometrySystem::GeometryConfig ui_config = {};
		ui_config.vertex_size = sizeof(Renderer::Vertex2D);
		CString::copy(Material::max_name_length, ui_config.material_name, "test_ui_material");
		CString::copy(Geometry::max_name_length, ui_config.name, "test_ui_geometry");

		ui_config.vertex_count = 4;
		ui_config.vertices.init(ui_config.vertex_size * ui_config.vertex_count, 0, AllocationTag::MAIN);
		ui_config.indices.init(6, 0, AllocationTag::MAIN);
		Renderer::Vertex2D* uiverts = (Renderer::Vertex2D*)&ui_config.vertices[0];

		const float32 w = 1077.0f * 0.25f;
		const float32 h = 1278.0f * 0.25f;
		uiverts[0].position.x = 0.0f;  // 0    3
		uiverts[0].position.y = 0.0f;  //
		uiverts[0].tex_coordinates.x = 0.0f;  //
		uiverts[0].tex_coordinates.y = 0.0f;  // 2    1

		uiverts[1].position.y = h;
		uiverts[1].position.x = w;
		uiverts[1].tex_coordinates.x = 1.0f;
		uiverts[1].tex_coordinates.y = 1.0f;

		uiverts[2].position.x = 0.0f;
		uiverts[2].position.y = h;
		uiverts[2].tex_coordinates.x = 0.0f;
		uiverts[2].tex_coordinates.y = 1.0f;

		uiverts[3].position.x = w;
		uiverts[3].position.y = 0.0;
		uiverts[3].tex_coordinates.x = 1.0f;
		uiverts[3].tex_coordinates.y = 0.0f;

		// Indices - counter-clockwise
		ui_config.indices[0] = 2;
		ui_config.indices[1] = 1;
		ui_config.indices[2] = 0;
		ui_config.indices[3] = 3;
		ui_config.indices[4] = 0;
		ui_config.indices[5] = 1;

		// Get UI geometry from config.
		app_state->ui_meshes.init(1, 0, AllocationTag::MAIN);
		Mesh* ui_mesh = app_state->ui_meshes.push({});
		ui_mesh->geometries.init(1, 0, AllocationTag::MAIN);
		ui_mesh->geometries.push(0);
		ui_mesh->geometries[0] = GeometrySystem::acquire_from_config(ui_config, true);
		ui_mesh->transform = Math::transform_create();
		// end

		if (!game_inst->init(game_inst))
		{
			SHMFATAL("ERROR: Failed to initialize game instance!");
			return false;
		}

		game_inst->on_resize(app_state->game_inst, app_state->width, app_state->height);

		initialized = true;
		return true;

	}

	bool32 run()
	{

		clock_start(&app_state->clock);
		clock_update(&app_state->clock);
		app_state->last_time = app_state->clock.elapsed;

		float32 running_time = 0;
		uint32 frame_count = 0;
		float32 target_frame_seconds = 1.0f / 120.0f;

		Renderer::RenderPacket render_packet = {};
		render_packet.views.init(3, 0, AllocationTag::MAIN);

		while (app_state->is_running)
		{

			clock_update(&app_state->clock);
			float32 current_time = app_state->clock.elapsed;
			float32 delta_time = current_time - app_state->last_time;
			float64 frame_start_time = Platform::get_absolute_time();
			app_state->last_time = current_time;

			if (!Platform::pump_messages())
			{
				app_state->is_running = false;
			}

			Input::frame_start();

			if (!app_state->is_suspended)
			{
				if (!app_state->game_inst->update(app_state->game_inst, delta_time))
				{
					app_state->is_running = false;
					break;
				}

				if (!app_state->game_inst->render(app_state->game_inst, delta_time))
				{
					app_state->is_running = false;
					break;
				}

				render_packet.delta_time = delta_time;

				// TODO: temporary

				Renderer::SkyboxPacketData skybox_data = {};
				skybox_data.skybox = &app_state->skybox;
				if (!RenderViewSystem::build_packet(RenderViewSystem::get("skybox"), &skybox_data, &render_packet.views[0]))
				{
					SHMERROR("Failed to build packet for view 'skybox'.");
					return false;
				}

				if (app_state->world_meshes.count > 0)
				{
					
					Math::Quat rotation = Math::quat_from_axis_angle(VEC3F_UP, 0.5f * delta_time, true);
					Math::transform_rotate(app_state->world_meshes[0].transform, rotation);

					if (app_state->world_meshes.count > 1)
					{
						Math::transform_rotate(app_state->world_meshes[1].transform, rotation);
					}				

					if (app_state->world_meshes.count > 2)
					{
						Math::transform_rotate(app_state->world_meshes[2].transform, rotation);
					}						
					
				}

				Renderer::MeshPacketData world_mesh_data = {};
				world_mesh_data.mesh_count = app_state->world_meshes.count;
				world_mesh_data.meshes = app_state->world_meshes.data;
				if (!RenderViewSystem::build_packet(RenderViewSystem::get("world_opaque"), &world_mesh_data, &render_packet.views[1]))
				{
					SHMERROR("Failed to build packet for view 'world_opaque'.");
					return false;
				}

				Renderer::MeshPacketData ui_mesh_data = {};
				ui_mesh_data.mesh_count = app_state->ui_meshes.count;
				ui_mesh_data.meshes = app_state->ui_meshes.data;
				if (!RenderViewSystem::build_packet(RenderViewSystem::get("ui"), &ui_mesh_data, &render_packet.views[2]))
				{
					SHMERROR("Failed to build packet for view 'ui'.");
					return false;
				}

				Renderer::draw_frame(&render_packet);

				for (uint32 i = 0; i < render_packet.views.count; i++)
				{
					render_packet.views[i].geometries.free_data();
				}
			}

			float64 frame_end_time = Platform::get_absolute_time();
			float64 frame_elapsed_time = frame_end_time - frame_start_time;
			running_time += (float32)frame_elapsed_time;
			float64 remaining_s = target_frame_seconds - frame_elapsed_time;

			if (remaining_s > 0)
			{
				uint32 remaining_ms = (uint32)(remaining_s * 1000);

				bool32 limit_frames = false;
				if (limit_frames)
					Platform::sleep(remaining_ms);
			}
			frame_count++;

			Input::frame_end(delta_time);	
		}

		app_state->is_running = false;

		// temp
		Renderer::texture_map_release_resources(&app_state->skybox.cubemap);
		// end
	
		CameraSystem::system_shutdown();
		GeometrySystem::system_shutdown();
		MaterialSystem::system_shutdown();
		TextureSystem::system_shutdown();
		ShaderSystem::system_shutdown();
		Renderer::system_shutdown();	
		ResourceSystem::system_shutdown();
		Platform::system_shutdown();
		Event::system_shutdown();
		Input::system_shutdown();
		Memory::system_shutdown();
		Log::system_shutdown();	

		return true;

	}

	void get_framebuffer_size(uint32* width, uint32* height)
	{
		*width = app_state->width;
		*height = app_state->height;
	}

	bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		switch (code)
		{
		case SystemEventCode::APPLICATION_QUIT:
		{
			SHMINFO("Application Quit event received. Shutting down.");
			app_state->is_running = false;
			return true;
		}
		}

		return false;
	}

	bool32 on_key(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		if (code == SystemEventCode::KEY_PRESSED)
		{
			uint32 key_code = data.ui32[0];

			switch (key_code)
			{
			case Keys::KEY_ESCAPE:
				Event::event_fire(SystemEventCode::APPLICATION_QUIT, 0, {});
				return true;
				break;
			case Keys::KEY_A:
				SHMDEBUG("A key pressed!");
				break;
			default:
				SHMDEBUGV("'%c' key pressed!", key_code);
				break;
			}
		}
		else if (code == SystemEventCode::KEY_RELEASED)
		{
			uint32 key_code = data.ui32[0];

			switch (key_code)
			{
			case Keys::KEY_B:
				SHMDEBUG("B key released!");
				break;
			default:
				SHMDEBUGV("'%c' key released!", key_code);
				break;
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

			if (width != app_state->width || height != app_state->height)
			{
				app_state->width = width;
				app_state->height = height;

				SHMDEBUGV("Window resize occured: %u, %u", width, height);

				if (!width || !height)
				{
					SHMDEBUG("Window minimized. Suspending application.");
					app_state->is_suspended = true;
					return true;
				}
				else
				{
					if (app_state->is_suspended)
					{
						SHMDEBUG("Window restores. Continuing application.");
						app_state->is_suspended = false;
					}
					app_state->game_inst->on_resize(app_state->game_inst, width, height);
					Renderer::on_resized(width, height);
				}
				
			}
				
		}

		return false;
	}

}