#pragma once

#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"
#include "utility/Math.hpp"
#include <stdarg.h>

// TODO: These have to be replaced! Should not be called inside a contiguous function call at the moment and probably arent sufficiently fast as well!
namespace String
{

	SHMINLINE bool32 is_whitespace(char c)
	{
		return (
			c != ' ' &&
			c != '\f' &&
			c != '\n' &&
			c != '\r' &&
			c != '\t' &&
			c != '\v');
	}

	char* to_string(uint32 val);
	char* to_string(uint64 val);
	char* to_string(int32 val);
	char* to_string(int64 val);
	char* to_string(float32 val, int32 decimals = 2);
	char* to_string(float64 val, int32 decimals = 2);

	SHMINLINE uint32 length(const char* buffer)
	{
		uint32 ret = 0;
		while (*buffer++)
			ret++;
		return ret;
	}

	SHMINLINE int32 index_of(const char* buffer, char c)
	{
		uint32 ret = 0;
		while (buffer[ret] && buffer[ret] != c)
		{
			ret++;
		}

		if (!buffer[ret])
			return -1;
		else			
			return ret;
	}

	SHMINLINE void empty(char* buffer)
	{
		char* p = buffer;
		while (*p)
			*p++ = 0;
	}

	SHMINLINE void left_of_last(uint32 length, char* buffer_output, char split_c, bool32 exclude_last = false)
	{
		for (int32 i = (int32)length - 1; i >= 0; i--)
		{
			if (buffer_output[i] == split_c && (i != (int32)length - 1 || !exclude_last))
			{
				buffer_output[i] = 0;
				return;
			}

			buffer_output[i] = 0;
		}
	}

	SHMAPI uint32 string_append(uint32 buffer_output_size, char* buffer_output, char appendage);
	SHMAPI uint32 string_append(uint32 buffer_output_size, char* buffer_output, const char* buffer_source);

	SHMAPI void strings_concat(uint32 buffer_output_size, char* buffer_output, const char* buffer_a, const char* buffer_b);

	SHMAPI void string_copy(uint32 buffer_output_size, char* buffer_output, const char* buffer_source, uint32 length = 0);

	SHMAPI bool32 strings_eq(const char* a, const char* b);
	SHMAPI bool32 strings_eq_case_insensitive(const char* a, const char* b);

	SHMAPI char* string_trim(char* string);

	SHMAPI void string_mid(uint32 buffer_output_size, char* buffer_output, const char* buffer_source, uint32 start, uint32 length);

	SHMAPI bool32 parse(const char* s, float32& out_f);
	SHMAPI bool32 parse(const char* s, float64& out_f);
	SHMAPI bool32 parse(const char* s, int32& out_f);
	SHMAPI bool32 parse(const char* s, int64& out_f);
	SHMAPI bool32 parse(const char* s, uint32& out_f);
	SHMAPI bool32 parse(const char* s, uint64& out_f);

	struct Arg
	{

		enum class Type { NONE, INT32, INT64, UINT32, UINT64, FLOAT32, FLOAT64, CHAR, CHAR_PTR, FLOAT32_PTR, FLOAT64_PTR, INT32_PTR, INT64_PTR, UINT32_PTR, UINT64_PTR };
	
		Type type;
		union {
			int32 int32_value[2];
			int64 int64_value;
			uint32 uint32_value[2];
			uint64 uint64_value;
			float32 float32_value[2];
			float64 float64_value;
			char char_value[8];
			char* char_ptr;
			float32* float32_ptr;
			float64* float64_ptr;
			int32* int32_ptr;
			int64* int64_ptr;
			uint32* uint32_ptr;
			uint64* uint64_ptr;
		};

		Arg() : int64_value(0), type(Type::NONE) {}

		Arg(int32 value) : type(Type::INT32) { int32_value[0] = value; }
		Arg(int64 value) : type(Type::INT64) { int64_value = value; }
		Arg(uint32 value) : type(Type::UINT32) { uint32_value[0] = value; }
		Arg(uint64 value) : type(Type::UINT64) { uint64_value = value; }
		Arg(float32 value) : type(Type::FLOAT32) { float32_value[0] = value; }
		Arg(float64 value) : type(Type::FLOAT64) { float64_value = value; }
		Arg(char value) : type(Type::CHAR) { char_value[0] = value; }

		Arg(char* value) : type(Type::CHAR_PTR) { char_ptr = value; }
		Arg(int32* value) : type(Type::INT32_PTR) { int32_ptr = value; }
		Arg(int64* value) : type(Type::INT64_PTR) { int64_ptr = value; }
		Arg(uint32* value) : type(Type::UINT32_PTR) { uint32_ptr = value; }
		Arg(uint64* value) : type(Type::UINT64_PTR) { uint64_ptr = value; }
		Arg(float32* value) : type(Type::FLOAT32_PTR) { float32_ptr = value; }
		Arg(float64* value) : type(Type::FLOAT64_PTR) { float64_ptr = value; }	
	
	};

	SHMAPI bool32 _print_s_base(char* target_buffer, uint32 buffer_limit, const char* format, const Arg* args, uint64 arg_count);

	bool32 print_s(char* target_buffer, uint32 buffer_limit, const char* format, va_list arg_ptr);
	SHMAPI bool32 print_s(char* target_buffer, uint32 buffer_limit, const char* format, ...);

	template<typename... Args>
	SHMINLINE bool32 safe_print_s(char* target_buffer, uint32 buffer_limit, const char* format, const Args&... args)
	{
		Arg arg_array[] = { args... };
		return _print_s_base(target_buffer, buffer_limit, format, arg_array, sizeof...(Args));
	}

	SHMAPI bool32 _string_scan_base(const char* source, const char* format, const Arg* args, uint64 arg_count);

	SHMAPI bool32 string_scan(const char* source, const char* format, ...);

	template<typename... Args>
	SHMINLINE bool32 safe_string_scan(const char* source, const char* format, const Args&... args)
	{
		Arg arg_array[] = { args... };
		return _string_scan_base(source, format, arg_array, sizeof...(Args));
	}
}
