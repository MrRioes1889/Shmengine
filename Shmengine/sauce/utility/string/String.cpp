#include "../String.hpp"

#include "core/Memory.hpp"

namespace String
{

	void strings_concat(uint32 buffer_output_size, char* buffer_output, const char* buffer_a, const char* buffer_b)
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

	uint32 string_append(uint32 buffer_output_size, char* buffer_output, char appendage)
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

	uint32 string_append(uint32 buffer_output_size, char* buffer_output, const char* buffer_source)
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

	void string_copy(uint32 buffer_output_size, char* buffer_output, const char* buffer_source, uint32 length)
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

		int32 base = 10;
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

		int32 base = 10;
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

	char* to_string(float val, int32 decimals) {

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
		int64 value = Math::round_f_to_i64(val);
		if (is_neg)
			value = -value;

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

	bool32 strings_eq(const char* a, const char* b)
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

	bool32 strings_eq_case_insensitive(const char* a, const char* b)
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

	char* string_trim(char* string)
	{
		char* ret = (char*)string;
		char* end = (char*)string;

		while (*end)
		{
			if ((*ret == ' '))
				ret++;

			end++;
		}
		
		if (end != ret)
		{
			end--;
			while (!*end || *end == ' ')
			{
				*end = 0;
				end--;
			}
		}

		return ret;	
	}

	void string_mid(uint32 buffer_output_size, char* buffer_output, const char* buffer_source, uint32 start, uint32 len)
	{
		SHMASSERT(buffer_output_size > len && length(buffer_source) >= start);

		const char* source = buffer_source;
		char* dest = buffer_output;
		Memory::zero_memory(dest, buffer_output_size);
		
		for (uint32 i = start; (i < start + len) && source[i]; i++)
		{
			*dest = source[i];
			dest++;
		}

		dest++;
		*dest = 0;
		return;
	}

	bool32 parse(const char* s, float32* out_f)
	{
		return true;
	}

	enum class StringScanType
	{
		FLOAT32
	};

	struct TypedScanPointer
	{
		void* ptr;
		StringScanType type;
	};	

}

