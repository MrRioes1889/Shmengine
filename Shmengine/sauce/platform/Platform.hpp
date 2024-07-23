#pragma once

#include "Defines.hpp"
#include "utility/Math.hpp"

namespace Platform
{

	bool32 system_init(
		PFN_allocator_allocate_callback allocator_callback,
		void*& out_state,
		const char* application_name,
		int32 x, int32 y,
		int32 width, int32 height);

	void system_shutdown();
	bool32 pump_messages();

	void* allocate(uint64 size, uint16 alignment);
	void free_memory(void* mem, bool32 aligned);

	void* zero_memory(void* mem, uint64 size);
	void* copy_memory(const void* source, void* dest, uint64 size);
	void* set_memory(void* dest, int32 value, uint64 size);

	void init_console();
	void console_write(const char* message, uint8 color);
	void console_write_error(const char* message, uint8 color);

	float64 get_absolute_time();

	void sleep(uint32 ms);

	int32 get_processor_count();

	Math::Vec2i get_cursor_pos();
	void set_cursor_pos(int32 x, int32 y);
	bool32 clip_cursor(bool32 clip);

}

