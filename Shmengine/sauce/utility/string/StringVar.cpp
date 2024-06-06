#include "../String.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"

namespace String
{

	void _print_s_base(char* target_buffer, uint32 buffer_limit, const char* format, const Arg* args, uint64 arg_count)
	{

		for (uint32 i = 0; i < buffer_limit; i++)
			target_buffer[i] = 0;

		uint32 target_i = 0;
		uint32 arg_i = 0;

		for (const char* c = format; *c != 0; c++)
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

			bool32 format_parse_successful = true;
			switch (*c)
			{
			case('i'):
			{
				if (args[arg_i].type == Arg::Type::INT32)
				{
					int32 v = args[arg_i].int32_value[0];         //Fetch Integer argument
					target_i += string_append(buffer_limit, target_buffer, to_string(v));
					arg_i++;
				}		
				break;
			}
			case('u'):
			{
				if (args[arg_i].type == Arg::Type::UINT32)
				{
					uint32 v = args[arg_i].uint32_value[0];         //Fetch Integer argument
					target_i += string_append(buffer_limit, target_buffer, to_string(v));
					arg_i++;
				}
				break;
			}
			case('s'):
			{
				if (args[arg_i].type == Arg::Type::CHAR_PTR)
				{
					char* v = args[arg_i].char_ptr;         //Fetch String argument
					target_i += string_append(buffer_limit, target_buffer, v);
					arg_i++;
				}
				break;
			}
			case('c'):
			{
				if (args[arg_i].type == Arg::Type::CHAR)
				{
					char v = args[arg_i].char_value[0];         //Fetch Character argument
					target_i += string_append(buffer_limit, target_buffer, v);
					arg_i++;
				}
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

				if (args[arg_i].type == Arg::Type::FLOAT32)
				{
					float32 v = args[arg_i].float32_value[0];         //Fetch Character argument
					target_i += string_append(buffer_limit, target_buffer, to_string(v, decimals));
					arg_i++;
				}			
				break;
			}
			}
		}

	}	

	void print_s(char* target_buffer, uint32 buffer_limit, const char* format, va_list arg_ptr)
	{

		uint32 arg_count = 0;
		Arg args[20] = {};
		const char* temp_format_ptr = format;

		int32 macro_index = index_of(temp_format_ptr, '%');
		while (macro_index >= 0 && *temp_format_ptr)
		{
			temp_format_ptr = &temp_format_ptr[macro_index + 1];
			switch (temp_format_ptr[0])
			{
			case 'f':
			{
				args[arg_count].float32_value[0] = (float32)va_arg(arg_ptr, float64);
				args[arg_count++].type = Arg::Type::FLOAT32;
				temp_format_ptr++;
				break;
			}
			case 'i':
			{
				args[arg_count].int32_value[0] = (int32)va_arg(arg_ptr, int64);
				args[arg_count++].type = Arg::Type::INT32;
				temp_format_ptr++;
				break;
			}
			case 'u':
			{
				args[arg_count].uint32_value[0] = (uint32)va_arg(arg_ptr, uint64);
				args[arg_count++].type = Arg::Type::UINT32;
				temp_format_ptr++;
				break;
			}
			case 's':
			{
				args[arg_count].char_ptr = va_arg(arg_ptr, char*);
				args[arg_count++].type = Arg::Type::CHAR_PTR;
				temp_format_ptr++;
				break;
			}
			case 'c':
			{
				args[arg_count].char_value[0] = va_arg(arg_ptr, char);
				args[arg_count++].type = Arg::Type::CHAR;
				temp_format_ptr++;
				break;
			}
			}

			macro_index = index_of(temp_format_ptr, '%');
		}

		_print_s_base(target_buffer, buffer_limit, format, args, arg_count);

	}

	void print_s(char* target_buffer, uint32 buffer_limit, const char* format, ...)
	{

		va_list arg_ptr;
		va_start(arg_ptr, format);

		print_s(target_buffer, buffer_limit, format, arg_ptr);

		va_end(arg_ptr);
	}

	// TODO: Replace asserts width regular errors
	void _string_scan_base(const char* source, const char* format, const Arg* args, uint64 arg_count)
	{

		SHMASSERT_MSG(arg_count <= 20, "Argument count exceeded string scan limit");

		Arg arg_copies[20] = {};

		uint32 ptr_i = 0;
		while (*source)
		{
			if (*format == '%')
			{
				format++;
				SHMASSERT_MSG(*format || !*source, "String scan failed: Format does not match source string!");

				const uint32 parse_buffer_size = 512;
				char parse_buffer[parse_buffer_size] = {};

				for (uint32 i = 0; i < parse_buffer_size; i++)
				{
					if (*source == *(format + 1))
						break;

					SHMASSERT_MSG(*source, "String scan failed: Format does not match source string!");

					parse_buffer[i] = *source;
					source++;
				}

				switch (*format)
				{
				case 'f':
				{
					format++;
					float32* f = &arg_copies[ptr_i++].float32_value[0];         //Fetch String argument
					parse(parse_buffer, f);
					break;
				}
				}
			}
			else
			{
				SHMASSERT_MSG(*format == *source, "String scan failed: Format does not match source string!");
				format++;
				source++;
			}

		}

		SHMASSERT_MSG(!*format, "String scan failed: Format does not match source string!");

		for (uint32 i = 0; i < arg_count; i++)
		{
			switch (args[i].type)
			{
			case Arg::Type::FLOAT32_PTR:
			{
				*(args[i].float32_ptr) = arg_copies[i].float32_value[0];
				break;
			}
			}
		}

	}

	void string_scan(const char* source, const char* format, ...)
	{

		va_list arg_ptr;
		va_start(arg_ptr, format);

		uint32 arg_count = 0;
		Arg args[20] = {};
		const char* temp_format_ptr = format;

		int32 macro_index = index_of(temp_format_ptr, '%');
		while (macro_index >= 0 && *temp_format_ptr)
		{
			temp_format_ptr = &temp_format_ptr[macro_index + 1];
			switch (temp_format_ptr[0])
			{
			case 'f':
			{
				args[arg_count].float32_ptr = (float32*)va_arg(arg_ptr, float32*);
				args[arg_count++].type = Arg::Type::FLOAT32_PTR;
				temp_format_ptr++;
				break;
			}
			}

			macro_index = index_of(temp_format_ptr, '%');
		}

		_string_scan_base(source, format, args, arg_count);

		va_end(arg_ptr);
	}

}