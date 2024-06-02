#include "FileSystem.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "utility/Utility.hpp"

#if _WIN32

#include <windows.h>

namespace FileSystem
{

	SHMAPI bool32 file_exists(const char* path)
	{
		DWORD dwAttrib = GetFileAttributesA(path);

		return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}

	SHMAPI int64 get_file_size(FileHandle* file)
	{
		LARGE_INTEGER file_size;
		if (!GetFileSizeEx(file->handle, &file_size))
		{
			return -1;
		}
		return (int64)file_size.QuadPart;
	}

	SHMAPI bool32 file_open(const char* path, FileMode mode, FileHandle* out_file)
	{

		out_file->handle = 0;

		HANDLE file_handle;

		if ((mode & FILE_MODE_READ) && (mode & FILE_MODE_WRITE))
		{
			file_handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
		}
		else if (mode & FILE_MODE_READ)
		{
			file_handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		}
		else if (mode & FILE_MODE_WRITE)
		{
			file_handle = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
		}
		else 
		{
			SHMERRORV("Invalid mode passed while trying to open file: '%s'", path);
			return false;
		}

		if (file_handle == INVALID_HANDLE_VALUE)
		{
			CloseHandle(file_handle);
			SHMERRORV("Failed to open/create file: '%s'", path);
			return false;
		}

		out_file->handle = file_handle;

		return true;
	}

	SHMAPI void file_close(FileHandle* file)
	{		

		CloseHandle(file->handle);
		*file = {};

	}

	bool32 read_bytes(FileHandle* file, uint32 size, Buffer* out_buffer, uint32* out_bytes_read)
	{

		if (file->handle) {

			out_buffer->free_data();
	
			out_buffer->init(size, AllocationTag::RAW);

			if (!ReadFile(file->handle, out_buffer->data, size, (LPDWORD)out_bytes_read, 0))
			{
				SHMERROR("Failed to read file.");
				return false;
			}

			return true;
		}
		return false;

	}

	SHMAPI bool32 read_all_bytes(FileHandle* file, Buffer* out_buffer, uint32* out_bytes_read)
	{

		if (file->handle) {

			int64 file_size = get_file_size(file);
			if (file_size < 0)
			{
				SHMERROR("Failed to get file size for reading file");
				return false;
			}

			uint32 file_size32 = s_truncate_uint64((uint64)file_size);

			return read_bytes(file, file_size32, out_buffer, out_bytes_read);

		}
		return false;

	}

	SHMAPI bool32 write(FileHandle* file, uint32 size, const void* data, uint32* out_bytes_written)
	{

		if (!WriteFile(file->handle, data, size, (DWORD*)out_bytes_written, 0))
		{
			SHMERROR("Failed to write to file.");
			return false;
		}

		if (*out_bytes_written < size)
			SHMWARN("Wrote less bytes to file than anticipated!");

		return true;

	}

}

#endif