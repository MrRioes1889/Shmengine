#pragma once

#include "Defines.hpp"
#include "utility/Math.hpp"
#include "containers/Darray.hpp"
#include "core/Subsystems.hpp"
#include "utility/String.hpp"

namespace Platform
{

#if _WIN32
	struct SHMAPI WindowHandle
	{	
		void* h_instance;
		void* h_wnd;
	};

	inline const char* dynamic_library_ext = ".dll";
	inline const char* dynamic_library_prefix = "";
#else
	
#endif

	struct DynamicLibrary
	{
		char name[256];
		char filename[Constants::max_filepath_length];
		void* handle;
		uint32 watch_id;
	};

	enum class ReturnCode
	{
		SUCCESS = 0,
		UNKNOWN,
		FILE_NOT_FOUND,
		FILE_LOCKED,
		FILE_ALREADY_EXISTS
	};

	struct WindowConfig
	{
		const char* title;
		uint32 pos_x, pos_y;
		uint32 width, height;
	};

	struct Window
	{
		WindowHandle handle;
		const char* title;
		uint32 id;
		uint32 pos_x, pos_y;
		uint32 client_width, client_height;
		bool8 cursor_clipped;
	};

	struct SystemConfig
	{

	};

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	bool8 create_window(WindowConfig config);
	void destroy_window(uint32 window_id);

	SHMAPI const Window* get_active_window();

	ReturnCode get_last_error();

	bool8 pump_messages();
	void update_file_watches();

	SHMAPI const char* get_root_dir();

	void* allocate(uint64 size, uint16 alignment);
	void free_memory(void* mem, bool8 aligned);

	void* zero_memory(void* mem, uint64 size);
	void* copy_memory(const void* source, void* dest, uint64 size);
	void* set_memory(void* dest, int32 value, uint64 size);

	SHMAPI Platform::ReturnCode register_file_watch(const char* path, uint32* out_watch_id);
	SHMAPI bool8 unregister_file_watch(uint32 watch_id);

	SHMAPI void init_console();
	void console_write(const char* message, uint8 color);
	void console_write_error(const char* message, uint8 color);

	float64 get_absolute_time();

	SHMAPI void sleep(uint32 ms);

	int32 get_processor_count();

	Math::Vec2i get_cursor_pos();
	void set_cursor_pos(int32 x, int32 y);
	bool8 clip_cursor(const Window* window, bool8 clip);

	SHMAPI bool8 load_dynamic_library(const char* name, const char* filename, DynamicLibrary* out_lib);
	SHMAPI bool8 unload_dynamic_library(DynamicLibrary* lib);
	SHMAPI bool8 load_dynamic_library_function(DynamicLibrary* lib, const char* name, void** out_function);

	SHMAPI void message_box(const char* prompt, const char* message);
	SHMAPI void set_window_text(WindowHandle window_handle, const char* s);

}

