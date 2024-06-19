#include "../CString.hpp"

#include "core/Memory.hpp"
#include "utility/Math.hpp"

namespace CString
{

	void concat(uint32 buffer_output_size, char* buffer_output, const char* buffer_a, const char* buffer_b)
	{
		char* write_ptr = buffer_output;
		uint32 write_output_size = buffer_output_size - 1;
		uint32 characters_written = 0;

		while (*buffer_a && characters_written <= write_output_size)
		{
			*write_ptr++ = *buffer_a++;
			characters_written++;
		}

		while (*buffer_b && characters_written <= write_output_size)
		{
			*write_ptr++ = *buffer_b++;
			characters_written++;
		}

		*write_ptr = 0;
	}

	uint32 append(uint32 buffer_output_size, char* buffer_output, char appendage)
	{
		uint32 appendix_length = 1;
		for (uint32 i = 0; i < buffer_output_size; i++)
		{
			if (!buffer_output[i])
			{
				buffer_output[i] = appendage;
				buffer_output[i + 1] = 0;
				break;
			}
		}
		return appendix_length;
	}

	uint32 append(uint32 buffer_output_size, char* buffer_output, const char* buffer_source)
	{
		uint32 appendix_length = 0;
		char* write_ptr = buffer_output;
		uint32 write_output_size = buffer_output_size - 1;

		while (*write_ptr)
		{
			write_ptr++;
			write_output_size--;
		}

		for (uint32 i = 0; i < write_output_size; i++)
		{
			if (!*buffer_source)
				break;

			*write_ptr++ = *buffer_source++;
			appendix_length++;
		}

		*write_ptr = 0;
		return appendix_length;
	}

	void copy(uint32 buffer_output_size, char* buffer_output, const char* buffer_source, uint32 length)
	{
		char* write_ptr = buffer_output;
		uint32 write_output_size = buffer_output_size - 1;
		if (length != 0 && length < write_output_size)
			write_output_size = length;

		for (uint32 i = 0; i < write_output_size; i++)
		{
			if (!*buffer_source)
				break;

			*write_ptr++ = *buffer_source++;
		}

		*write_ptr = 0;
	}

	char* to_string(uint32 val) {

		uint32 base = 10;
		static char buf[64] = {};

		int i = 62;
		if (val)
		{
			for (; val && i; val /= base)
				buf[i--] = "0123456789abcdef"[val % base];
		}
		else
			buf[i--] = '0';

		return &buf[i + 1];

	}

	char* to_string(uint64 val) {

		uint32 base = 10;
		static char buf[64] = {};

		int i = 62;
		if (val)
		{
			for (; val && i; val /= base)
				buf[i--] = "0123456789abcdef"[val % base];
		}
		else
			buf[i--] = '0';

		return &buf[i + 1];

	}

	// NOTE: Could take the base to get different number formats
	char* to_string(int32 val) {

		uint32 base = 10;
		static char buf[64] = {};
		bool32 is_neg = (val < 0);
		uint32 value = is_neg ? -val : val;

		int i = 62;
		if (val)
		{
			for (; value && i; value /= base)
				buf[i--] = "0123456789abcdef"[value % base];

			if (is_neg)
				buf[i--] = '-';
		}
		else
			buf[i--] = '0';

		return &buf[i + 1];

	}

	char* to_string(int64 val) {

		uint32 base = 10;
		static char buf[64] = {};
		bool32 is_neg = (val < 0);
		uint64 value = is_neg ? -val : val;

		int i = 62;
		if (val)
		{
			for (; value && i; value /= base)
				buf[i--] = "0123456789abcdef"[value % base];

			if (is_neg)
				buf[i--] = '-';
		}
		else
			buf[i--] = '0';

		return &buf[i + 1];

	}

	char* to_string(float32 val, int32 decimals) {

		int32 base = 10;
		int32 leading_zeroes = 0;
		for (int32 i = 0; i < decimals; i++)
		{
			if (val < 1.0f && val > -1.0f)
				leading_zeroes++;
			val *= 10;
		}

		// NOTE: Accounting for pre-comma zero
		if (val < 1.0f && val > -1.0f && val != 0.0f)
			leading_zeroes++;

		static char buf[64] = {};
		bool32 is_neg = (val < 0);
		int64 value = Math::round_f_to_i64(is_neg ? -val : val);

		int32 i = 62;

		if (leading_zeroes)
		{
			decimals = -1;
		}

		for (; value && i; value /= base)
		{
			buf[i--] = "0123456789abcdef"[value % base];
			decimals--;
			if (decimals == 0)
				buf[i--] = '.';
		}

		for (int32 j = 0; j < leading_zeroes - 1; j++)
		{
			buf[i--] = '0';
		}
		if (leading_zeroes)
		{
			buf[i--] = '.';
			buf[i--] = '0';
			leading_zeroes--;
		}

		if (is_neg)
			buf[i--] = '-';

		return &buf[i + 1];

	}

	char* to_string(float64 val, int32 decimals) {

		int32 base = 10;
		int32 leading_zeroes = 0;
		for (int32 i = 0; i < decimals; i++)
		{
			if (val < 1.0f && val > -1.0f)
				leading_zeroes++;
			val *= 10;
		}

		// NOTE: Accounting for pre-comma zero
		if (val < 1.0f && val > -1.0f && val != 0.0f)
			leading_zeroes++;

		static char buf[64] = {};
		bool32 is_neg = (val < 0);
		int64 value = Math::round_f_to_i64(is_neg ? -val : val);

		int32 i = 62;

		if (leading_zeroes)
		{
			decimals = -1;
		}

		for (; value && i; value /= base)
		{
			buf[i--] = "0123456789abcdef"[value % base];
			decimals--;
			if (decimals == 0)
				buf[i--] = '.';
		}

		for (int32 j = 0; j < leading_zeroes - 1; j++)
		{
			buf[i--] = '0';
		}
		if (leading_zeroes)
		{
			buf[i--] = '.';
			buf[i--] = '0';
			leading_zeroes--;
		}

		if (is_neg)
			buf[i--] = '-';

		return &buf[i + 1];

	}

	bool32 equal(const char* a, const char* b)
	{
		while (*a != 0)
		{
			if (*a != *b)
				return false;
			a++;
			b++;
		}

		if (*b == 0)
			return true;
		else
			return false;
	}

	bool32 equal_i(const char* a, const char* b)
	{

		int32 upper_lower_offset = 'a' - 'A';

		while (*a != 0)
		{		
			
			if (*a != *b)
			{
				bool32 lower_case_a = (*a >= 'a' && *a <= 'z');
				bool32 upper_case_a = (*a >= 'A' && *a <= 'Z');

				if (!lower_case_a && !upper_case_a)
					return false;
				else if (lower_case_a && (*b != (*a) - upper_lower_offset))
					return false;
				else if (upper_case_a && (*b != (*a) + upper_lower_offset))
					return false;
			}

			a++;
			b++;
		}

		if (*b == 0)
			return true;
		else
			return false;

	}

	char* trim(char* string)
	{
		char* start = string;
		char* end = string;

		while (*end)
		{
			if (is_whitespace(*start))
				start++;

			end++;
		}
		
		if (end != start)
		{
			end--;
			while (!*end || is_whitespace(*end))
			{
				*end = 0;
				end--;
			}
		}

		return start;
	}

	void mid(uint32 buffer_output_size, char* buffer_output, const char* buffer_source, uint32 start, int32 len)
	{
		SHMASSERT((int32)buffer_output_size > len && length(buffer_source) >= start);

		const char* source = buffer_source;
		char* dest = buffer_output;
		Memory::zero_memory(dest, buffer_output_size);
		
		for (uint32 i = start; (len < 0 || (i < start + len)) && source[i]; i++)
		{
			*dest = source[i];
			dest++;
		}

		dest++;
		*dest = 0;
		return;
	}

	bool32 parse(const char* s, float32& out_f)
	{

		int32 sign = 1;
		const char* ptr = s;
		float32 ret = 0.0f;

		if (ptr[0] == '-')
		{
			sign = -1;
			ptr++;
		}

		s = ptr;
		uint32 length = 0;
		uint32 sep_index = 0;
		while (*ptr)
		{
			if (*ptr == '.' && !sep_index && ptr != s)
				sep_index = length;
			else if (*ptr < '0' || *ptr > '9')
				return false;

			length++;
			ptr++;
		}

		ptr = s;
		uint32 low_part_l;
		uint32 high_part_l;
		if (!sep_index)
		{
			high_part_l = length;
			low_part_l = 0;			
		}
		else
		{
			high_part_l = sep_index;
			low_part_l = length - high_part_l - 1;
		}

		float64 multiplier = 1 / Math::pow(10.0, low_part_l);

		for (int32 i = (int32)(high_part_l + low_part_l); i >= 0; i--)
		{
			if (ptr[i] != '.')
			{
				uint32 digit = ptr[i] - '0';
				ret += (float32)(digit * multiplier);
				multiplier *= 10;
			}
		}

		out_f = ret * sign;
		return true;
	}

	bool32 parse(const char* s, float64& out_f)
	{

		int32 sign = 1;
		const char* ptr = s;
		float64 ret = 0.0;

		if (ptr[0] == '-')
		{
			sign = -1;
			ptr++;
		}

		s = ptr;
		uint32 length = 0;
		uint32 sep_index = 0;
		while (*ptr)
		{
			if (*ptr == '.' && !sep_index && ptr != s)
				sep_index = length;
			else if (*ptr < '0' || *ptr > '9')
				return false;

			length++;
			ptr++;
		}

		ptr = s;
		uint32 low_part_l;
		uint32 high_part_l;
		if (!sep_index)
		{
			high_part_l = length;
			low_part_l = 0;
		}
		else
		{
			high_part_l = sep_index;
			low_part_l = length - high_part_l - 1;
		}

		float64 multiplier = 1 / Math::pow(10.0, low_part_l);

		for (int32 i = (int32)(high_part_l + low_part_l); i >= 0; i--)
		{
			if (ptr[i] != '.')
			{
				uint32 digit = ptr[i] - '0';
				ret += digit * multiplier;
				multiplier *= 10;
			}
		}

		out_f = ret * sign;
		return true;
	}

	bool32 parse(const char* s, int32& out_i)
	{

		int32 sign = 1;
		const char* ptr = s;
		int32 ret = 0;

		if (ptr[0] == '-')
		{
			sign = -1;
			ptr++;
		}

		uint32 multiplier = 1 ;

		for (int32 i = (int32)(length(ptr) - 1); i >= 0; i--)
		{
			if (ptr[i] < '0' || ptr[i] > '9')
				return false;

			uint32 digit = ptr[i] - '0';
			ret += digit * multiplier;
			multiplier *= 10;
		}

		out_i = ret * sign;
		return true;
	}

	bool32 parse(const char* s, int64& out_i)
	{

		int32 sign = 1;
		const char* ptr = s;
		int64 ret = 0;

		if (ptr[0] == '-')
		{
			sign = -1;
			ptr++;
		}

		uint64 multiplier = 1;

		for (int32 i = (int32)(length(ptr) - 1); i >= 0; i--)
		{
			if (ptr[i] < '0' || ptr[i] > '9')
				return false;

			uint32 digit = ptr[i] - '0';
			ret += digit * multiplier;
			multiplier *= 10;
		}

		out_i = ret * sign;
		return true;
	}

	bool32 parse(const char* s, uint32& out_i)
	{

		int32 sign = 1;
		const char* ptr = s;
		uint32 ret = 0;

		if (ptr[0] == '-')
		{
			sign = -1;
			ptr++;
		}

		uint32 multiplier = 1;

		for (int32 i = (int32)(length(ptr) - 1); i >= 0; i--)
		{
			if (ptr[i] < '0' || ptr[i] > '9')
				return false;

			uint32 digit = ptr[i] - '0';
			ret += digit * multiplier;
			multiplier *= 10;
		}

		out_i = ret * sign;
		return true;
	}

	bool32 parse(const char* s, uint64& out_i)
	{

		int32 sign = 1;
		const char* ptr = s;
		uint64 ret = 0;

		if (ptr[0] == '-')
		{
			sign = -1;
			ptr++;
		}

		uint64 multiplier = 1;

		for (int32 i = (int32)(length(ptr) - 1); i >= 0; i--)
		{
			if (ptr[i] < '0' || ptr[i] > '9')
				return false;

			uint32 digit = ptr[i] - '0';
			ret += digit * multiplier;
			multiplier *= 10;
		}

		out_i = ret * sign;
		return true;
	}

}

