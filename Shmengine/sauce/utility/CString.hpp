#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"
#include "utility/Math.hpp"
#include <stdarg.h>

namespace CString
{	

	SHMAPI char* to_string(uint32 val);
	SHMAPI char* to_string(uint64 val);
	SHMAPI char* to_string(int32 val);
	SHMAPI char* to_string(int64 val);
	SHMAPI char* to_string(float32 val, int32 decimals = 2);
	SHMAPI char* to_string(float64 val, int32 decimals = 2);

	SHMAPI uint32 append(char* buffer_output, uint32 buffer_output_size, char appendage);
	SHMAPI uint32 append(char* buffer_output, uint32 buffer_output_size, const char* buffer_source, int32 length = -1);

	SHMAPI void concat(char* buffer_output, uint32 buffer_output_size, const char* buffer_a, const char* buffer_b);

	SHMAPI uint32 copy(const char* buffer_source, char* buffer_output, uint32 buffer_output_size, int32 length = -1);

	// Case insensitive version: equal_i()
	SHMAPI bool32 equal(const char* a, const char* b);
	SHMAPI bool32 equal_i(const char* a, const char* b);

	SHMAPI bool32 nequal(const char* a, const char* b, uint32 length);
	SHMAPI bool32 nequal_i(const char* a, const char* b, uint32 length);

	SHMAPI uint32 trim(char* string);

	SHMAPI uint32 mid(char* buffer, uint32 buffer_length, uint32 start, int32 length = -1);

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

	SHMINLINE int32 index_of_last(const char* buffer, char c)
	{
		int32 ret = -1;
		int32 i = 0;
		while (buffer[i])
		{
			if (buffer[i] == c)
				ret = i;
			i++;
		}
		return ret;
	}

	SHMINLINE bool32 is_whitespace(char c)
	{
		return index_of(" \f\n\r\t\v", c) != -1;
	}

	SHMINLINE void empty(char* buffer, bool32 full_empty = false)
	{
		char* p = buffer;
		*p++ = 0;
		if (full_empty)
		{
			while (*p)
				*p++ = 0;
		}	
	}

	SHMINLINE uint32 left_of_last(char* buffer_output, uint32 buffer_length, char split_c)
	{
		int32 i = index_of_last(buffer_output, split_c);
		if (i > 0)
			return mid(buffer_output, buffer_length, 0, i);
		else
			return buffer_length;
	}

	SHMINLINE uint32 right_of_last(char* buffer_output, uint32 buffer_length, char split_c)
	{
		int32 i = index_of_last(buffer_output, split_c);
		return mid(buffer_output, buffer_length, i + 1);
	}

	SHMINLINE void replace(char* buffer, char old_char, char new_char)
	{
		while (*buffer)
		{
			if (*buffer == old_char)
				*buffer = new_char;
			buffer++;
		}
	}

	SHMAPI bool32 parse(const char* s, float32* out);
	SHMAPI bool32 parse(const char* s, float64* out);
	SHMAPI bool32 parse(const char* s, int8* out);
	SHMAPI bool32 parse(const char* s, int16* out);
	SHMAPI bool32 parse(const char* s, int32* out);
	SHMAPI bool32 parse(const char* s, int64* out);
	SHMAPI bool32 parse(const char* s, uint8* out);
	SHMAPI bool32 parse(const char* s, uint16* out);
	SHMAPI bool32 parse(const char* s, uint32* out);
	SHMAPI bool32 parse(const char* s, uint64* out);
	SHMAPI bool32 parse(const char* s, bool8* out);

	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, float32* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, float64* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, int8* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, int16* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, int32* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, int64* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, uint8* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, uint16* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, uint32* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, uint64* out_arr);
	SHMAPI bool32 parse_arr(const char* s, char delimiter, uint32 arr_count, bool8* out_arr);

	struct Arg
	{

		enum class Type { NONE, INT8, INT16, INT32, INT64, UINT8, UINT16, UINT32, UINT64, FLOAT32, FLOAT64, CHAR, CHAR_PTR, FLOAT32_PTR, FLOAT64_PTR, INT8_PTR, INT16_PTR, INT32_PTR, INT64_PTR, UINT8_PTR, UINT16_PTR, UINT32_PTR, UINT64_PTR };
	
		Type type;
		union {
			int8 int8_value[8];
			int16 int16_value[4];
			int32 int32_value[2];
			int64 int64_value;
			uint8 uint8_value[8];
			uint16 uint16_value[4];
			uint32 uint32_value[2];
			uint64 uint64_value;
			float32 float32_value[2];
			float64 float64_value;
			char char_value[8];
			const char* char_ptr;
			char* char_buffer;
			float32* float32_ptr;
			float64* float64_ptr;
			int8* int8_ptr;
			int16* int16_ptr;
			int32* int32_ptr;
			int64* int64_ptr;
			uint8* uint8_ptr;
			uint16* uint16_ptr;
			uint32* uint32_ptr;
			uint64* uint64_ptr;
		};

		Arg() : type(Type::NONE), int64_value(0){}

		Arg(int8 value) : type(Type::INT8) { int8_value[0] = value; }
		Arg(int16 value) : type(Type::INT16) { int16_value[0] = value; }
		Arg(int32 value) : type(Type::INT32) { int32_value[0] = value; }
		Arg(int64 value) : type(Type::INT64) { int64_value = value; }
		Arg(uint8 value) : type(Type::UINT8) { uint8_value[0] = value; }
		Arg(uint16 value) : type(Type::UINT16) { uint16_value[0] = value; }
		Arg(uint32 value) : type(Type::UINT32) { uint32_value[0] = value; }
		Arg(uint64 value) : type(Type::UINT64) { uint64_value = value; }
		Arg(float32 value) : type(Type::FLOAT32) { float32_value[0] = value; }
		Arg(float64 value) : type(Type::FLOAT64) { float64_value = value; }
		Arg(char value) : type(Type::CHAR) { char_value[0] = value; }

		Arg(char* value) : type(Type::CHAR_PTR) { char_buffer = value; }
		Arg(const char* value) : type(Type::CHAR_PTR) { char_ptr = value; }
		Arg(int8* value) : type(Type::INT8_PTR) { int8_ptr = value; }
		Arg(int16* value) : type(Type::INT16_PTR) { int16_ptr = value; }
		Arg(int32* value) : type(Type::INT32_PTR) { int32_ptr = value; }
		Arg(int64* value) : type(Type::INT64_PTR) { int64_ptr = value; }
		Arg(uint8* value) : type(Type::UINT8_PTR) { uint8_ptr = value; }
		Arg(uint16* value) : type(Type::UINT16_PTR) { uint16_ptr = value; }
		Arg(uint32* value) : type(Type::UINT32_PTR) { uint32_ptr = value; }
		Arg(uint64* value) : type(Type::UINT64_PTR) { uint64_ptr = value; }
		Arg(float32* value) : type(Type::FLOAT32_PTR) { float32_ptr = value; }
		Arg(float64* value) : type(Type::FLOAT64_PTR) { float64_ptr = value; }	
	
	};

	enum class ArgSize
	{
		LONG,
		SHORT_SHORT,
		SHORT,
		LONG_LONG
	};

	SHMAPI int32 _print_s_base(char* target_buffer, uint32 buffer_limit, const char* format, const Arg* args, uint64 arg_count);

	int32 print_s(char* target_buffer, uint32 buffer_limit, const char* format, va_list arg_ptr);
	SHMAPI int32 print_s(char* target_buffer, uint32 buffer_limit, const char* format, ...);

	template<typename... Args>
	SHMINLINE int32 safe_print_s(char* target_buffer, uint32 buffer_limit, const char* format, const Args&... args)
	{
		Arg arg_array[] = { args... };
		return _print_s_base(target_buffer, buffer_limit, format, arg_array, sizeof...(Args));
	}

	SHMAPI bool32 _scan_base(const char* source, const char* format, const Arg* args, uint64 arg_count);

	SHMAPI bool32 scan(const char* source, const char* format, ...);

	template<typename... Args>
	SHMINLINE bool32 safe_scan(const char* source, const char* format, const Args&... args)
	{
		Arg arg_array[] = { args... };
		return _scan_base(source, format, arg_array, sizeof...(Args));
	}

	SHMINLINE bool32 parse(const char* s, Math::Mat4* out_f)
	{
		return safe_scan<float32*, float32*, float32*, float32*, float32*, float32*, float32*, float32*, float32*, float32*, float32*, float32*, float32*, float32*, float32*, float32*>
			(s, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", 
				&out_f->data[0], &out_f->data[1], &out_f->data[2], &out_f->data[3],
				&out_f->data[4], &out_f->data[5], &out_f->data[6], &out_f->data[7],
				&out_f->data[8], &out_f->data[9], &out_f->data[10], &out_f->data[11],
				&out_f->data[12], &out_f->data[13], &out_f->data[14], &out_f->data[15]);
	}

	SHMINLINE bool32 parse(const char* s, Math::Vec4f* out_f)
	{
		return safe_scan<float32*, float32*, float32*, float32*>(s, "%f %f %f %f", &out_f->e[0], &out_f->e[1], &out_f->e[2], &out_f->e[3]);
	}

	SHMINLINE bool32 parse(const char* s, Math::Vec3f* out_f)
	{
		return safe_scan<float32*, float32*, float32*>(s, "%f %f %f", &out_f->e[0], &out_f->e[1], &out_f->e[2]);
	}

	SHMINLINE bool32 parse(const char* s, Math::Vec2f* out_f)
	{
		return safe_scan<float32*, float32*>(s, "%f %f", &out_f->e[0], &out_f->e[1]);
	}

	
}
