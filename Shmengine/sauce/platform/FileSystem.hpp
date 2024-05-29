#pragma once

#include "Defines.hpp"

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
		bool32 is_valid;
	};

	SHMAPI bool32 file_exists(const char* path);

	SHMAPI bool32 file_open(const char* path, FileMode mode, bool32 binary, FileHandle* out_file);
	SHMAPI void file_close(FileHandle* file);

	SHMAPI bool32 read_line(FileHandle* file, char** line_buf);	

	SHMAPI bool32 read_bytes(FileHandle* file, uint64 size, void* out_data, uint64* out_bytes_read);
	SHMAPI bool32 read_all_bytes(FileHandle* file, uint8** out_data, uint64* out_bytes_read);

	SHMAPI bool32 write_line(FileHandle* file, const char* text);
	SHMAPI bool32 write(FileHandle* file, uint64 size, const void* data, uint64* out_bytes_written);

}


