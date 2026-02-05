#include "DebugConsole.hpp"

#include <utility/String.hpp>
#include <core/Input.hpp>
#include <core/Event.hpp>
#include <core/Console.hpp>
#include <core/Keymap.hpp>
#include <core/Engine.hpp>

static void command_exit(Console::CommandContext context);
static bool8 consumer_write(void* inst, Log::LogLevel log_level, const char* message);
static void update_displayed_console_text(DebugConsole* console);

bool8 DebugConsole::init()
{
	state = ResourceState::Initializing;
	Memory::zero_memory(line_lenghts, lines_limit * sizeof(uint16));
	console_text.reserve(lines_limit * 128);
	CString::copy("--> ", entry_text_prefix, sizeof(entry_text_prefix));

	lines_display_limit = 10;
	lines_display_offset = 0;
	visible = false;
	held_key = 0;

	UITextConfig ui_text_config = {};
	ui_text_config.font_name = "Martian Mono";
	ui_text_config.font_size = 21;
	ui_text_config.text_content = "";

	if (!ui_text_init(&ui_text_config, &text_control))
	{
		SHMERROR("Failed to load basic ui truetype text.");
		return false;
	}
	ui_text_set_position(&text_control, { 3.0f, 30.0f, 0 });

	if (!ui_text_init(&ui_text_config, &entry_control))
	{
		SHMERROR("Failed to load basic ui truetype text.");
		return false;
	}
	ui_text_set_position(&entry_control, { 3.0f, 30.0f + (lines_display_limit * 21.0f), 0 });

	Console::register_consumer(this, consumer_write, &consumer_id);

	Console::register_command("exit", 0, command_exit);
	Console::register_command("quit", 0, command_exit);

	entry_text = entry_text_prefix;
	entry_text.append('_');
	ui_text_set_text(&entry_control, entry_text.c_str());

	state = ResourceState::Initialized;
	return true;
}

void DebugConsole::destroy()
{
	if (state != ResourceState::Initialized)
		return;

	ui_text_destroy(&text_control);
	ui_text_destroy(&entry_control);

	console_text.free_data();
	state = ResourceState::Destroyed;
}

void DebugConsole::update()
{
}

UIText* DebugConsole::get_text()
{
	return &text_control;
}

UIText* DebugConsole::get_entry_text()
{
	return &entry_control;
}

bool8 DebugConsole::is_visible()
{
	return visible;
}

void DebugConsole::set_visible(bool8 flag)
{
	visible = flag;
}

static void update_displayed_console_text(DebugConsole* console)
{
	int32 text_length = 0;
	for (uint32 i = 0; i < console->lines_display_limit && i < console->lines_count; i++)
		text_length += console->line_lenghts[console->lines_display_offset + i];

	ui_text_set_text(&console->text_control, console->console_text.c_str(), (uint32)console->text_display_offset, text_length);
}

void DebugConsole::scroll_down()
{
	if (lines_display_offset >= lines_count - lines_display_limit)
		return;
	
	text_display_offset += line_lenghts[lines_display_offset];
	lines_display_offset++;
	update_displayed_console_text(this);
}

void DebugConsole::scroll_up()
{
	if (!lines_display_offset)
		return;

	lines_display_offset--;
	text_display_offset -= line_lenghts[lines_display_offset];
	update_displayed_console_text(this);
}

void DebugConsole::scroll_to_bottom()
{
	if (lines_count <= lines_display_limit)
	{
		lines_display_offset = 0;
		text_display_offset = 0;
	}
	else
	{
		lines_display_offset = lines_count - lines_display_limit;
		text_display_offset = console_text.len();
		for (uint32 i = 0; i < lines_display_limit; i++)
			text_display_offset -= line_lenghts[lines_count - 1 - i];
	}
	update_displayed_console_text(this);
}

void DebugConsole::scroll_to_top()
{
	lines_display_offset = 0;
	text_display_offset = 0;
	update_displayed_console_text(this);
}

void DebugConsole::on_module_reload()
{
	Console::register_consumer(this, consumer_write, &consumer_id);

	Console::register_command("exit", 0, command_exit);
	Console::register_command("quit", 0, command_exit);

	setup_keymap();
}

void DebugConsole::on_module_unload()
{
	Console::unregister_consumer(consumer_id);

	Console::unregister_command("exit");
	Console::unregister_command("quit");
}

static SHMINLINE void console_add_line(DebugConsole* console, uint16 message_length)
{
	uint32 line_remove_count = SHMAX(console->lines_limit / 20, 1);

	if (console->lines_count + 1 > console->lines_limit)
	{
		uint32 remove_length = 0;
		for (uint32 i = 0; i < line_remove_count; i++)
			remove_length += console->line_lenghts[i];

		console->console_text.mid(remove_length);
		Memory::copy_memory(&console->line_lenghts[line_remove_count], console->line_lenghts, line_remove_count * sizeof(uint16));
		console->lines_count -= line_remove_count;
		console->lines_display_offset = (uint32)SHMIN((int32)(console->lines_display_offset - line_remove_count), 0);
		console->text_display_offset = (uint64)SHMIN((int64)(console->text_display_offset - remove_length), 0);
	}

	console->line_lenghts[console->lines_count] = message_length;
	console->lines_count++;
	if (console->lines_count > console->lines_display_limit)
		console->lines_display_offset++;
}

static bool8 consumer_write(void* inst, Log::LogLevel log_level, const char* message)
{
	DebugConsole* console = (DebugConsole*)inst;
	if (console->state < ResourceState::Initialized)
		return true;

	uint16 message_length = 0;
	const char* c = message;
	while (*c)
	{
		message_length++;

		if (*c == '\n')
		{
			console_add_line(console, message_length);
			message_length = 0;
		}

		c++;
	}

	if (message_length)
		console_add_line(console, message_length + 1);

	console->console_text.append(message);
	if (console->console_text.last() != '\n')
		console->console_text.append('\n');

	console->scroll_to_bottom();
	return true;
}

static char get_mapped_char(KeyCode::Value key_code, bool8 shift_held, bool8 alt_held, bool8 ctrl_held)
{
	char char_code = 0;
	if (key_code >= KeyCode::A && key_code <= KeyCode::Z)
	{		
		if (!shift_held)
			char_code = key_code + 32;
	}
	else if (shift_held)
	{
		switch (key_code)
		{
		case KeyCode::Num_0:		  char_code = ')'; break;
		case KeyCode::Num_1:		  char_code = '!'; break;
		case KeyCode::Num_2:		  char_code = '@'; break;
		case KeyCode::Num_3:		  char_code = '#'; break;
		case KeyCode::Num_4:		  char_code = '$'; break;
		case KeyCode::Num_5:		  char_code = '%'; break;
		case KeyCode::Num_6:		  char_code = '^'; break;
		case KeyCode::Num_7:		  char_code = '&'; break;
		case KeyCode::Num_8:		  char_code = '*'; break;
		case KeyCode::Num_9:		  char_code = '('; break;
		case KeyCode::Dot:			  char_code = '>'; break;
		case KeyCode::Comma:		  char_code = '<'; break;
		case KeyCode::Minus:		  char_code = '_'; break;
		case KeyCode::Equals:	      char_code = '+'; break;
		case KeyCode::Backslash:	  char_code = '|'; break;
		case KeyCode::BracketOpening: char_code = '{'; break;
		case KeyCode::BracketClosing: char_code = '}'; break;
		default: char_code = 0; break;
		}
	}
	else
	{
		switch (key_code)
		{
		case KeyCode::Space: char_code = ' '; break;
		case KeyCode::Minus: char_code = '-'; break;
		case KeyCode::Dot: char_code = '.'; break;
		case KeyCode::Comma: char_code = ','; break;
		case KeyCode::Slash: char_code = '/'; break;
		case KeyCode::BracketOpening: char_code = '['; break;
		case KeyCode::BracketClosing: char_code = ']'; break;
		case KeyCode::Equals: char_code = '='; break;
		case KeyCode::Backslash: char_code = '\\'; break;
		default: char_code = 0; break;
		}
	}

	return char_code;
}

static void on_key(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	DebugConsole* console = (DebugConsole*)user_data;

	if (!console->visible || type != Input::KeymapBindingType::Press)
		return;

	KeyCode::Value key_code = key;
	bool8 shift_held = Input::is_key_down(KeyCode::Shift) || Input::is_key_down(KeyCode::LShift) || Input::is_key_down(KeyCode::RShift);
	bool8 alt_held = Input::is_key_down(KeyCode::Alt) || Input::is_key_down(KeyCode::LAlt) || Input::is_key_down(KeyCode::RAlt);
	bool8 ctrl_held = Input::is_key_down(KeyCode::Control) || Input::is_key_down(KeyCode::LControl) || Input::is_key_down(KeyCode::RControl);

	if (key_code == KeyCode::Enter)
	{
		if (console->entry_text.len() <= sizeof(console->entry_text_prefix))
			return;

		console->entry_text.pop();
		Console::execute_command(console->entry_text.c_str() + sizeof(console->entry_text_prefix) - 1);

		console->entry_text = console->entry_text_prefix;
		console->entry_text.append('_');
		ui_text_set_text(&console->entry_control, console->entry_text.c_str());
	}
	if (key_code == KeyCode::Backspace)
	{
		if (console->entry_text.len() <= sizeof(console->entry_text_prefix))
			return;

		console->entry_text.pop();
		console->entry_text[console->entry_text.len() - 1] = '_';
		ui_text_set_text(&console->entry_control, console->entry_text.c_str());
	}
	else
	{
		char char_code = get_mapped_char(key_code, shift_held, alt_held, ctrl_held);
		if (!char_code)
			return;

		console->entry_text[console->entry_text.len() - 1] = char_code;
		console->entry_text.append('_');
		ui_text_set_text(&console->entry_control, console->entry_text.c_str());
	}
	
}

static void on_key_hold(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	DebugConsole* console = (DebugConsole*)user_data;

	if (!console->visible)
		return;

	static float64 repeat_start_timer = 0.0;
	if (type == Input::KeymapBindingType::Release)
	{
		console->held_key = 0;
		repeat_start_timer = 0.0;
		return;
	}
	else if (!console->held_key)
	{
		console->held_key = key;
	}
	else if (key != console->held_key)
	{
		return;
	}

	float64 delta_time = Engine::get_frame_delta_time();
	repeat_start_timer += delta_time;
	if (repeat_start_timer < 0.5)
		return;

	KeyCode::Value key_code = console->held_key;
	bool8 shift_held = Input::is_key_down(KeyCode::Shift) || Input::is_key_down(KeyCode::LShift) || Input::is_key_down(KeyCode::RShift);
	bool8 alt_held = Input::is_key_down(KeyCode::Alt) || Input::is_key_down(KeyCode::LAlt) || Input::is_key_down(KeyCode::RAlt);
	bool8 ctrl_held = Input::is_key_down(KeyCode::Control) || Input::is_key_down(KeyCode::LControl) || Input::is_key_down(KeyCode::RControl);

	if (key_code == KeyCode::Backspace)
	{
		if (console->entry_text.len() <= sizeof(console->entry_text_prefix))
			return;

		console->entry_text.pop();
		console->entry_text[console->entry_text.len() - 1] = '_';
		ui_text_set_text(&console->entry_control, console->entry_text.c_str());
	}
	else
	{
		char char_code = get_mapped_char(key_code, shift_held, alt_held, ctrl_held);
		if (!char_code)
			return;

		console->entry_text[console->entry_text.len() - 1] = char_code;
		console->entry_text.append('_');
		ui_text_set_text(&console->entry_control, console->entry_text.c_str());
	}
}

static void on_console_scroll(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	DebugConsole* console = (DebugConsole*)user_data;
	if (key == KeyCode::Up)
		console->scroll_up();
	else
		console->scroll_down();
}

static void on_console_scroll_hold(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	DebugConsole* console = (DebugConsole*)user_data;
	static float64 accumulated_time = 0.0f;

	float64 delta_time = Engine::get_frame_delta_time();
	accumulated_time += delta_time;
	if (accumulated_time < 0.1f)
		return;

	if (key == KeyCode::Up)
		console->scroll_up();
	else
		console->scroll_down();

	accumulated_time = 0.0f;
}

static void on_console_hide(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	DebugConsole* console = (DebugConsole*)user_data;
	if(!console->is_visible())
		return;

	console->set_visible(false);
	Input::pop_keymap();
}

void DebugConsole::setup_keymap()
{
	keymap.init();
	keymap.overrides_all = true;

	keymap.add_binding(KeyCode::Escape, Input::KeymapBindingType::Press, 0, this, on_console_hide);

	keymap.add_binding(KeyCode::Up, Input::KeymapBindingType::Press, 0, this, on_console_scroll);
	keymap.add_binding(KeyCode::Down, Input::KeymapBindingType::Press, 0, this, on_console_scroll);
	keymap.add_binding(KeyCode::Up, Input::KeymapBindingType::Hold, 0, this, on_console_scroll_hold);
	keymap.add_binding(KeyCode::Down, Input::KeymapBindingType::Hold, 0, this, on_console_scroll_hold);

	keymap.add_binding(KeyCode::Enter, Input::KeymapBindingType::Press, 0, this, on_key);

	for (KeyCode::Value code = KeyCode::A; code <= KeyCode::Z; code++)
		keymap.add_binding(code, Input::KeymapBindingType::Press, 0, this, on_key);
	for (KeyCode::Value code = KeyCode::Num_0; code <= KeyCode::Num_9; code++)
		keymap.add_binding(code, Input::KeymapBindingType::Press, 0, this, on_key);

	keymap.add_binding(KeyCode::Backspace, Input::KeymapBindingType::Press, 0, this, on_key);
	keymap.add_binding(KeyCode::Space, Input::KeymapBindingType::Press, 0, this, on_key);
	keymap.add_binding(KeyCode::Minus, Input::KeymapBindingType::Press, 0, this, on_key);
	keymap.add_binding(KeyCode::Dot, Input::KeymapBindingType::Press, 0, this, on_key);
	keymap.add_binding(KeyCode::Comma, Input::KeymapBindingType::Press, 0, this, on_key);
	keymap.add_binding(KeyCode::Slash, Input::KeymapBindingType::Press, 0, this, on_key);
	keymap.add_binding(KeyCode::BracketOpening, Input::KeymapBindingType::Press, 0, this, on_key);
	keymap.add_binding(KeyCode::BracketClosing, Input::KeymapBindingType::Press, 0, this, on_key);
	keymap.add_binding(KeyCode::Equals, Input::KeymapBindingType::Press, 0, this, on_key);
	keymap.add_binding(KeyCode::Backslash, Input::KeymapBindingType::Press, 0, this, on_key);

	for (KeyCode::Value code = KeyCode::A; code <= KeyCode::Z; code++)
		keymap.add_binding(code, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	for (KeyCode::Value code = KeyCode::Num_0; code <= KeyCode::Num_9; code++)
		keymap.add_binding(code, Input::KeymapBindingType::Hold, 0, this, on_key_hold);

	keymap.add_binding(KeyCode::Backspace, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Space, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Minus, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Dot, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Comma, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Slash, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::BracketOpening, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::BracketClosing, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Equals, Input::KeymapBindingType::Hold, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Backslash, Input::KeymapBindingType::Hold, 0, this, on_key_hold);

	for (KeyCode::Value code = KeyCode::A; code <= KeyCode::Z; code++)
		keymap.add_binding(code, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	for (KeyCode::Value code = KeyCode::Num_0; code <= KeyCode::Num_9; code++)
		keymap.add_binding(code, Input::KeymapBindingType::Release, 0, this, on_key_hold);

	keymap.add_binding(KeyCode::Backspace, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Space, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Minus, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Dot, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Comma, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Slash, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::BracketOpening, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::BracketClosing, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Equals, Input::KeymapBindingType::Release, 0, this, on_key_hold);
	keymap.add_binding(KeyCode::Backslash, Input::KeymapBindingType::Release, 0, this, on_key_hold);

	if (is_visible())
		Input::push_keymap(&keymap);
}
	
static void command_exit(Console::CommandContext context)
{
	SHMDEBUG("game exit called!");
	Event::event_fire(SystemEventCode::APPLICATION_QUIT, 0, {});
}
