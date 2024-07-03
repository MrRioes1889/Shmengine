#include "../String.hpp"
#include "../CString.hpp"

String::String()
{
	arr.init(String::min_reserve_size, 0, AllocationTag::STRING);
}

String::String(uint32 reserve_size)
{
	reserve_size++;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, 0, AllocationTag::STRING);
}

String::String(const char* s, uint32 length)
{
	uint32 s_length = CString::length(s);
	if (s_length < length)
		length = s_length;

	uint32 reserve_size = length + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, 0, AllocationTag::STRING);
	CString::copy(arr.size, arr.data, s, length);
	arr.count = length;
}

String::String(const char* s)
{
	uint32 s_length = CString::length(s);
	uint32 reserve_size = s_length + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, 0, AllocationTag::STRING);
	CString::copy(arr.size, arr.data, s);
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

	arr.init(reserve_size, 0, AllocationTag::STRING);
	CString::copy(arr.size, arr.data, other.c_str());
	arr.count = other.len();
}

String& String::operator=(const String& other)
{

	free_data();
	uint32 reserve_size = other.len() + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, 0, AllocationTag::STRING);

	CString::copy(arr.size, arr.data, other.c_str());
	arr.count = other.len();
	arr.push(0);
	return *this;

}

String::String(String&& other) noexcept
{
	arr.data = other.arr.data;
	arr.size = other.arr.size;
	arr.count = other.arr.count;

	other.arr.data = 0;
	other.arr.size = 0;
	other.arr.count = 0;
}

String& String::operator=(String&& other) noexcept
{
    free_data();
	arr.data = other.arr.data;
	arr.size = other.arr.size;
	arr.count = other.arr.count;

	other.arr.data = 0;
	other.arr.size = 0;
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
		arr.init(reserve_size, 0, AllocationTag::STRING);
	}
	else if (arr.size < reserve_size)
	{				
		arr.resize(reserve_size);		
	}

	CString::copy(arr.size, arr.data, s);
	arr.count = s_length;
	arr.push(0);
	return *this;

}

void String::free_data()
{
	arr.free_data();
}

void String::append(char appendage)
{
	uint32 total_length = 1 + arr.count;
	if (total_length + 1 > arr.size)
		arr.resize(total_length + 1);
	CString::append(arr.size, arr.data, appendage);
}

void String::append(const char* appendage)
{
	uint32 total_length = CString::length(appendage) + arr.count;
	if (total_length + 1 > arr.size)
		arr.resize(total_length + 1);
	CString::append(arr.size, arr.data, appendage);
}

void String::append(const String& appendage)
{
	append(appendage.c_str());
}

namespace CString
{
	Darray<String> split(const char* s, char delimiter)
	{
		Darray<String> arr(1, 0, AllocationTag::MAIN);
		const char* ptr = s;

		while (*ptr)
		{
			int32 del_index = index_of(ptr, delimiter);
			if (del_index < 0)
				break;
			else if (del_index > 0)
			{
				String tmp(ptr, del_index);
				arr.push(tmp);
			}		
			ptr += del_index + 1;
		}

		if (*ptr)
		{
			String tmp(ptr);
			arr.push(tmp);
		}

		return arr;
	}
}

String mid(const String& source, uint32 start, int32 length)
{
	String s = source;
	s.arr.count = CString::mid(s.arr.data, start, length);
	return s;
}

String left_of_last(const String& source, char c)
{
	String s = source;
	s.arr.count = CString::left_of_last(s.arr.data, c);
	return s;
}

String right_of_last(const String& source, char c)
{
	String s = source;
	s.arr.count = CString::right_of_last(s.arr.data, c);
	return s;
}

String trim(const String& other)
{
	String s = other;
	s.arr.count = CString::trim(s.arr.data);
	return s;
}

int32 print_s(String& out_s, const char* format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);

	int32 res = CString::print_s(out_s.arr.data, out_s.arr.size, format, arg_ptr);
	if (res >= 0)
		out_s.arr.count = (uint32)res;

	va_end(arg_ptr);

	return res;
}
