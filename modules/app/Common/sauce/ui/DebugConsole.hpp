#pragma once

#include "Defines.hpp"
#include <resources/UIText.hpp>
#include <resources/ResourceTypes.hpp>
#include <core/Keymap.hpp>

struct DebugConsole
{
	static constexpr uint32 lines_limit = 1024;
	uint16 line_lenghts[lines_limit];

	uint32 consumer_id;
	uint32 lines_count;
	uint32 lines_display_limit;
	uint32 lines_display_offset;
	uint64 text_display_offset;

	String console_text;
	String entry_text;
	char entry_text_prefix[5];

	ResourceState state;
	bool8 visible;
	bool8 loaded;
	KeyCode::Value held_key;

	Input::Keymap keymap;
	UIText text_control;
	UIText entry_control;

	bool8 init();
	void destroy();

	void setup_keymap();

	bool8 load();
	bool8 unload();

	void update();

	UIText* get_text();
	UIText* get_entry_text();

	bool8 is_visible();
	void set_visible(bool8 flag);

	void scroll_up();
	void scroll_down();
	void scroll_to_top();
	void scroll_to_bottom();

	void on_module_reload();
	void on_module_unload();
};
