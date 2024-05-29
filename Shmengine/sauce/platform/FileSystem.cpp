#include "FileSystem.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"

#include <stdio.h>
#include <sys/stat.h>

//TODO: Rewrite the whole filesystem. It's trash right now.

namespace FileSystem
{

	SHMAPI bool32 file_exists(const char* path)
	{
		struct stat buffer;
		return stat(path, &buffer) == 0;
	}

	SHMAPI bool32 file_open(const char* path, FileMode mode, bool32 binary, FileHandle* out_file)
	{
		out_file->is_valid = false;
		out_file->handle = 0;
		const char* mode_str;

		if ((mode & FILE_MODE_READ) && (mode & FILE_MODE_WRITE)) {
			mode_str = binary ? "w+b" : "w+";
		}
		else if ((mode & FILE_MODE_READ)) {
			mode_str = binary ? "rb" : "r";
		}
		else if ((mode & FILE_MODE_WRITE)) {
			mode_str = binary ? "wb" : "w";
		}
		else {
			SHMERRORV("Invalid mode passed while trying to open file: '%s'", path);
			return false;
		}

		// Attempt to open the file.
		FILE* file = 0;
		fopen_s(&file, path, mode_str);
		if (!file) {
			SHMERRORV("Error opening file: '%s'", path);
			return false;
		}

		out_file->handle = file;
		out_file->is_valid = true;

		return true;
	}

	void file_close(FileHandle* file)
	{
		if (file->handle) {
			fclose((FILE*)file->handle);
			file->handle = 0;
			file->is_valid = false;
		}
	}

	bool32 read_line(FileHandle* file, char** line_buf)
	{
		if (file->handle) {
			// Since we are reading a single line, it should be safe to assume this is enough characters.
			char buffer[32000];
			if (fgets(buffer, 32000, (FILE*)file->handle) != 0) {
				uint32 length = String::length(buffer);
				*line_buf = (char*)Memory::allocate((sizeof(char) * length) + 1, true, AllocationTag::RAW);
				String::copy_string_to_buffer(length, *line_buf, buffer);
				return true;
			}
		}
		return false;
	}

	bool32 read_bytes(FileHandle* file, uint64 size, void* out_data, uint64* out_bytes_read)
	{
		if (file->handle && out_data) {
			*out_bytes_read = fread(out_data, 1, size, (FILE*)file->handle);
			if (*out_bytes_read != size) {
				return false;
			}
			return true;
		}
		return false;
	}

	bool32 read_all_bytes(FileHandle* file, uint8** out_data, uint64* out_bytes_read)
	{
		if (file->handle) {
			// File size
			fseek((FILE*)file->handle, 0, SEEK_END);
			uint64 size = ftell((FILE*)file->handle);
			rewind((FILE*)file->handle);

			*out_data = (uint8*)Memory::allocate(sizeof(uint8) * size, true, AllocationTag::RAW);
			*out_bytes_read = fread(*out_data, 1, size, (FILE*)file->handle);
			if (*out_bytes_read != size) {
				return false;
			}
			return true;
		}
		return false;
	}

	bool32 write_line(FileHandle* file, const char* text)
	{
		if (file->handle) {
			int32 result = fputs(text, (FILE*)file->handle);
			if (result != EOF) {
				result = fputc('\n', (FILE*)file->handle);
			}

			// Make sure to flush the stream so it is written to the file immediately.
			// This prevents data loss in the event of a crash.
			fflush((FILE*)file->handle);
			return result != EOF;
		}
		return false;
	}

	bool32 write(FileHandle* file, uint64 size, const void* data, uint64* out_bytes_written)
	{
		if (file->handle) {
			*out_bytes_written = fwrite(data, 1, size, (FILE*)file->handle);
			if (*out_bytes_written != size) {
				return false;
			}
			fflush((FILE*)file->handle);
			return true;
		}
		return false;
	}

}