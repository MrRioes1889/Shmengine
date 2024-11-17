#include "DebugConsole.hpp"

#include <utility/String.hpp>
#include <core/Input.hpp>
#include <core/Event.hpp>
#include <core/Console.hpp>

namespace DebugConsole
{

	static void command_exit(Console::CommandContext context);

	static bool32 consumer_write(void* inst, Log::LogLevel log_level, const char* message)
	{

		DebugConsole::ConsoleState* console_state = (DebugConsole::ConsoleState*)inst;

		Darray<String> parts;
		CString::split(message, parts, '\n');
		for (uint32 i = 0; i < parts.count; i++)
			console_state->lines.push_steal(parts[i]);
		
		parts.free_data();
		console_state->dirty = true;
		return true;

	}

	static bool32 on_key(uint16 code, void* sender, void* listener_inst, EventData data)
	{

		DebugConsole::ConsoleState* console_state = (DebugConsole::ConsoleState*)listener_inst;

		if (!console_state->visible)
			return false;

		if (code != SystemEventCode::KEY_PRESSED)
			return false;

		uint16 key_code = data.ui16[0];
		bool8 shift_held = Input::is_key_down(KeyCode::SHIFT) || Input::is_key_down(KeyCode::LSHIFT) || Input::is_key_down(KeyCode::RSHIFT);
		bool8 alt_gr_held = Input::is_key_down(KeyCode::CONTROL) && Input::is_key_down(KeyCode::ALT);

		if (key_code == KeyCode::ENTER)
		{
			if (console_state->entry_control.text.len() <= console_state->entry_prefix.len() + 1)
				return false;

			console_state->entry_control.text.pop();
			if (!Console::execute_command(console_state->entry_control.text.c_str() + console_state->entry_prefix.len()))
			{
			
			}

			console_state->entry_control.text = console_state->entry_prefix;
			console_state->entry_control.text.append('_');
			ui_text_refresh(&console_state->entry_control);
		}
		if (key_code == KeyCode::BACKSPACE)
		{
			if (console_state->entry_control.text.len() <= console_state->entry_prefix.len() + 1)
				return false;

			console_state->entry_control.text.pop();
			console_state->entry_control.text[console_state->entry_control.text.len() - 1] = '_';
			ui_text_refresh(&console_state->entry_control);
		}
		else
		{
			char char_code = (char)key_code;
			if (key_code >= KeyCode::A && key_code <= KeyCode::Z)
			{		
				if (!shift_held)
					char_code += 32;
			}
			else if (shift_held)
			{
				switch (key_code)
				{
				case KeyCode::NUM_0:		  char_code = '='; break;
				case KeyCode::NUM_1:		  char_code = '!'; break;
				case KeyCode::NUM_2:		  char_code = '"'; break;
				case KeyCode::NUM_3:		  char_code = ' '; break;
				case KeyCode::NUM_4:		  char_code = '$'; break;
				case KeyCode::NUM_5:		  char_code = '%'; break;
				case KeyCode::NUM_6:		  char_code = '&'; break;
				case KeyCode::NUM_7:		  char_code = '/'; break;
				case KeyCode::NUM_8:		  char_code = '('; break;
				case KeyCode::NUM_9:		  char_code = ')'; break;
				case KeyCode::DOT:			  char_code = ':'; break;
				case KeyCode::COMMA:		  char_code = ';'; break;
				case KeyCode::MINUS:		  char_code = '_'; break;
				case KeyCode::PLUS:			  char_code = '*'; break;
				case KeyCode::QUESTION_MARK:  char_code = '?'; break;
				case KeyCode::POUND:		  char_code = '\''; break;
				default: char_code = 0; break;
				}
			}
			else if (alt_gr_held)
			{
				switch (key_code)
				{
				case KeyCode::NUM_0: char_code = '}'; break;
				case KeyCode::NUM_7: char_code = '{'; break;
				case KeyCode::NUM_8: char_code = '['; break;
				case KeyCode::NUM_9: char_code = ']'; break;
				default: char_code = 0; break;
				}
			}
			else
			{
				switch (key_code)
				{
				case KeyCode::SPACE: char_code = ' '; break;
				case KeyCode::MINUS: char_code = '-'; break;
				case KeyCode::PLUS: char_code = '+'; break;
				case KeyCode::DOT: char_code = '.'; break;
				case KeyCode::COMMA: char_code = ','; break;
				case KeyCode::POUND: char_code = '#'; break;
				default: char_code = 0; break;
				}
			}

			if (!char_code)
				return false;

			console_state->entry_control.text[console_state->entry_control.text.len() - 1] = char_code;
			console_state->entry_control.text.append('_');
			ui_text_refresh(&console_state->entry_control);
		}
		
		return false;

	}


	void init(ConsoleState* console_state)
	{

		console_state->lines.init(16, 0);

		console_state->line_display_count = 10;
		console_state->line_offset = 0;
		console_state->visible = false;

		console_state->entry_prefix = "--> ";

		Console::register_consumer(console_state, consumer_write, &console_state->consumer_id);

		Console::register_command("exit", 0, command_exit);
		Console::register_command("quit", 0, command_exit);

		Event::event_register(SystemEventCode::KEY_PRESSED, console_state, on_key);
		Event::event_register(SystemEventCode::KEY_RELEASED, console_state, on_key);

	}

	void destroy(ConsoleState* console_state)
	{

		unload(console_state);
		console_state->lines.free_data();						

	}

	bool32 load(ConsoleState* console_state)
	{

		if (!ui_text_create(UITextType::TRUETYPE, "Martian Mono", 21, "", &console_state->text_control))
		{
			SHMERROR("Failed to load basic ui truetype text.");
			return false;
		}
		ui_text_set_position(&console_state->text_control, { 3.0f, 30.0f, 0 });

		if (!ui_text_create(UITextType::TRUETYPE, "Martian Mono", 21, "", &console_state->entry_control))
		{
			SHMERROR("Failed to load basic ui truetype text.");
			return false;
		}
		ui_text_set_position(&console_state->entry_control, { 3.0f, 30.0f + (console_state->line_display_count * 21.0f), 0});
		String entry_prefix(console_state->entry_prefix);
		entry_prefix.append('_');
		ui_text_set_text(&console_state->entry_control, entry_prefix.c_str());

		console_state->loaded = true;
		return true;

	}

	void unload(ConsoleState* console_state)
	{
		if (!console_state->loaded)
			return;

		ui_text_destroy(&console_state->text_control);
		ui_text_destroy(&console_state->entry_control);
	}

	void update(ConsoleState* console_state)
	{

		if (!console_state->dirty)
			return;

		uint32 line_count = console_state->lines.count;
		uint32 max_lines_shown_count = console_state->line_display_count;

		uint32 min_line = (uint32)SHMAX((int32)line_count - (int32)max_lines_shown_count - console_state->line_offset, 0);
		uint32 max_line = min_line + SHMIN(max_lines_shown_count, line_count) - 1;

		static char buffer[16384];
		uint32 buffer_offset = 0;
		for (uint32 i = min_line; i <= max_line; ++i) {
			// TODO: insert colour codes for the message type.		
			uint32 copy_length = CString::copy(sizeof(buffer) - buffer_offset, &buffer[buffer_offset], console_state->lines[i].c_str());
			buffer[buffer_offset + copy_length] = '\n';
			buffer_offset += copy_length + 1;
		}

		buffer[buffer_offset] = 0;
		ui_text_set_text(&console_state->text_control, buffer);

		console_state->dirty = false;

	}

	UIText* get_text(ConsoleState* console_state)
	{
		return &console_state->text_control;
	}

	UIText* get_entry_text(ConsoleState* console_state)
	{
		return &console_state->entry_control;
	}

	bool32 is_visible(ConsoleState* console_state)
	{
		return console_state->visible;
	}

	void set_visible(ConsoleState* console_state, bool8 flag)
	{
		console_state->visible = flag;
	}

	void scroll_up(ConsoleState* console_state)
	{
		console_state->dirty = true;
		
		if (console_state->lines.count <= console_state->line_display_count)
		{
			console_state->line_offset = 0;
			return;
		}

		console_state->line_offset++;
		console_state->line_offset = SHMIN(console_state->line_offset, (int32)console_state->lines.count - (int32)console_state->line_display_count);
	}

	void scroll_down(ConsoleState* console_state)
	{
		if (!console_state->line_offset)
			return;

		console_state->dirty = true;

		if (console_state->lines.count <= console_state->line_display_count)
		{
			console_state->line_offset = 0;
			return;
		}

		console_state->line_offset--;
		console_state->line_offset = SHMAX(console_state->line_offset, 0);
	}

	void scroll_to_top(ConsoleState* console_state)
	{
		console_state->dirty = true;

		if (console_state->lines.count <= console_state->line_display_count)
		{
			console_state->line_offset = 0;
			return;
		}

		console_state->line_offset = console_state->lines.count - console_state->lines.count;
	}

	void scroll_to_bottom(ConsoleState* console_state)
	{
		console_state->dirty = true;
		console_state->line_offset = 0;
	}

	void on_module_reload(ConsoleState* console_state)
	{
		Console::register_consumer(console_state, consumer_write, &console_state->consumer_id);

		Console::register_command("exit", 0, command_exit);
		Console::register_command("quit", 0, command_exit);

		Event::event_register(SystemEventCode::KEY_PRESSED, console_state, on_key);
		Event::event_register(SystemEventCode::KEY_RELEASED, console_state, on_key);
	}

	void on_module_unload(ConsoleState* console_state)
	{
		Console::unregister_consumer(console_state->consumer_id);

		Console::unregister_command("exit");
		Console::unregister_command("quit");

		Event::event_unregister(SystemEventCode::KEY_PRESSED, console_state, on_key);
		Event::event_unregister(SystemEventCode::KEY_RELEASED, console_state, on_key);
	}

	static void command_exit(Console::CommandContext context)
	{
		SHMDEBUG("game exit called!");
		Event::event_fire(SystemEventCode::APPLICATION_QUIT, 0, {});
	}

}