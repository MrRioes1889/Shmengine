#pragma once

#include "Defines.hpp"

#include "containers/Buffer.hpp"

enum FileMode
{
	FILE_MODE_READ = 0b1,
	FILE_MODE_WRITE = 0b10
};

namespace FileSystem
{

	struct FileHandle
	{
		void* handle;
	};

	SHMAPI bool32 file_exists(const char* path);
	SHMAPI uint32 get_file_size32(FileHandle* file);

	SHMAPI bool32 file_open(const char* path, FileMode mode, FileHandle* out_file);
	SHMAPI void file_close(FileHandle* file);

	SHMAPI bool32 read_bytes(FileHandle* file, uint32 size, void* out_buffer, uint32 out_buffer_size, uint32* out_bytes_read);
	SHMAPI bool32 read_all_bytes(FileHandle* file, void* out_buffer, uint32 out_buffer_size, uint32* out_bytes_read);
	SHMAPI int32 read_line(const char* file_buffer, char* line_buffer, uint32 line_buffer_size, const char** out_continue_ptr = 0);

	SHMAPI bool32 write(FileHandle* file, uint32 size, const void* data, uint32* out_bytes_written);


	SHMINLINE bool32 read_bytes(FileHandle* file, uint32 size, Buffer* out_buffer, uint32* out_bytes_read)
	{
		return read_bytes(file, size, out_buffer->data, out_buffer->size, out_bytes_read);
	}
	SHMINLINE bool32 read_all_bytes(FileHandle* file, Buffer* out_buffer, uint32* out_bytes_read)
	{
		return read_all_bytes(file, out_buffer->data, out_buffer->size, out_bytes_read);
	}
	SHMINLINE int32 read_line(const Buffer* file_buffer, const Buffer* line_buffer, const char** out_continue_ptr = 0)
	{
		return read_line((const char*)file_buffer->data, (char*)line_buffer->data, line_buffer->size, out_continue_ptr);
	}

}


