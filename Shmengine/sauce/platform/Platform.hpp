#pragma once

#include "Defines.hpp"

namespace Platform
{

	bool32 startup(
		void* linear_allocator, 
		void*& out_state,
		const char* application_name,
		int32 x, int32 y,
		int32 width, int32 height);

	void shutdown();
	bool32 pump_messages();

	void* allocate(uint64 size, bool32 aligned);
	void free_memory(void* mem, bool32 aligned);

	void* zero_memory(void* mem, uint64 size);
	void* copy_memory(const void* source, void* dest, uint64 size);
	void* set_memory(void* dest, int32 value, uint64 size);

	void init_console();
	void console_write(const char* message, uint8 color);
	void console_write_error(const char* message, uint8 color);

	float64 get_absolute_time();

	void sleep(uint32 ms);

}

