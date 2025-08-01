#pragma once

#include "Defines.hpp"
#include "CString.hpp"
#include "containers/Darray.hpp"

struct SHMAPI String
{

	static const uint32 min_reserve_size = 16;

	String();
	String(uint32 reserve_size);
	String(const char* s);
	String(const char* s, uint32 length);
	~String();

	String(const String& other);
	String& operator=(const String& other);

	// TODO: clang-cl : For some reason these copy/move constructors mess up debug symbols in importing libraries. Might have something todo with custom allocation(?)
	String(String&& other) noexcept;
	String& operator=(String&& other) noexcept;

	String& operator=(const char* s);
	void copy_n(const char* s, uint32 length);

	void reserve(uint32 reserve_size);
	void free_data();

	SHMINLINE char& operator[](uint32 index) { return arr.data[index]; }
	SHMINLINE const char& operator[](uint32 index) const { return arr.data[index]; }

	SHMINLINE bool8 equal(const char* other) { return CString::equal(arr.data, other); }
	SHMINLINE bool8 equal(const String& other) { return CString::equal(arr.data, other.c_str()); }
	SHMINLINE bool8 operator==(const char* other) { return equal(other); }
	SHMINLINE bool8 operator==(const String& other) { return equal(other); }

	SHMINLINE bool8 equal_i(const char* other) { return CString::equal_i(arr.data, other); }
	SHMINLINE bool8 equal_i(const String& other) { return CString::equal_i(arr.data, other.c_str()); }
	SHMINLINE bool8 nequal(const char* other, uint32 length) { return CString::nequal(arr.data, other, length); }
	SHMINLINE bool8 nequal(const String& other, uint32 length) { return CString::nequal(arr.data, other.c_str(), length); }
	SHMINLINE bool8 nequal_i(const char* other, uint32 length) { return CString::nequal_i(arr.data, other, length); }
	SHMINLINE bool8 nequal_i(const String& other, uint32 length) { return CString::nequal_i(arr.data, other.c_str(), length); }

	void append(char appendage);
	void append(const char* appendage, int32 length = -1);
	void append(const String& appendage, int32 length = -1);
	SHMINLINE void operator+=(char appendage) { append(appendage); }
	SHMINLINE void operator+=(const char* appendage) { append(appendage); }
	SHMINLINE void operator+=(const String& appendage) { append(appendage); }
	SHMINLINE String operator+(char appendage) { String s = *this; s.append(appendage); return s; }
	SHMINLINE String operator+(const char* appendage) { String s = *this; s.append(appendage); return s; }
	SHMINLINE String operator+(const String& appendage) { String s = *this; s.append(appendage); return s; }

	SHMINLINE void pop() { if (!arr.count) return; arr[arr.count - 1] = 0; arr.pop(); }
	SHMINLINE void trim() { arr.count = CString::trim(arr.data); }
	SHMINLINE void mid(uint32 start, int32 length = -1) { arr.count = CString::mid(arr.data, arr.count, start, length); }
	SHMINLINE void left_of_last(char c) { arr.count = CString::left_of_last(arr.data, arr.count, c); }
	SHMINLINE void right_of_last(char c) { arr.count = CString::right_of_last(arr.data, arr.count, c); }

	SHMINLINE int32 index_of(char c) const { return CString::index_of(arr.data, c); }
	SHMINLINE int32 index_of_last(char c) const { return CString::index_of_last(arr.data, c); }
	SHMINLINE bool8 is_empty() const { return !arr.data || !arr.count; }
	SHMINLINE char first() const { return arr[0]; }
	SHMINLINE char last() const { return arr[arr.count-1]; }

	SHMINLINE const char* c_str() const { return arr.data; }
	SHMINLINE char* c_str_vulnerable() { return arr.data; }
	//SHMINLINE operator const char* () { return arr.data; }
	SHMINLINE void update_len() { arr.count = CString::length(arr.data); }
	SHMINLINE uint32 len() const { return arr.count; }

	SHMINLINE void split(Darray<String>& out_arr, char delimiter);

	int32 print_s(const char* format, ...);

	// TODO: Implement print_s properly for dynamically sized strings!
	template<typename... Args>
	SHMINLINE int32 safe_print_s(const char* format, const Args&... args)
	{
		if (!arr.data)
			return -1;

		CString::PrintArg arg_array[] = { args... };
		int32 res = CString::_print_s_base(arr.data, arr.capacity, format, arg_array, sizeof...(Args));
		if (res >= 0)
			arr.count = (uint32)res;
		
		return res;
	}

private:
	Darray<char> arr;
};

namespace CString
{
	SHMAPI void split(const char* s, Darray<String>& out_arr, char delimiter);
}

SHMAPI void mid(String& out_s, const char* source, uint32 start, int32 length = -1);
SHMAPI void left_of_last(String& out_s, const char* source, char c);
SHMAPI void right_of_last(String& out_s, const char* source, char c);
SHMAPI void trim(String& out_s, const char* other);

SHMINLINE void String::split(Darray<String>& out_arr, char delimiter) 
{ 
	return CString::split(arr.data, out_arr, delimiter);
}

struct SHMAPI StringRef
{

	StringRef();
	StringRef(const char* s);
	StringRef(const char* s, uint32 offset);
	StringRef(const char* s, uint32 offset, uint32 length);
	StringRef(const String* s);
	StringRef(const String* s, uint32 offset);
	StringRef(const String* s, uint32 offset, uint32 length);

	void operator=(const char* s);
	void operator=(const String* s);

	void set_text(const char* s, uint32 offset = 0, uint32 length = Constants::max_u32);
	void set_text(const String* s, uint32 offset = 0, uint32 length = Constants::max_u32);

	SHMINLINE void set_ref_offset(uint32 offset) { ref_offset = SHMIN(offset, full_length); ref_length = SHMIN(ref_length, full_length - ref_offset); }
	SHMINLINE void set_ref_length(uint32 length) { ref_length = SHMIN(length, full_length - ref_offset); }
	SHMINLINE char operator[](uint32 index) { return index <= ref_length - 1 ? str[ref_offset + index] : 0; }

	SHMINLINE bool8 nequal(const char* other) { return CString::nequal((str + ref_offset), other, ref_length); }
	SHMINLINE bool8 nequal(const String& other) { return CString::nequal((str + ref_offset), other.c_str(), ref_length); }
	SHMINLINE bool8 operator==(const char* other) { return nequal(other); }
	SHMINLINE bool8 operator==(const String& other) { return nequal(other); }

	SHMINLINE bool8 nequal_i(const char* other, uint32 length) { return CString::nequal_i((str + ref_offset), other, ref_length); }
	SHMINLINE bool8 nequal_i(const String& other, uint32 length) { return CString::nequal_i((str + ref_offset), other.c_str(), ref_length); }

	SHMINLINE bool8 is_empty() { return !str || !str[ref_offset]; }
	SHMINLINE char first() { return str[ref_offset]; }
	SHMINLINE char last() { return str[ref_offset + ref_length - 1]; }

	SHMINLINE const char* c_str() const { return (str + ref_offset); }
	SHMINLINE uint32 len() const { return ref_length; }

private:
	const char* str;
	uint32 full_length;
	uint32 ref_offset;
	uint32 ref_length;
};
