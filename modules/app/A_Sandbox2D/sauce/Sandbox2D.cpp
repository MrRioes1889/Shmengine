#include "Sandbox2D.hpp"
#include "ApplicationState.hpp"
#include "Keybinds.hpp"
#include "ui/DebugConsole.hpp"

#include <containers/Darray.hpp>
#include <core/Logging.hpp>
#include <core/Input.hpp>
#include <core/Event.hpp>
#include <core/Clock.hpp>
#include <core/FrameData.hpp>
#include <core/Identifier.hpp>
#include <utility/Utility.hpp>
#include <utility/String.hpp>
#include <renderer/RendererFrontend.hpp>
#include <utility/Sort.hpp>

// TODO: temp
#include <utility/math/Transform.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <systems/FontSystem.hpp>
#include <systems/ShaderSystem.hpp>
// end

#include <optick.h>

ApplicationState* app_state = 0;

static void register_events();
static void unregister_events();

static bool8 on_debug_event(uint16 code, void* sender, void* listener_inst, EventData e_data);

bool8 application_load_config(ApplicationConfig* out_config)
{
	out_config->app_frame_data_size = sizeof(ApplicationFrameData);
	out_config->state_size = sizeof(ApplicationState);

	out_config->start_pos_x = 100;
	out_config->start_pos_y = 100;
	out_config->start_width = 1600;
	out_config->start_height = 900;
	out_config->name = "Shmengine Sandbox2D";   
	out_config->renderer_module_name = "M_VulkanRenderer";

	out_config->limit_framerate = true;
	
	return true;
}

bool8 application_init(Application* app_inst)
{
	app_state = (ApplicationState*)app_inst->state;

	register_events();
	add_keymaps();

	app_state->allocation_count = 0;

	if (!FontSystem::load_font("Noto Serif 21px", "NotoSerif_21", 21) || 
		!FontSystem::load_font("Roboto Mono 21px", "RobotoMono_21", 21) || 
		!FontSystem::load_font("Martian Mono", "MartianMono", 21))
	{
		SHMERROR("Failed to load default fonts.");
		return false;
	}

	app_state->debug_console.init();

	UITextConfig ui_text_config = {};
	ui_text_config.font_name = "Martian Mono";
	ui_text_config.font_size = 21;
	ui_text_config.text_content = "Some täest text,\n\tyo!";

	if (!ui_text_init(&ui_text_config, &app_state->debug_info_text))
	{
		SHMERROR("Failed to load basic ui truetype text.");
		return false;
	}
	ui_text_set_position(&app_state->debug_info_text, { 500, 550, 0 });

	return true;
}

void application_shutdown()
{
	// TODO: temp
	ui_text_destroy(&app_state->debug_info_text);

	app_state->debug_console.destroy();
	app_state->ui_meshes.free_data();

	unregister_events();
	// end
}

bool8 application_update(FrameData* frame_data)
{
	OPTICK_EVENT();
	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	uint32 allocation_count = Memory::get_current_allocation_count();
	app_state->allocation_count = allocation_count;

	app_state->debug_console.update();

	static float64 last_frametime = 0.0;
	static float64 last_logictime = 0.0;
	static float64 last_rendertime = 0.0;

	static float64 times_update_timer = 0;
	times_update_timer += metrics_last_frametime();
	if (times_update_timer > 1.0)
	{
		last_frametime = metrics_last_frametime();
		last_logictime = metrics_logic_time();
		last_rendertime = metrics_render_time();
		times_update_timer = 0.0;
	}

	Math::Vec2i mouse_pos = Input::get_mouse_position();

	char ui_text_buffer[512];
	CString::safe_print_s<int32, int32, float64, float64, float64>
		(ui_text_buffer, 512, "Mouse Position: [%i, %i]\n\nLast frametime: %lf4 ms\nLogic: %lf4 ms / Render: %lf4 ms",
			mouse_pos.x, mouse_pos.y, last_frametime * 1000.0, last_logictime * 1000.0, last_rendertime * 1000.0);

	ui_text_set_text(&app_state->debug_info_text, ui_text_buffer);

	return true;
}

bool8 application_render(FrameData* frame_data)
{
	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	RenderViewSystem::ui_text_draw(&app_state->debug_info_text, frame_data);

	if (app_state->debug_console.is_visible())
	{
		UIText* console_text = app_state->debug_console.get_text();
		RenderViewSystem::ui_text_draw(console_text, frame_data);

		UIText* entry_text = app_state->debug_console.get_entry_text();
		RenderViewSystem::ui_text_draw(entry_text, frame_data);
	}

	return true;
}

void application_on_resize(uint32 width, uint32 height)
{
	if (!app_state)
		return;

	app_state->width = width;
	app_state->height = height;

	ui_text_set_position(&app_state->debug_info_text, { 20.0f ,(float32)app_state->height - 150.0f, 0.0f });
	return;
}

void application_on_module_reload(void* application_state)
{
	app_state = (ApplicationState*)application_state;

	register_events();
	app_state->debug_console.on_module_reload();
	add_keymaps();
}

void application_on_module_unload()
{
	unregister_events();
	app_state->debug_console.on_module_unload();
	Input::clear_keymaps();
}

static bool8 on_debug_event(uint16 code, void* sender, void* listener_inst, EventData e_data)
{
	if (code == SystemEventCode::KEY_PRESSED)
	{
		uint32 key_code = e_data.ui32[0];
		SHMDEBUGV("Pressed Key. Code: %u", key_code);
	}

	return false;
}

static void register_events()
{
	//Event::event_register(SystemEventCode::KEY_PRESSED, 0, on_debug_event);
	return;
}

static void unregister_events()
{
	//Event::event_unregister(SystemEventCode::KEY_PRESSED, 0, on_debug_event);
	return;
}

