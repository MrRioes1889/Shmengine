#pragma once

#include "Defines.hpp"
#include <resources/UIText.hpp>

namespace DebugConsole
{

	struct ConsoleState
	{
		uint32 line_display_count;
		int32 line_offset;

		bool8 dirty;
		bool8 visible;
		bool8 loaded;

		uint32 consumer_id;

		Darray<String> lines;	

		UIText text_control;
		UIText entry_control;

		String entry_prefix;
	};

	void init(ConsoleState* console_state);
	void destroy(ConsoleState* console_state);

	bool32 load(ConsoleState* console_state);
	void unload(ConsoleState* console_state);

	void update(ConsoleState* console_state);

	UIText* get_text(ConsoleState* console_state);
	UIText* get_entry_text(ConsoleState* console_state);

	bool32 is_visible(ConsoleState* console_state);
	void set_visible(ConsoleState* console_state, bool8 flag);

	void scroll_up(ConsoleState* console_state);
	void scroll_down(ConsoleState* console_state);
	void scroll_to_top(ConsoleState* console_state);
	void scroll_to_bottom(ConsoleState* console_state);

	void on_module_reload(ConsoleState* console_state);
	void on_module_unload(ConsoleState* console_state);

}
