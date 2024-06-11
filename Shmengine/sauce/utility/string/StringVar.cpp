#include "../String.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"

namespace String
{

	bool32 _print_s_base(char* target_buffer, uint32 buffer_limit, const char* format, const Arg* args, uint64 arg_count)
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
				{
					SHMERROR("print_s: Buffer ran out of space!");
					return false;
				}
				c++;
			}

			if (*c == 0)
				break;

			c++;

			char format_identifier = c[0];

			bool32 ll_value = false;
			if (format_identifier == 'l')
			{
				ll_value = true;
				c++;
			}

			bool32 format_parse_successful = true;
			switch (format_identifier)
			{
			case('i'):
			{
				if (!(args[arg_i].type == Arg::Type::INT32 || (args[arg_i].type == Arg::Type::INT64 && ll_value)))
				{
					SHMERROR("print_s: Provided arguments do not match format!");
					return false;
				}

				if (args[arg_i].type == Arg::Type::INT32)
				{
					int32 v = args[arg_i++].int32_value[0];         //Fetch Integer argument
					target_i += append(buffer_limit, target_buffer, to_string(v));
				}	
				else
				{
					int64 v = args[arg_i++].int64_value;         //Fetch Integer argument
					target_i += append(buffer_limit, target_buffer, to_string(v));
				}
				break;
			}
			case('u'):
			{
				if (!(args[arg_i].type == Arg::Type::UINT32 || (args[arg_i].type == Arg::Type::UINT64 && ll_value)))
				{
					SHMERROR("print_s: Provided arguments do not match format!");
					return false;
				}

				if (args[arg_i].type == Arg::Type::UINT32)
				{
					uint32 v = args[arg_i++].uint32_value[0];         //Fetch Integer argument
					target_i += append(buffer_limit, target_buffer, to_string(v));
				}
				else
				{
					uint64 v = args[arg_i++].uint64_value;         //Fetch Integer argument
					target_i += append(buffer_limit, target_buffer, to_string(v));
				}
				break;
			}
			case('s'):
			{
				if (args[arg_i].type == Arg::Type::CHAR_PTR)
				{
					const char* v = args[arg_i++].char_ptr;         //Fetch String argument
					target_i += append(buffer_limit, target_buffer, v);
				}
				break;
			}
			case('c'):
			{
				if (args[arg_i].type == Arg::Type::CHAR)
				{
					char v = args[arg_i++].char_value[0];         //Fetch Character argument
					target_i += append(buffer_limit, target_buffer, v);
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

				if (!(args[arg_i].type == Arg::Type::FLOAT32 || (args[arg_i].type == Arg::Type::FLOAT64 && ll_value)))
				{
					SHMERROR("print_s: Provided arguments do not match format!");
					return false;
				}

				if (args[arg_i].type == Arg::Type::FLOAT32)
				{
					float32 v = args[arg_i++].float32_value[0];         //Fetch Character argument
					target_i += append(buffer_limit, target_buffer, to_string(v, decimals));
				}
				else
				{
					float64 v = args[arg_i++].float64_value;         //Fetch Integer argument
					target_i += append(buffer_limit, target_buffer, to_string(v, decimals));
				}
				break;
			}
			}
		}

		return true;

	}	

	bool32 print_s(char* target_buffer, uint32 buffer_limit, const char* format, va_list arg_ptr)
	{

		uint32 arg_count = 0;
		Arg args[20] = {};
		const char* temp_format_ptr = format;

		int32 macro_index = index_of(temp_format_ptr, '%');
		while (macro_index >= 0 && *temp_format_ptr)
		{
			temp_format_ptr = &temp_format_ptr[macro_index + 1];
			char format_identifier = temp_format_ptr[0];

			bool32 ll_value = false;
			if (format_identifier == 'l')
			{
				ll_value = true;
				temp_format_ptr++;
			}

			switch (format_identifier)
			{
			case 'f':
			{
				if (!ll_value)
				{
					args[arg_count].float32_value[0] = (float32)va_arg(arg_ptr, float64);
					args[arg_count++].type = Arg::Type::FLOAT32;
				}
				else
				{
					args[arg_count].float64_value = va_arg(arg_ptr, float64);
					args[arg_count++].type = Arg::Type::FLOAT64;
				}

				temp_format_ptr++;
				break;
			}
			case 'i':
			{
				if (!ll_value)
				{
					args[arg_count].int32_value[0] = (int32)va_arg(arg_ptr, int64);
					args[arg_count++].type = Arg::Type::INT32;
				}
				else
				{
					args[arg_count].int64_value = va_arg(arg_ptr, int64);
					args[arg_count++].type = Arg::Type::INT64;
				}

				temp_format_ptr++;
				break;
			}
			case 'u':
			{
				if (!ll_value)
				{
					args[arg_count].uint32_value[0] = (uint32)va_arg(arg_ptr, uint64);
					args[arg_count++].type = Arg::Type::UINT32;
				}
				else
				{
					args[arg_count].uint64_value = va_arg(arg_ptr, uint64);
					args[arg_count++].type = Arg::Type::UINT64;
				}

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

		return _print_s_base(target_buffer, buffer_limit, format, args, arg_count);

	}

	bool32 print_s(char* target_buffer, uint32 buffer_limit, const char* format, ...)
	{

		va_list arg_ptr;
		va_start(arg_ptr, format);

		bool32 res = print_s(target_buffer, buffer_limit, format, arg_ptr);

		va_end(arg_ptr);

		return res;

	}

	// TODO: Replace asserts width regular errors
	bool32 _scan_base(const char* source, const char* format, const Arg* args, uint64 arg_count)
	{

		SHMASSERT_MSG(arg_count <= 20, "Argument count exceeded string scan limit");

		Arg arg_copies[20] = {};

		uint32 ptr_i = 0;
		while (*source)
		{
			if (*format == '%')
			{
				
				format++;
				if (!(*format || !*source))
				{
					SHMERROR("string_scan: Provided arguments do not match format!");
					return false;
				}

				char format_identifier = format[0];

				bool32 ll_value = false;
				if (format_identifier == 'l')
				{
					ll_value = true;
					format++;
				}

				const uint32 parse_buffer_size = 512;
				char parse_buffer[parse_buffer_size] = {};

				for (uint32 i = 0; i < parse_buffer_size; i++)
				{
					if (*source == *(format + 1))
						break;

					if (!*source)
					{
						SHMERROR("string_scan: Provided arguments do not match format!");
						return false;
					}

					parse_buffer[i] = *source;
					source++;
				}

				switch (format_identifier)
				{
				case 'f':
				{
					if (!(args[ptr_i].type == Arg::Type::FLOAT32_PTR || (args[ptr_i].type == Arg::Type::FLOAT64_PTR && ll_value)))
					{
						SHMERROR("string_scan: Provided arguments do not match format!");
						return false;
					}

					if (args[ptr_i].type == Arg::Type::FLOAT32_PTR)
						parse(parse_buffer, arg_copies[ptr_i++].float32_value[0]);
					else
						parse(parse_buffer, arg_copies[ptr_i++].float64_value);

					format++;				
					break;
				}
				case 'i':
				{
					if (!(args[ptr_i].type == Arg::Type::INT32_PTR || (args[ptr_i].type == Arg::Type::INT64_PTR && ll_value)))
					{
						SHMERROR("string_scan: Provided arguments do not match format!");
						return false;
					}

					if (args[ptr_i].type == Arg::Type::INT32_PTR)
						parse(parse_buffer, arg_copies[ptr_i++].int32_value[0]);
					else
						parse(parse_buffer, arg_copies[ptr_i++].int64_value);

					format++;
					break;
				}
				case 'u':
				{
					if (!(args[ptr_i].type == Arg::Type::UINT32_PTR || (args[ptr_i].type == Arg::Type::UINT64_PTR && ll_value)))
					{
						SHMERROR("string_scan: Provided arguments do not match format!");
						return false;
					}

					if (args[ptr_i].type == Arg::Type::UINT32_PTR)
						parse(parse_buffer, arg_copies[ptr_i++].uint32_value[0]);
					else
						parse(parse_buffer, arg_copies[ptr_i++].uint64_value);

					format++;
					break;
				}
				}
			}
			else
			{
				if (*format != *source)
				{
					SHMERROR("string_scan: Provided arguments do not match format!");
					return false;
				}

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
			case Arg::Type::FLOAT64_PTR:
			{
				*(args[i].float64_ptr) = arg_copies[i].float64_value;
				break;
			}
			case Arg::Type::INT32_PTR:
			{
				*(args[i].int32_ptr) = arg_copies[i].int32_value[0];
				break;
			}
			case Arg::Type::INT64_PTR:
			{
				*(args[i].int64_ptr) = arg_copies[i].int64_value;
				break;
			}
			case Arg::Type::UINT32_PTR:
			{
				*(args[i].uint32_ptr) = arg_copies[i].uint32_value[0];
				break;
			}
			case Arg::Type::UINT64_PTR:
			{
				*(args[i].uint64_ptr) = arg_copies[i].uint64_value;
				break;
			}
			}
		}

		return true;

	}

	bool32 scan(const char* source, const char* format, ...)
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
			char format_identifier = temp_format_ptr[0];

			bool32 ll_value = false;
			if (format_identifier == 'l')
			{
				ll_value = true;
				temp_format_ptr++;
			}

			switch (format_identifier)
			{
			case 'f':
			{
				if (!ll_value)
				{
					args[arg_count].float32_ptr = (float32*)va_arg(arg_ptr, float32*);
					args[arg_count++].type = Arg::Type::FLOAT32_PTR;
				}
				else
				{
					args[arg_count].float64_ptr = (float64*)va_arg(arg_ptr, float64*);
					args[arg_count++].type = Arg::Type::FLOAT64_PTR;
				}
			
				temp_format_ptr++;
				break;
			}
			case 'i':
			{
				if (!ll_value)
				{
					args[arg_count].int32_ptr = (int32*)va_arg(arg_ptr, int32*);
					args[arg_count++].type = Arg::Type::INT32_PTR;
				}
				else
				{
					args[arg_count].int64_ptr = (int64*)va_arg(arg_ptr, int64*);
					args[arg_count++].type = Arg::Type::INT64_PTR;
				}

				temp_format_ptr++;
				break;
			}
			case 'u':
			{
				if (!ll_value)
				{
					args[arg_count].uint32_ptr = (uint32*)va_arg(arg_ptr, uint32*);
					args[arg_count++].type = Arg::Type::UINT32_PTR;
				}
				else
				{
					args[arg_count].uint64_ptr = (uint64*)va_arg(arg_ptr, uint64*);
					args[arg_count++].type = Arg::Type::UINT64_PTR;
				}

				temp_format_ptr++;
				break;
			}
			}

			macro_index = index_of(temp_format_ptr, '%');
		}

		bool32 res = _scan_base(source, format, args, arg_count);

		va_end(arg_ptr);

		return res;
	}

}