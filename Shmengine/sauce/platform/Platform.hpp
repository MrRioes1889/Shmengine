#pragma once

#include "Defines.hpp"
#include "utility/Math.hpp"
#include "containers/Darray.hpp"
#include "core/Subsystems.hpp"
#include "utility/String.hpp"

namespace Platform
{

	struct SHMAPI WindowHandle
	{	
#if _WIN32
		void* h_instance;
		void* h_wnd;
#endif
	};

	struct DynamicLibFunction
	{
		String name;
		void* function_ptr;
	};

	struct DynamicLibrary
	{
		String name;
		String filename;
		void* handle;

		Darray<DynamicLibFunction> functions;
	};

	struct SystemConfig 
	{
		const char* application_name;
		uint32 x;
		uint32 y;
		uint32 width;
		uint32 height;
	};

	

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	bool32 pump_messages();

	void* allocate(uint64 size, uint16 alignment);
	void free_memory(void* mem, bool32 aligned);

	void* zero_memory(void* mem, uint64 size);
	void* copy_memory(const void* source, void* dest, uint64 size);
	void* set_memory(void* dest, int32 value, uint64 size);

	SHMAPI void init_console();
	void console_write(const char* message, uint8 color);
	void console_write_error(const char* message, uint8 color);

	float64 get_absolute_time();

	void sleep(uint32 ms);

	int32 get_processor_count();

	Math::Vec2i get_cursor_pos();
	void set_cursor_pos(int32 x, int32 y);
	bool32 clip_cursor(bool32 clip);

	SHMAPI WindowHandle get_window_handle();

	SHMAPI bool32 load_dynamic_library(const char* name, DynamicLibrary* out_lib);
	SHMAPI bool32 unload_dynamic_library(DynamicLibrary* lib);
	SHMAPI bool32 load_dynamic_library_function(DynamicLibrary* lib, const char* name);

}

