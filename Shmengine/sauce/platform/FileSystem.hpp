#pragma once

#include "Defines.hpp"

#include "platform/Platform.hpp"
#include "utility/String.hpp"

enum FileMode
{
	FILE_MODE_READ = 0b1,
	FILE_MODE_WRITE = 0b10
};

namespace FileSystem
{

	struct FileHandle
	{
		bool8 is_valid;
		void* handle;
	};

	SHMAPI bool8 file_exists(const char* path);
	SHMAPI uint32 get_file_size32(FileHandle* file);

	SHMAPI bool8 file_open(const char* path, FileMode mode, FileHandle* out_file);
	SHMAPI void file_close(FileHandle* file);

	SHMAPI Platform::ReturnCode file_copy(const char* source, const char* dest, bool8 overwrite);

	SHMAPI bool8 read_bytes(FileHandle* file, uint32 size, void* out_buffer, uint32 out_buffer_size, uint32* out_bytes_read);
	SHMAPI bool8 read_all_bytes(FileHandle* file, void* out_buffer, uint32 out_buffer_size, uint32* out_bytes_read);
	SHMAPI int32 read_line(const char* file_buffer, char* line_buffer, uint32 line_buffer_size, const char** out_continue_ptr = 0);

	SHMAPI bool8 write(FileHandle* file, uint32 size, const void* data, uint32* out_bytes_written);

	SHMAPI bool8 read_bytes(FileHandle* file, uint32 size, String& out_buffer, uint32* out_bytes_read);
	SHMAPI bool8 read_all_bytes(FileHandle* file, String& out_buffer, uint32* out_bytes_read);
	SHMAPI bool8 read_line(const char* file_buffer, String& line_buffer, const char** out_continue_ptr = 0);

}


