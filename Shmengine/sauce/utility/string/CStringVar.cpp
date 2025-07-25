#include "../CString.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "utility/String.hpp"

namespace CString
{

	int32 _print_s_base(char* target_buffer, uint32 buffer_limit, const char* format, const PrintArg* args, uint64 arg_count)
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
					return -1;
				}
				c++;
			}

			if (*c == 0)
				break;

			c++;

			char format_identifier = c[0];

			ArgSize arg_size = ArgSize::LONG;
			if (format_identifier == 'l')
			{
				arg_size = ArgSize::LONG_LONG;
				c++;
			}
			if (format_identifier == 'h')
			{
				arg_size = ArgSize::SHORT;
				c++;
				if (c[0] == 'h')
				{
					arg_size = ArgSize::SHORT_SHORT;
					c++;
				}
			}

			format_identifier = c[0];

			bool8 valid_format = true;

			switch (format_identifier)
			{
			case('i'):
			{

				switch (arg_size)
				{
				case ArgSize::LONG_LONG:
				{
					if (args[arg_i].type == PrintArg::Type::INT64)
					{
						int64 v = args[arg_i++].int64_value;         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v));
					}
					else
						valid_format = false;
					break;
				}
				case ArgSize::SHORT:
				{
					if (args[arg_i].type == PrintArg::Type::INT16)
					{
						int16 v = args[arg_i++].int16_value[0];         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v));
					}
					else
						valid_format = false;
					break;
				}
				case ArgSize::SHORT_SHORT:
				{
					if (args[arg_i].type == PrintArg::Type::INT8)
					{
						int8 v = args[arg_i++].int8_value[0];         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v));
					}
					else
						valid_format = false;
					break;
				}
				default:
				{
					if (args[arg_i].type == PrintArg::Type::INT32)
					{
						int32 v = args[arg_i++].int32_value[0];         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v));
					}
					else
						valid_format = false;
					break;
				}
				}				

				break;
			}
			case('u'):
			{

				switch (arg_size)
				{
				case ArgSize::LONG_LONG:
				{
					if (args[arg_i].type == PrintArg::Type::UINT64)
					{
						uint64 v = args[arg_i++].uint64_value;         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v));
					}
					else
						valid_format = false;
					break;
				}
				case ArgSize::SHORT:
				{
					if (args[arg_i].type == PrintArg::Type::UINT16)
					{
						uint16 v = args[arg_i++].uint16_value[0];         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v));
					}
					else
						valid_format = false;
					break;
				}
				case ArgSize::SHORT_SHORT:
				{
					if (args[arg_i].type == PrintArg::Type::UINT8)
					{
						uint8 v = args[arg_i++].uint8_value[0];         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v));
					}
					else
						valid_format = false;
					break;
				}
				default:
				{
					if (args[arg_i].type == PrintArg::Type::UINT32)
					{
						uint32 v = args[arg_i++].uint32_value[0];         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v));
					}
					else
						valid_format = false;
					break;
				}
				}

				break;
			}
			case('s'):
			{
				if (args[arg_i].type != PrintArg::Type::CHAR_PTR)
				{
					SHMERROR("print_s: Provided arguments do not match format!");
					return -1;
				}

				const char* v = args[arg_i++].char_ptr;         //Fetch String argument
				target_i += append(target_buffer, buffer_limit, v);

				break;
			}
			case('c'):
			{
				if (args[arg_i].type != PrintArg::Type::CHAR)
				{
					SHMERROR("print_s: Provided arguments do not match format!");
					return -1;
				}

				char v = args[arg_i++].char_value[0];         //Fetch Character argument
				target_i += append(target_buffer, buffer_limit, v);

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

				switch (arg_size)
				{
				case ArgSize::LONG_LONG:
				{
					if (args[arg_i].type == PrintArg::Type::FLOAT64)
					{
						float64 v = args[arg_i++].float64_value;         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v, decimals));
					}
					else
						valid_format = false;
					break;
				}
				default:
				{
					if (args[arg_i].type == PrintArg::Type::FLOAT32)
					{
						float32 v = args[arg_i++].float32_value[0];         //Fetch Integer argument
						target_i += append(target_buffer, buffer_limit, to_string(v, decimals));
					}
					else
						valid_format = false;
					break;
				}
				}

				break;
			}
			}

			if (!valid_format)
			{
				SHMERROR("print_s: Provided arguments do not match format!");
				return -1;
			}
		}

		return (int32)target_i;

	}	

	int32 print_s(char* target_buffer, uint32 buffer_limit, const char* format, va_list arg_ptr)
	{

		uint32 arg_count = 0;
		PrintArg args[20] = {};
		const char* temp_format_ptr = format;

		int32 macro_index = index_of(temp_format_ptr, '%');
		while (macro_index >= 0 && *temp_format_ptr)
		{
			temp_format_ptr = &temp_format_ptr[macro_index + 1];
			char format_identifier = temp_format_ptr[0];

			ArgSize arg_size = ArgSize::LONG;
			if (format_identifier == 'l')
			{
				arg_size = ArgSize::LONG_LONG;
				temp_format_ptr++;
			}
			if (format_identifier == 'h')
			{
				arg_size = ArgSize::SHORT;
				temp_format_ptr++;
				if (temp_format_ptr[0] == 'h')
				{
					arg_size = ArgSize::SHORT_SHORT;
					temp_format_ptr++;
				}
			}

			format_identifier = temp_format_ptr[0];

			switch (format_identifier)
			{
			case 'f':
			{
				switch (arg_size)
				{
				case ArgSize::LONG_LONG:
				{
					args[arg_count].float64_value = va_arg(arg_ptr, float64);
					args[arg_count++].type = PrintArg::Type::FLOAT64;
					break;
				}
				default:
				{
					args[arg_count].float32_value[0] = (float32)va_arg(arg_ptr, float64);
					args[arg_count++].type = PrintArg::Type::FLOAT32;
					break;
				}
				}

				temp_format_ptr++;
				break;
			}
			case 'i':
			{
				switch (arg_size)
				{
				case ArgSize::LONG_LONG:
				{
					args[arg_count].int64_value = va_arg(arg_ptr, int64);
					args[arg_count++].type = PrintArg::Type::INT64;
					break;
				}
				case ArgSize::SHORT:
				{
					args[arg_count].int16_value[0] = (int16)va_arg(arg_ptr, int64);
					args[arg_count++].type = PrintArg::Type::INT16;
					break;
				}
				case ArgSize::SHORT_SHORT:
				{
					args[arg_count].int8_value[0] = (int8)va_arg(arg_ptr, int64);
					args[arg_count++].type = PrintArg::Type::INT8;
					break;
				}
				default:
				{
					args[arg_count].int32_value[0] = (int32)va_arg(arg_ptr, int64);
					args[arg_count++].type = PrintArg::Type::INT32;
					break;
				}
				}

				temp_format_ptr++;
				break;
			}
			case 'u':
			{
				switch (arg_size)
				{
				case ArgSize::LONG_LONG:
				{
					args[arg_count].uint64_value = va_arg(arg_ptr, uint64);
					args[arg_count++].type = PrintArg::Type::UINT64;
					break;
				}
				case ArgSize::SHORT:
				{
					args[arg_count].uint16_value[0] = (uint16)va_arg(arg_ptr, uint64);
					args[arg_count++].type = PrintArg::Type::UINT16;
					break;
				}
				case ArgSize::SHORT_SHORT:
				{
					args[arg_count].uint8_value[0] = (uint8)va_arg(arg_ptr, uint64);
					args[arg_count++].type = PrintArg::Type::UINT8;
					break;
				}
				default:
				{
					args[arg_count].uint32_value[0] = (uint32)va_arg(arg_ptr, uint64);
					args[arg_count++].type = PrintArg::Type::UINT32;
					break;
				}
				}

				temp_format_ptr++;
				break;
			}
			case 's':
			{
				args[arg_count].char_ptr = va_arg(arg_ptr, char*);
				args[arg_count++].type = PrintArg::Type::CHAR_PTR;
				temp_format_ptr++;
				break;
			}
			case 'c':
			{
				args[arg_count].char_value[0] = (char)va_arg(arg_ptr, uint64);
				args[arg_count++].type = PrintArg::Type::CHAR;
				temp_format_ptr++;
				break;
			}
			}

			macro_index = index_of(temp_format_ptr, '%');
		}

		return _print_s_base(target_buffer, buffer_limit, format, args, arg_count);

	}

	int32 print_s(char* target_buffer, uint32 buffer_limit, const char* format, ...)
	{

		va_list arg_ptr;
		va_start(arg_ptr, format);

		int32 res = print_s(target_buffer, buffer_limit, format, arg_ptr);

		va_end(arg_ptr);

		return res;

	}

	// TODO: Replace asserts width regular errors
	bool8 _scan_base(const char* source, const char* format, const ScanArg* args, uint64 arg_count)
	{

		SHMASSERT_MSG(arg_count <= 20, "Argument count exceeded string scan limit");

		PrintArg arg_copies[20] = {};

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

				ArgSize arg_size = ArgSize::LONG;
				if (format_identifier == 'l')
				{
					arg_size = ArgSize::LONG_LONG;
					format++;
				}
				if (format_identifier == 'h')
				{
					arg_size = ArgSize::SHORT;
					format++;
					if (format[0] == 'h')
					{
						arg_size = ArgSize::SHORT_SHORT;
						format++;
					}
				}

				format_identifier = format[0];

				const uint32 parse_buffer_size = 512;
				char parse_buffer[parse_buffer_size] = {};

				for (uint32 i = 0; i < parse_buffer_size; i++)
				{
					if (*source == *(format + 1) || (!*(format + 1) && is_whitespace(*source)))
						break;

					if (!*source || i == (parse_buffer_size - 1))
					{
						SHMERROR("string_scan: Provided arguments do not match format!");
						return false;
					}

					parse_buffer[i] = *source;
					source++;
				}

				bool8 valid_format = true;
				switch (format_identifier)
				{
				case 'f':
				{
					switch (arg_size)
					{
					case ArgSize::LONG_LONG:
					{
						if (args[ptr_i].type == ScanArg::Type::FLOAT64_PTR)
							parse(parse_buffer, &arg_copies[ptr_i++].float64_value);
						else
							valid_format = false;
						break;
					}
					default:
					{
						if (args[ptr_i].type == ScanArg::Type::FLOAT32_PTR)	
							parse(parse_buffer, &arg_copies[ptr_i++].float32_value[0]);
						else
							valid_format = false;						
						break;
					}
					}

					format++;				
					break;
				}
				case 'i':
				{
					switch (arg_size)
					{
					case ArgSize::LONG_LONG:
					{
						if (args[ptr_i].type == ScanArg::Type::INT64_PTR)
							parse(parse_buffer, &arg_copies[ptr_i++].int64_value);
						else
							valid_format = false;
						break;
					}
					case ArgSize::SHORT:
					{
						if (args[ptr_i].type == ScanArg::Type::INT16_PTR)
							parse(parse_buffer, &arg_copies[ptr_i++].int16_value[0]);
						else
							valid_format = false;
						break;
					}
					case ArgSize::SHORT_SHORT:
					{
						if (args[ptr_i].type == ScanArg::Type::INT8_PTR)
							parse(parse_buffer, &arg_copies[ptr_i++].int8_value[0]);
						else
							valid_format = false;
						break;
					}
					default:
					{
						if (args[ptr_i].type == ScanArg::Type::INT32_PTR)
							parse(parse_buffer, &arg_copies[ptr_i++].int32_value[0]);
						else
							valid_format = false;
						break;
					}
					}

					format++;
					break;
				}
				case 'u':
				{

					switch (arg_size)
					{
					case ArgSize::LONG_LONG:
					{
						if (args[ptr_i].type == ScanArg::Type::UINT64_PTR)
							parse(parse_buffer, &arg_copies[ptr_i++].uint64_value);
						else
							valid_format = false;
						break;
					}
					case ArgSize::SHORT:
					{
						if (args[ptr_i].type == ScanArg::Type::UINT16_PTR)
							parse(parse_buffer, &arg_copies[ptr_i++].uint16_value[0]);
						else
							valid_format = false;
						break;
					}
					case ArgSize::SHORT_SHORT:
					{
						if (args[ptr_i].type == ScanArg::Type::UINT8_PTR)
							parse(parse_buffer, &arg_copies[ptr_i++].uint8_value[0]);
						else
							valid_format = false;
						break;
					}
					default:
					{
						if (args[ptr_i].type == ScanArg::Type::UINT32_PTR)
							parse(parse_buffer, &arg_copies[ptr_i++].uint32_value[0]);
						else
							valid_format = false;
						break;
					}
					}

					format++;
					break;
				}
				case 's':
				{
					if (args[ptr_i].type != ScanArg::Type::STRING_PTR)
					{
						SHMERROR("string_scan: Provided arguments do not match format!");
						return false;
					}

					(*args[ptr_i++].string_ptr) = parse_buffer;

					format++;
					break;
				}
				}

				if (!valid_format)
				{
					SHMERROR("string_scan: Provided arguments do not match format!");
					return false;
				}
			}
			else
			{

				if (!*format)
					break;

				while (*source == ' ' && *format != *source)
					source++;

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
			case ScanArg::Type::FLOAT32_PTR:
			{
				*(args[i].float32_ptr) = arg_copies[i].float32_value[0];
				break;
			}
			case ScanArg::Type::FLOAT64_PTR:
			{
				*(args[i].float64_ptr) = arg_copies[i].float64_value;
				break;
			}
			case ScanArg::Type::INT8_PTR:
			{
				*(args[i].int8_ptr) = arg_copies[i].int8_value[0];
				break;
			}
			case ScanArg::Type::INT16_PTR:
			{
				*(args[i].int16_ptr) = arg_copies[i].int16_value[0];
				break;
			}
			case ScanArg::Type::INT32_PTR:
			{
				*(args[i].int32_ptr) = arg_copies[i].int32_value[0];
				break;
			}
			case ScanArg::Type::INT64_PTR:
			{
				*(args[i].int64_ptr) = arg_copies[i].int64_value;
				break;
			}
			case ScanArg::Type::UINT8_PTR:
			{
				*(args[i].int8_ptr) = arg_copies[i].int8_value[0];
				break;
			}
			case ScanArg::Type::UINT16_PTR:
			{
				*(args[i].int16_ptr) = arg_copies[i].int16_value[0];
				break;
			}
			case ScanArg::Type::UINT32_PTR:
			{
				*(args[i].uint32_ptr) = arg_copies[i].uint32_value[0];
				break;
			}
			case ScanArg::Type::UINT64_PTR:
			{
				*(args[i].uint64_ptr) = arg_copies[i].uint64_value;
				break;
			}
			default:
				break;
			}
		}

		return true;

	}

	bool8 scan(const char* source, const char* format, ...)
	{

		va_list arg_ptr;
		va_start(arg_ptr, format);

		uint32 arg_count = 0;
		ScanArg args[20] = {};
		const char* temp_format_ptr = format;

		int32 macro_index = index_of(temp_format_ptr, '%');
		while (macro_index >= 0 && *temp_format_ptr)
		{
			temp_format_ptr = &temp_format_ptr[macro_index + 1];
			char format_identifier = temp_format_ptr[0];

			ArgSize arg_size = ArgSize::LONG;
			if (format_identifier == 'l')
			{
				arg_size = ArgSize::LONG_LONG;
				temp_format_ptr++;
			}
			if (format_identifier == 'h')
			{
				arg_size = ArgSize::SHORT;
				temp_format_ptr++;
				if (temp_format_ptr[0] == 'h')
				{
					arg_size = ArgSize::SHORT_SHORT;
					temp_format_ptr++;
				}
			}

			format_identifier = temp_format_ptr[0];

			switch (format_identifier)
			{
			case 'f':
			{
				switch (arg_size)
				{
				case ArgSize::LONG_LONG:
				{
					args[arg_count].float64_ptr = (float64*)va_arg(arg_ptr, float64*);
					args[arg_count++].type = ScanArg::Type::FLOAT64_PTR;
					break;
				}
				default:
				{
					args[arg_count].float32_ptr = (float32*)va_arg(arg_ptr, float32*);
					args[arg_count++].type = ScanArg::Type::FLOAT32_PTR;
					break;
				}
				}
			
				temp_format_ptr++;
				break;
			}
			case 'i':
			{
				switch (arg_size)
				{
				case ArgSize::LONG_LONG:
				{
					args[arg_count].int64_ptr = (int64*)va_arg(arg_ptr, int64*);
					args[arg_count++].type = ScanArg::Type::INT64_PTR;
					break;
				}
				case ArgSize::SHORT:
				{
					args[arg_count].int16_ptr = (int16*)va_arg(arg_ptr, int16*);
					args[arg_count++].type = ScanArg::Type::INT16_PTR;
					break;
				}
				case ArgSize::SHORT_SHORT:
				{
					args[arg_count].int8_ptr = (int8*)va_arg(arg_ptr, int8*);
					args[arg_count++].type = ScanArg::Type::INT8_PTR;
					break;
				}
				default:
				{
					args[arg_count].int32_ptr = (int32*)va_arg(arg_ptr, int32*);
					args[arg_count++].type = ScanArg::Type::INT32_PTR;
					break;
				}
				}

				temp_format_ptr++;
				break;
			}
			case 'u':
			{
				switch (arg_size)
				{
				case ArgSize::LONG_LONG:
				{
					args[arg_count].uint64_ptr = (uint64*)va_arg(arg_ptr, uint64*);
					args[arg_count++].type = ScanArg::Type::UINT64_PTR;
					break;
				}
				case ArgSize::SHORT:
				{
					args[arg_count].uint16_ptr = (uint16*)va_arg(arg_ptr, uint16*);
					args[arg_count++].type = ScanArg::Type::UINT16_PTR;
					break;
				}
				case ArgSize::SHORT_SHORT:
				{
					args[arg_count].uint8_ptr = (uint8*)va_arg(arg_ptr, uint8*);
					args[arg_count++].type = ScanArg::Type::UINT8_PTR;
					break;
				}
				default:
				{
					args[arg_count].uint32_ptr = (uint32*)va_arg(arg_ptr, uint32*);
					args[arg_count++].type = ScanArg::Type::UINT32_PTR;
					break;
				}
				}

				temp_format_ptr++;
				break;
			}
			case 's':
			{

				args[arg_count].string_ptr = (String*)va_arg(arg_ptr, char*);
				args[arg_count++].type = ScanArg::Type::STRING_PTR;

				temp_format_ptr++;
				break;
			}
			}

			macro_index = index_of(temp_format_ptr, '%');
		}

		bool8 res = _scan_base(source, format, args, arg_count);

		va_end(arg_ptr);

		return res;
	}

}