#pragma once

#include "Defines.hpp"

struct UIText;

namespace DebugConsole
{

	void init();
	void destroy();

	bool32 load();
	void unload();

	void update();

	UIText* get_text();
	UIText* get_entry_text();

	bool32 is_visible();
	void set_visible(bool8 flag);

	void scroll_up();
	void scroll_down();
	void scroll_to_top();
	void scroll_to_bottom();

}
