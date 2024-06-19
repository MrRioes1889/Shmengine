#include "../String.hpp"
#include "../CString.hpp"

String::String()
{
	arr.init(String::min_reserve_size, AllocationTag::STRING);
}

String::String(uint32 reserve_size)
{
	reserve_size++;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, AllocationTag::STRING);
}

String::String(const char* s, uint32 length)
{
	uint32 s_length = CString::length(s);
	if (s_length < length)
		length = s_length;

	uint32 reserve_size = length + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, AllocationTag::STRING);
	CString::copy(arr.size, arr.data, s, length);
	arr.count = length;
}

String::String(const char* s)
{
	uint32 s_length = CString::length(s);
	uint32 reserve_size = s_length + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	arr.init(reserve_size, AllocationTag::STRING);
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

	arr.init(reserve_size, AllocationTag::STRING);
	CString::copy(arr.size, arr.data, other.c_str());
	arr.count = other.len();
}

String& String::operator=(const String& other)
{

	uint32 reserve_size = other.len() + 1;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	if (!arr.data)
	{
		arr.init(reserve_size, AllocationTag::STRING);
	}
	else if (arr.size < reserve_size)
	{
		arr.resize(reserve_size);
	}

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
		arr.init(reserve_size, AllocationTag::STRING);
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

String::Array::Array()
{
	strings.init(String::Array::min_reserve_size, AllocationTag::MAIN);
}

String::Array::~Array()
{
	free_data();
}

String::Array::Array(const Array& other)
{
	uint32 reserve_size = other.strings.count;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	strings.init(reserve_size, AllocationTag::MAIN);
	for (uint32 i = 0; i < other.strings.count; i++)
		strings.push(other.strings.data[i]);
}

String::Array& String::Array::operator=(const Array& other)
{
	uint32 reserve_size = other.strings.count;
	if (reserve_size < String::min_reserve_size)
		reserve_size = String::min_reserve_size;

	strings.free_data();
	strings.init(reserve_size, AllocationTag::MAIN);
	for (uint32 i = 0; i < other.strings.count; i++)
		strings.push(other.strings.data[i]);
	return *this;
}

String::Array::Array(Array&& other) noexcept
{
	strings.data = other.strings.data;
	strings.size = other.strings.size;
	strings.count = other.strings.count;

	other.strings.data = 0;
	other.strings.size = 0;
	other.strings.count = 0;
}

String::Array& String::Array::operator=(Array&& other) noexcept
{
	strings.data = other.strings.data;
	strings.size = other.strings.size;
	strings.count = other.strings.count;

	other.strings.data = 0;
	other.strings.size = 0;
	other.strings.count = 0;
	return *this;
}

void String::Array::free_data()
{
	for (uint32 i = 0; i < strings.count; i++)
		strings[i].free_data();
	strings.free_data();
}

namespace CString
{
	String::Array split(const char* s, char delimiter)
	{
		String::Array arr;
		const char* ptr = s;

		while (*ptr)
		{
			int32 del_index = index_of(ptr, delimiter);
			if (del_index < 0)
				break;
			else if (del_index > 0)
			{
				String tmp(ptr, del_index);
				arr.strings.push(tmp);
			}		
			ptr += del_index + 1;
		}

		if (*ptr)
		{
			String tmp(ptr);
			arr.strings.push(tmp);
		}

		return arr;
	}
}
