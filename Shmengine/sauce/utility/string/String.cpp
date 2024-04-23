#include "String.hpp"

namespace String
{

	void concat_strings(uint32 buffer_output_size, char* buffer_output, char* buffer_a, char* buffer_b)
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

	uint32 append_to_string(uint32 buffer_output_size, char* buffer_output, char appendage)
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

	uint32 append_to_string(uint32 buffer_output_size, char* buffer_output, char* buffer_source)
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

	void copy_string_to_buffer(uint32 buffer_output_size, char* buffer_output, char* buffer_source)
	{
		char* write_ptr = buffer_output;
		uint32 write_output_size = buffer_output_size - 1;
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

	bool32 strings_eq(char* a, char* b)
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

	void print_s(char* target_buffer, uint32 buffer_limit, char* s, va_list arg_ptr)
	{

		for (uint32 i = 0; i < buffer_limit; i++)
			target_buffer[i] = 0;

		uint32 target_i = 0;

		//Module 1: Initializing Myprintf's arguments 	

		for (char* c = s; *c != 0; c++)
		{
			while (*c != '%' && *c != 0)
			{
				target_buffer[target_i++] = *c;
				if (target_i >= buffer_limit - 1)
					SHMASSERT(false);
				c++;
			}

			if (*c == 0)
				break;

			c++;

			switch (*c)
			{
			case('i'):
			{
				int32 i = va_arg(arg_ptr, int32);         //Fetch Integer argument
				target_i += append_to_string(buffer_limit, target_buffer, to_string(i));
				break;
			}
			case('s'):
			{
				char* i = va_arg(arg_ptr, char*);         //Fetch String argument
				target_i += append_to_string(buffer_limit, target_buffer, i);
				break;
			}
			case('c'):
			{
				char a = va_arg(arg_ptr, char);         //Fetch String argument
				target_i += append_to_string(buffer_limit, target_buffer, a);
				break;
			}
			case('f'):
			{
				int32 decimals = 2;
				if (*(c + 1) >= '0' && *(c + 1) <= '9')
				{
					c++;
					decimals = (int32)(*c - '0');
				}

				double i = va_arg(arg_ptr, double);         //Fetch Float argument
				target_i += append_to_string(buffer_limit, target_buffer, to_string((float)i, decimals));
				break;
			}
			}
		}

	}

	void print_s(char* target_buffer, uint32 buffer_limit, char* s, ...)
	{

		va_list arg_ptr;
		va_start(arg_ptr, s);

		print_s(target_buffer, buffer_limit, s, arg_ptr);

		va_end(arg_ptr);
	}

}

