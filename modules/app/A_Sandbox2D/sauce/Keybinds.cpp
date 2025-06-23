#include "Keybinds.hpp"

#include "ApplicationState.hpp"
#include "ui/DebugConsole.hpp"

#include <core/Event.hpp>
#include <core/Logging.hpp>
#include <core/Input.hpp>
#include <core/Keymap.hpp>
#include <core/FrameData.hpp>

#include <systems/RenderViewSystem.hpp>

extern ApplicationState* app_state;

static void on_escape(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	SHMDEBUG("Closing Application.");
	Event::event_fire(SystemEventCode::APPLICATION_QUIT, 0, {});
}

static void on_clip_cursor(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	SHMDEBUG("Clipping/Unclipping cursor!");
	Input::clip_cursor();
}

static void on_allocation_count_check(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	static uint32 total_allocation_count = 0;
	uint32 cur_allocation_count = Memory::get_current_allocation_count();
	SHMDEBUGV("Memory Stats: Current Allocation Count: %u, Since last check: %u", cur_allocation_count, cur_allocation_count - total_allocation_count);
	total_allocation_count = cur_allocation_count;
}

static void on_console_show(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	DebugConsole* console = (DebugConsole*)user_data;
	if(console->is_visible())
		return;

	console->set_visible(true);
	Input::push_keymap(&console->keymap);
}

void add_keymaps()
{

	Input::Keymap global_keymap;
	global_keymap.init();
	global_keymap.add_binding(KeyCode::Escape, Input::KeymapBindingType::Press, 0, 0, on_escape);
	global_keymap.add_binding(KeyCode::T, Input::KeymapBindingType::Press, 0, &app_state->debug_console, on_console_show);

	Input::push_keymap(&global_keymap);

	app_state->debug_console.setup_keymap();

}

