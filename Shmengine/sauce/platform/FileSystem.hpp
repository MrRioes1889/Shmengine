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
	SHMAPI int64 get_file_size(FileHandle* file);

	SHMAPI bool32 file_open(const char* path, FileMode mode, FileHandle* out_file);
	SHMAPI void file_close(FileHandle* file);

	SHMAPI bool32 read_bytes(FileHandle* file, uint32 size, Buffer* out_buffer, uint32* out_bytes_read);
	SHMAPI bool32 read_all_bytes(FileHandle* file, Buffer* out_buffer, uint32* out_bytes_read);

	SHMAPI bool32 write(FileHandle* file, uint32 size, const void* data, uint32* out_bytes_written);

}


