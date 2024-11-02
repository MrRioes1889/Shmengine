#include "FileSystem.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/CString.hpp"
#include "utility/Utility.hpp"

#if _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace FileSystem
{

	bool32 file_exists(const char* path)
	{
		DWORD dwAttrib = GetFileAttributesA(path);

		return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}

	static int64 get_file_size(FileHandle* file)
	{
		LARGE_INTEGER file_size;
		if (!GetFileSizeEx(file->handle, &file_size))
		{
			return -1;
		}
		return (int64)file_size.QuadPart;
	}

	uint32 get_file_size32(FileHandle* file)
	{

		int64 file_size = get_file_size(file);
		if (file_size < 0 || file_size > 0xFFFFFFFF)
		{
			SHMERROR("Failed to get file size for reading file");
			return 0;
		}

		return s_truncate_uint64((uint64)file_size);

	}

	bool32 file_open(const char* path, FileMode mode, FileHandle* out_file)
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

	void file_close(FileHandle* file)
	{		

		CloseHandle(file->handle);
		*file = {};

	}

	bool32 read_bytes(FileHandle* file, uint32 size, void* out_buffer, uint32 out_buffer_size, uint32* out_bytes_read)
	{

		if (file->handle) {

			if (!out_buffer || out_buffer_size < size)
			{
				return false;
			}

			if (!ReadFile(file->handle, out_buffer, size, (LPDWORD)out_bytes_read, 0))
			{
				SHMERROR("Failed to read file.");
				return false;
			}

			return true;
		}
		return false;

	}

	bool32 read_all_bytes(FileHandle* file, void* out_buffer, uint32 out_buffer_size, uint32* out_bytes_read)
	{

		uint32 file_size = get_file_size32(file);
		if (file_size) {
			read_bytes(file, file_size, out_buffer, out_buffer_size, out_bytes_read);
			return file_size == *out_bytes_read;
		}
		return false;

	}

	int32 read_line(const char* file_buffer, char* line_buffer, uint32 line_buffer_size, const char** out_continue_ptr)
	{

		if (!file_buffer || !line_buffer)
			return -1;

		const char* source = file_buffer;
		if (out_continue_ptr && *out_continue_ptr)
			source = *out_continue_ptr;

		if (!*source)
			return 0;

		int32 read_length = CString::index_of(source, '\n');
		if (read_length < 0)
			read_length = CString::length(source);

		if ((uint32)read_length > line_buffer_size - 1)
			read_length = line_buffer_size - 1;
			
		CString::copy(line_buffer_size, line_buffer, source, (uint32)read_length);

		if (out_continue_ptr)
			*out_continue_ptr = &source[read_length + 1];

		return 1;

	}

	bool32 write(FileHandle* file, uint32 size, const void* data, uint32* out_bytes_written)
	{
		if (!file)
			return true;

		if (!WriteFile(file->handle, data, size, (DWORD*)out_bytes_written, 0))
		{
			SHMERROR("Failed to write to file.");
			return false;
		}

		if (*out_bytes_written < size)
			SHMWARN("Wrote less bytes to file than anticipated!");

		return true;

	}

	bool32 read_bytes(FileHandle* file, uint32 size, String& out_buffer, uint32* out_bytes_read)
	{
		if (!file->handle)
			return false;

		if (out_buffer.arr.capacity < size)
			out_buffer.arr.resize(size);

		if (!ReadFile(file->handle, out_buffer.arr.data, size, (LPDWORD)out_bytes_read, 0))
		{
			SHMERROR("Failed to read file.");
			return false;
		}

		return true;

	}

	bool32 read_all_bytes(FileHandle* file, String& out_buffer, uint32* out_bytes_read)
	{
		uint32 file_size = get_file_size32(file);
		if (file_size) {
			read_bytes(file, file_size, out_buffer, out_bytes_read);
			return file_size == *out_bytes_read;
		}
		return false;
	}

	bool32 read_line(const char* file_buffer, String& line_buffer, const char** out_continue_ptr)
	{

		const char* source = file_buffer;
		if (out_continue_ptr && *out_continue_ptr)
			source = *out_continue_ptr;

		if (!*source)
			return false;

		int32 read_length = CString::index_of(source, '\n');
		if (read_length < 0) 
			read_length = CString::length(source);

		line_buffer.copy_n(source, (uint32)read_length);

		if (out_continue_ptr)
			*out_continue_ptr = &source[read_length + 1];

		return true;

	}	

}

#endif