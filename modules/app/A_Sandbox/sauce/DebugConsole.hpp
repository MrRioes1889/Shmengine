#pragma once

#include "Defines.hpp"
#include <resources/UIText.hpp>

namespace DebugConsole
{

	struct State
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

	void init(State* console_state);
	void destroy(State* console_state);

	bool32 load(State* console_state);
	void unload(State* console_state);

	void update(State* console_state);

	UIText* get_text(State* console_state);
	UIText* get_entry_text(State* console_state);

	bool32 is_visible(State* console_state);
	void set_visible(State* console_state, bool8 flag);

	void scroll_up(State* console_state);
	void scroll_down(State* console_state);
	void scroll_to_top(State* console_state);
	void scroll_to_bottom(State* console_state);

	void on_module_reload(State* console_state);
	void on_module_unload(State* console_state);

}
