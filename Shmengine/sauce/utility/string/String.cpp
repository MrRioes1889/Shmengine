#include "../String.hpp"
#include "../CString.hpp"

String::String()
	: arr({})
{
	//arr.init(String::min_reserve_size, DarrayFlag::IS_STRING, AllocationTag::STRING);
}

String::String(uint32 reserve_size)
{
	reserve_size++;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, DarrayFlags::IS_STRING, AllocationTag::STRING);
}

String::String(const char* s, uint32 length)
{
	uint32 s_length = CString::length(s);
	if (s_length < length)
		length = s_length;

	uint32 reserve_size = length + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, DarrayFlags::IS_STRING, AllocationTag::STRING);
	CString::copy(arr.capacity, arr.data, s, length);
	arr.count = length;
}

String::String(const char* s)
{
	uint32 s_length = CString::length(s);
	uint32 reserve_size = s_length + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, DarrayFlags::IS_STRING, AllocationTag::STRING);
	CString::copy(arr.capacity, arr.data, s);
	arr.count = s_length;
}

String::~String()
{
	free_data();
}

String::String(const String& other)
{
	uint32 reserve_size = other.len() + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, DarrayFlags::IS_STRING, AllocationTag::STRING);
	CString::copy(arr.capacity, arr.data, other.c_str());
	arr.count = other.len();
}

String& String::operator=(const String& other)
{

	//free_data();
	uint32 reserve_size = other.len() + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	if (!arr.data)
		arr.init(reserve_size, DarrayFlags::IS_STRING, AllocationTag::STRING);
	else if (arr.capacity < reserve_size)
		arr.resize(reserve_size);

	CString::copy(arr.capacity, arr.data, other.c_str());
	arr.count = other.len();	
	return *this;

}

String::String(String&& other) noexcept
{
	arr.data = other.arr.data;
	arr.capacity = other.arr.capacity;
	arr.count = other.arr.count;

	other.arr.data = 0;
	other.arr.capacity = 0;
	other.arr.count = 0;
}

String& String::operator=(String&& other) noexcept
{
    free_data();
	arr.data = other.arr.data;
	arr.capacity = other.arr.capacity;
	arr.count = other.arr.count;

	other.arr.data = 0;
	other.arr.capacity = 0;
	other.arr.count = 0;
	return *this;
}

String& String::operator=(const char* s)
{

	uint32 s_length = CString::length(s);
	uint32 reserve_size = s_length + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	if (!arr.data)
	{
		arr.init(reserve_size, DarrayFlags::IS_STRING, AllocationTag::STRING);
	}
	else if (arr.capacity < reserve_size)
	{				
		arr.resize(reserve_size);		
	}

	arr.count = CString::copy(arr.capacity, arr.data, s);
	return *this;

}

void String::copy_n(const char* s, uint32 length)
{
	uint32 reserve_size = length;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	if (!arr.data)
	{
		arr.init(reserve_size, DarrayFlags::IS_STRING, AllocationTag::STRING);
	}
	else if (arr.capacity < reserve_size)
	{
		arr.resize(reserve_size);
	}

	arr.count = CString::copy(arr.capacity, arr.data, s, (int32)length);
}

void String::free_data()
{
	arr.free_data();
}

void String::append(char appendage)
{
	uint32 total_length = 1 + arr.count;
	if (total_length + 1 > arr.capacity)
		arr.resize(total_length + 1);
	CString::append(arr.capacity, arr.data, appendage);
	arr.count = total_length;
}

void String::append(const char* appendage, int32 length)
{
	uint32 append_length = length < 0 ? CString::length(appendage) : (uint32)length;
	uint32 total_length = append_length + arr.count;
	if (total_length + 1 > arr.capacity)
		arr.resize(total_length + 1);
	CString::append(arr.capacity, arr.data, appendage, length);
	arr.count = total_length;
}

void String::append(const String& appendage, int32 length)
{
	int32 append_length = length < 0 ? (int32)appendage.len() : (uint32)length;
	append(appendage.c_str(), append_length);
}

namespace CString
{
	void split(const char* s, Darray<String>& out_arr, char delimiter)
	{
		out_arr.clear();
		const char* ptr = s;

		while (*ptr)
		{
			int32 del_index = index_of(ptr, delimiter);
			if (del_index < 0)
				break;
			else if (del_index > 0)
			{
				String tmp(ptr, del_index);
				out_arr.push_steal(tmp);	
			}		
			ptr += del_index + 1;
		}

		if (*ptr)
		{
			String tmp(ptr);
			out_arr.push_steal(tmp);
		}
	}
}

// TODO: Figure out whether it's possible to write (initialized)s = mid(s1,0) without extra allocation and free of tpm string
//String mid(const String& source, uint32 start, int32 length)
//{
//	String s = source;
//	s.arr.count = CString::mid(s.arr.data, start, length);
//	return s;
//}
//
//String left_of_last(const String& source, char c)
//{
//	String s = source;
//	s.arr.count = CString::left_of_last(s.arr.data, c);
//	return s;
//}
//
//String right_of_last(const String& source, char c)
//{
//	String s = source;
//	s.arr.count = CString::right_of_last(s.arr.data, c);
//	return s;
//}

void mid(String& out_s, const char* source, uint32 start, int32 length)
{
	out_s = source;
	out_s.arr.count = CString::mid(out_s.arr.data, out_s.arr.count, start, length);
}

void left_of_last(String& out_s, const char* source, char c)
{
	out_s = source;
	out_s.arr.count = CString::left_of_last(out_s.arr.data, out_s.arr.count, c);
}

void right_of_last(String& out_s, const char* source, char c)
{
	out_s = source;
	out_s.arr.count = CString::right_of_last(out_s.arr.data, out_s.arr.count, c);
}

void trim(String& out_s, const char* other)
{
	out_s = other;
	out_s.arr.count = CString::trim(out_s.arr.data);
}

int32 print_s(String& out_s, const char* format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);

	int32 res = CString::print_s(out_s.arr.data, out_s.arr.capacity, format, arg_ptr);
	if (res >= 0)
		out_s.arr.count = (uint32)res;

	va_end(arg_ptr);

	return res;
}
