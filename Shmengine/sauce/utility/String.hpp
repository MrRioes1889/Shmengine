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

	void free_data();

	SHMINLINE char& operator[](uint32 index) { return arr.data[index]; }

	SHMINLINE bool32 equal(const char* other) { return CString::equal(arr.data, other); }
	SHMINLINE bool32 equal(const String& other) { return CString::equal(arr.data, other.c_str()); }
	SHMINLINE bool32 operator==(const char* other) { return CString::equal(arr.data, other); }
	SHMINLINE bool32 operator==(const String& other) { return CString::equal(arr.data, other.c_str()); }

	SHMINLINE bool32 equal_i(const char* other) { return CString::equal_i(arr.data, other); }
	SHMINLINE bool32 equal_i(const String& other) { return CString::equal_i(arr.data, other.c_str()); }
	SHMINLINE bool32 nequal(const char* other, uint32 length) { return CString::nequal(arr.data, other, length); }
	SHMINLINE bool32 nequal(const String& other, uint32 length) { return CString::nequal(arr.data, other.c_str(), length); }
	SHMINLINE bool32 nequal_i(const char* other, uint32 length) { return CString::nequal_i(arr.data, other, length); }
	SHMINLINE bool32 nequal_i(const String& other, uint32 length) { return CString::nequal_i(arr.data, other.c_str(), length); }

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

	SHMINLINE int32 index_of(char c) { return CString::index_of(arr.data, c); }
	SHMINLINE int32 index_of_last(char c) { return CString::index_of_last(arr.data, c); }
	SHMINLINE bool32 is_empty() { return arr.count > 0; }

	SHMINLINE const char* c_str() const { return arr.data; }
	//SHMINLINE operator const char* () { return arr.data; }
	SHMINLINE void update_len() { arr.count = CString::length(arr.data); }
	SHMINLINE uint32 len() const { return arr.count; }

	SHMINLINE void split(Darray<String>& out_arr, char delimiter);

	Darray<char> arr;

};

//SHMINLINE bool32 equal(const String& s, const char* other) { return CString::equal(s.c_str(), other); }
//SHMINLINE bool32 equal(const String& s, const String& other) { return CString::equal(s.c_str(), other.c_str()); }
//SHMINLINE bool32 equal_i(const String& s, const char* other) { return CString::equal_i(s.c_str(), other); }
//SHMINLINE bool32 equal_i(const String& s, const String& other) { return CString::equal_i(s.c_str(), other.c_str()); }
//SHMINLINE bool32 nequal(const String& s, const char* other, uint32 length) { return CString::nequal(s.c_str(), other, length); }
//SHMINLINE bool32 nequal(const String& s, const String& other, uint32 length) { return CString::nequal(s.c_str(), other.c_str(), length); }
//SHMINLINE bool32 nequal_i(const String& s, const char* other, uint32 length) { return CString::nequal_i(s.c_str(), other, length); }
//SHMINLINE bool32 nequal_i(const String& s, const String& other, uint32 length) { return CString::nequal_i(s.c_str(), other.c_str(), length); }

namespace CString
{
	SHMAPI void split(const char* s, Darray<String>& out_arr, char delimiter);
}

//SHMAPI String mid(const String& source, uint32 start, int32 length = -1);
//SHMAPI String left_of_last(const String& source, char c);
//SHMAPI String right_of_last(const String& source, char c);
SHMAPI void mid(String& out_s, const char* source, uint32 start, int32 length = -1);
SHMAPI void left_of_last(String& out_s, const char* source, char c);
SHMAPI void right_of_last(String& out_s, const char* source, char c);
SHMAPI void trim(String& out_s, const char* other);

// TODO: Implement print_s properly for dynamically sized strings!
// TEMP
SHMAPI int32 print_s(String& out_s, const char* format, ...);

template<typename... Args>
SHMINLINE int32 safe_print_s(String& out_s, const char* format, const Args&... args)
{
	CString::Arg arg_array[] = { args... };
	int32 res = CString::_print_s_base(out_s.arr.data, out_s.arr.capacity, format, arg_array, sizeof...(Args));
	if (res >= 0)
		out_s.arr.count = (uint32)res;
	
	return res;
}
// END

SHMINLINE void String::split(Darray<String>& out_arr, char delimiter) 
{ 
	return CString::split(arr.data, out_arr, delimiter);
}


