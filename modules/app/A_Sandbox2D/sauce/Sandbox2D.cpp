#include "Sandbox2D.hpp"
#include "ApplicationState.hpp"
#include "Keybinds.hpp"
#include "ui/DebugConsole.hpp"

#include "views/RenderViewCanvas.hpp"
#include "views/RenderViewUI.hpp"

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
#include <renderer/RendererGeometry.hpp>
#include <utility/math/Transform.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/GeometrySystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <systems/FontSystem.hpp>
#include <systems/ShaderSystem.hpp>
// end

ApplicationState* app_state = 0;

static bool32 init_render_views(Application* app_inst);

static void register_events();
static void unregister_events();

static bool32 on_debug_event(uint16 code, void* sender, void* listener_inst, EventData e_data);

bool32 application_load_config(ApplicationConfig* out_config)
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

bool32 application_init(Application* app_inst)
{
	app_state = (ApplicationState*)app_inst->state;

	if (!init_render_views(app_inst))
	{
		SHMFATAL("Failed to initialize render views!");
		return false;
	}

	register_events();
	add_keymaps();

	app_state->allocation_count = 0;

	FontSystem::BitmapFontConfig bitmap_font_configs[2] = {};
	bitmap_font_configs[0].name = "Noto Serif 21px";
	bitmap_font_configs[0].resource_name = "NotoSerif_21";
	bitmap_font_configs[0].size = 21;

	bitmap_font_configs[1].name = "Roboto Mono 21px";
	bitmap_font_configs[1].resource_name = "RobotoMono_21";
	bitmap_font_configs[1].size = 21;

	FontSystem::TruetypeFontConfig truetype_font_configs[1] = {};
	truetype_font_configs[0].name = "Martian Mono";
	truetype_font_configs[0].resource_name = "MartianMono";
	truetype_font_configs[0].default_size = 21;

	if (!FontSystem::load_bitmap_font(bitmap_font_configs[0]) || !FontSystem::load_bitmap_font(bitmap_font_configs[1]) || !FontSystem::load_truetype_font(truetype_font_configs[0]))
	{
		SHMERROR("Failed to load default fonts.");
		return false;
	}

	app_state->debug_console.init();
	app_state->debug_console.load();

	UITextConfig ui_text_config = {};
	ui_text_config.type = UITextType::TRUETYPE;
	ui_text_config.font_name = "Martian Mono";
	ui_text_config.font_size = 21;
	ui_text_config.text_content = "Some täest text,\n\tyo!";

	if (!ui_text_init(&ui_text_config, &app_state->debug_info_text) || !ui_text_load(&app_state->debug_info_text))
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

bool32 application_update(FrameData* frame_data)
{
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
		(ui_text_buffer, 512, "Mouse Position: [%i, %i]\n\nLast frametime: %lf4 ms\nLogic: %lf4 / Render: %lf4",
			mouse_pos.x, mouse_pos.y, last_frametime * 1000.0, last_logictime * 1000.0, last_rendertime * 1000.0);

	ui_text_set_text(&app_state->debug_info_text, ui_text_buffer);
	ui_text_update(&app_state->debug_info_text);

	return true;
}

bool32 application_render(Renderer::RenderPacket* packet, FrameData* frame_data)
{
	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	const uint32 view_count = 1;
	RenderView** render_views = (RenderView**)frame_data->frame_allocator.allocate(view_count * sizeof(RenderView*));
	packet->views.init(view_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, render_views);

	uint32 ui_shader_id = ShaderSystem::get_ui_shader_id();

	//uint32 canvas_view_i = packet->views.emplace(RenderViewSystem::get("canvas"));
	uint32 ui_view_i = packet->views.emplace(RenderViewSystem::get("ui"));

	RenderViewSystem::ui_text_draw(packet->views[ui_view_i], &app_state->debug_info_text, ui_shader_id, frame_data);

	if (app_state->debug_console.is_visible())
	{
		UIText* console_text = app_state->debug_console.get_text();
		RenderViewSystem::ui_text_draw(packet->views[ui_view_i], console_text, ui_shader_id, frame_data);

		UIText* entry_text = app_state->debug_console.get_entry_text();
		RenderViewSystem::ui_text_draw(packet->views[ui_view_i], entry_text, ui_shader_id, frame_data);
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

static bool32 init_render_views(Application* app_inst)
{
	app_inst->render_views.init(Sandbox2DRenderViews::VIEW_COUNT, 0);

	{
		RenderView* canvas_view = &app_inst->render_views[Sandbox2DRenderViews::CANVAS];
		canvas_view->width = 0;
		canvas_view->height = 0;
		canvas_view->name = "canvas";

		canvas_view->on_build_packet = render_view_canvas_on_build_packet;
		canvas_view->on_end_frame = render_view_canvas_on_end_frame;
		canvas_view->on_render = render_view_canvas_on_render;
		canvas_view->on_register = render_view_canvas_on_register;
		canvas_view->on_unregister = render_view_canvas_on_unregister;
		canvas_view->on_resize = render_view_canvas_on_resize;
		canvas_view->regenerate_attachment_target = 0;

		const uint32 canvas_pass_count = 1;
		Renderer::RenderPassConfig canvas_pass_configs[canvas_pass_count];

		Renderer::RenderPassConfig* canvas_pass_config = &canvas_pass_configs[0];
		canvas_pass_config->name = "Builtin.Canvas";
		canvas_pass_config->dim = { app_inst->main_window->client_width, app_inst->main_window->client_height };
		canvas_pass_config->offset = { 0, 0 };
		canvas_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
		canvas_pass_config->clear_flags = Renderer::RenderpassClearFlags::COLOR_BUFFER;
		canvas_pass_config->depth = 1.0f;
		canvas_pass_config->stencil = 0;

		const uint32 canvas_target_att_count = 1;
		Renderer::RenderTargetAttachmentConfig canvas_att_configs[canvas_target_att_count];
		canvas_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
		canvas_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
		canvas_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
		canvas_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
		canvas_att_configs[0].present_after = false;

		canvas_pass_config->target_config.attachment_count = canvas_target_att_count;
		canvas_pass_config->target_config.attachment_configs = canvas_att_configs;
		canvas_pass_config->render_target_count = Renderer::get_window_attachment_count();

		RenderViewSystem::register_view(canvas_view, canvas_pass_count, canvas_pass_configs);
	}

	{
		RenderView* ui_view = &app_inst->render_views[Sandbox2DRenderViews::UI];
		ui_view->width = 0;
		ui_view->height = 0;
		ui_view->name = "ui";

		ui_view->on_build_packet = render_view_ui_on_build_packet;
		ui_view->on_end_frame = render_view_ui_on_end_frame;
		ui_view->on_render = render_view_ui_on_render;
		ui_view->on_register = render_view_ui_on_register;
		ui_view->on_unregister = render_view_ui_on_unregister;
		ui_view->on_resize = render_view_ui_on_resize;
		ui_view->regenerate_attachment_target = 0;

		const uint32 ui_pass_count = 1;
		Renderer::RenderPassConfig ui_pass_configs[ui_pass_count];

		Renderer::RenderPassConfig* ui_pass_config = &ui_pass_configs[0];
		ui_pass_config->name = "Builtin.UI";
		ui_pass_config->dim = { app_inst->main_window->client_width, app_inst->main_window->client_height };
		ui_pass_config->offset = { 0, 0 };
		ui_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
		ui_pass_config->clear_flags = Renderer::RenderpassClearFlags::COLOR_BUFFER;
		ui_pass_config->depth = 1.0f;
		ui_pass_config->stencil = 0;

		const uint32 ui_target_att_count = 1;
		Renderer::RenderTargetAttachmentConfig ui_att_configs[ui_target_att_count];
		ui_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
		ui_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
		ui_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
		ui_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
		ui_att_configs[0].present_after = true;

		ui_pass_config->target_config.attachment_count = ui_target_att_count;
		ui_pass_config->target_config.attachment_configs = ui_att_configs;
		ui_pass_config->render_target_count = Renderer::get_window_attachment_count();

		RenderViewSystem::register_view(ui_view, ui_pass_count, ui_pass_configs);
	}

	return true;
}

static bool32 on_debug_event(uint16 code, void* sender, void* listener_inst, EventData e_data)
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

