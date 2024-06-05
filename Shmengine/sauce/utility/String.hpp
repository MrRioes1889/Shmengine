#pragma once

#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"
#include "utility/Math.hpp"
#include <stdarg.h>

// TODO: These have to be replaced! Should not be called inside a contiguous function call at the moment and probably arent sufficiently fast as well!
namespace String
{

	char* to_string(uint32 val);
	char* to_string(int32 val);
	char* to_string(float val, int32 decimals = 2);

	SHMINLINE uint32 length(const char* buffer)
	{
		uint32 ret = 0;
		while (*buffer++)
			ret++;
		return ret;
	}

	SHMINLINE void empty(char* buffer)
	{
		char* p = buffer;
		while (*p)
			*p++ = 0;
	}

	SHMINLINE int32 parse_int32(char* s)
	{
		int32 ret = 0;
		int32 sign = 1;
		if (s[0] == '-')
		{
			sign = -1;
			s++;
		}

		uint32 l = length(s);
		for (uint32 i = 0; i < l; i++)
		{
			SHMASSERT(s[i] >= '0' && s[i] <= '9');
			ret += (int32)(s[i] - '0') * Math::pow(10, l - i - 1);
		}
		return (ret * sign);
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

	uint32 append_to_string(uint32 buffer_output_size, char* buffer_output, char appendage);
	uint32 append_to_string(uint32 buffer_output_size, char* buffer_output, const char* buffer_source);
	void concat_strings(uint32 buffer_output_size, char* buffer_output, const char* buffer_a, const char* buffer_b);
	void copy_string_to_buffer(uint32 buffer_output_size, char* buffer_output, const char* buffer_source);
	SHMAPI bool32 strings_eq(const char* a, const char* b);
	SHMAPI bool32 strings_eq_case_insensitive(const char* a, const char* b);
	SHMAPI void print_s(char* target_buffer, uint32 buffer_limit, const char* s, ...);
	void print_s(char* target_buffer, uint32 buffer_limit, const char* s, va_list arg_ptr);

}
