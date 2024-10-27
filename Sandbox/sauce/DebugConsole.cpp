#include "DebugConsole.hpp"

#include <utility/String.hpp>
#include <resources/UIText.hpp>
#include <core/Input.hpp>
#include <core/Event.hpp>
#include <core/Console.hpp>

namespace DebugConsole
{

	struct DConsole
	{

		uint32 line_display_count;
		int32 line_offset;

		Darray<String> lines;

		bool8 dirty;
		bool8 visible;
		bool8 loaded;

		UIText text_control;
		UIText entry_control;

		static const char* entry_prefix;
		static uint32 entry_prefix_length;

	};

	const char* DConsole::entry_prefix = "--> ";
	uint32 DConsole::entry_prefix_length = sizeof("--> ") - 1;

	static DConsole* console_state = 0;

	static void command_exit(Console::CommandContext context);

	static bool32 consumer_write(void* inst, Log::LogLevel log_level, const char* message)
	{

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

		if (!console_state->visible)
			return false;

		if (code != SystemEventCode::KEY_PRESSED)
			return false;

		uint16 key_code = data.ui16[0];
		bool8 shift_held = Input::is_key_down(KeyCode::SHIFT) || Input::is_key_down(KeyCode::LSHIFT) || Input::is_key_down(KeyCode::RSHIFT);
		bool8 alt_gr_held = Input::is_key_down(KeyCode::CONTROL) && Input::is_key_down(KeyCode::ALT);

		if (key_code == KeyCode::ENTER)
		{
			if (console_state->entry_control.text.len() <= DConsole::entry_prefix_length + 1)
				return false;

			console_state->entry_control.text.pop();
			if (!Console::execute_command(console_state->entry_control.text.c_str() + DConsole::entry_prefix_length))
			{
			
			}

			console_state->entry_control.text = console_state->entry_prefix;
			console_state->entry_control.text.append('_');
			ui_text_refresh(&console_state->entry_control);
		}
		if (key_code == KeyCode::BACKSPACE)
		{
			if (console_state->entry_control.text.len() <= DConsole::entry_prefix_length + 1)
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
				case KeyCode::NUM_3:		  char_code = '§'; break;
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


	void init()
	{

		console_state = (DConsole*)Memory::allocate(sizeof(DConsole), AllocationTag::UNKNOWN);
		console_state->lines.init(16, 0);

		console_state->line_display_count = 10;
		console_state->line_offset = 0;
		console_state->visible = false;

		Console::register_consumer(0, consumer_write);

		Console::register_command("exit", 0, command_exit);
		Console::register_command("quit", 0, command_exit);

	}

	void destroy()
	{

		unload();
		console_state->lines.free_data();

	}

	bool32 load()
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

		Event::event_register(SystemEventCode::KEY_PRESSED, 0, on_key);
		Event::event_register(SystemEventCode::KEY_RELEASED, 0, on_key);

		console_state->loaded = true;
		return true;

	}

	void unload()
	{
		if (!console_state->loaded)
			return;

		ui_text_destroy(&console_state->text_control);
		ui_text_destroy(&console_state->entry_control);

		Event::event_unregister(SystemEventCode::KEY_PRESSED, 0, on_key);
		Event::event_unregister(SystemEventCode::KEY_RELEASED, 0, on_key);
	}

	void update()
	{

		if (!console_state->dirty)
			return;

		uint32 line_count = console_state->lines.count;
		uint32 max_lines_count = console_state->line_display_count;

		uint32 min_line = (uint32)SHMAX((int32)line_count - (int32)max_lines_count - console_state->line_offset, 0);
		uint32 max_line = min_line + max_lines_count - 1;

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

	UIText* get_text()
	{
		return &console_state->text_control;
	}

	UIText* get_entry_text()
	{
		return &console_state->entry_control;
	}

	bool32 is_visible()
	{
		return console_state->visible;
	}

	void set_visible(bool8 flag)
	{
		console_state->visible = flag;
	}

	void scroll_up()
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

	void scroll_down()
	{
		console_state->dirty = true;

		if (console_state->lines.count <= console_state->line_display_count)
		{
			console_state->line_offset = 0;
			return;
		}

		console_state->line_offset--;
		console_state->line_offset = SHMAX(console_state->line_offset, 0);
	}

	void scroll_to_top()
	{
		console_state->dirty = true;

		if (console_state->lines.count <= console_state->line_display_count)
		{
			console_state->line_offset = 0;
			return;
		}

		console_state->line_offset = console_state->lines.count - console_state->lines.count;
	}

	void scroll_to_bottom()
	{
		console_state->dirty = true;
		console_state->line_offset = 0;
	}

	static void command_exit(Console::CommandContext context)
	{
		SHMDEBUG("game exit called!");
		Event::event_fire(SystemEventCode::APPLICATION_QUIT, 0, {});
	}


}