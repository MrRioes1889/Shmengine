#pragma once

#include "Defines.hpp"
#include "CString.hpp"
#include "containers/Darray.hpp"

struct String
{

	static const uint32 min_reserve_size = 16;

	String();
	String(uint32 reserve_size);
	String(const char* s);
	String(const char* s, uint32 length);
	~String();

	String(const String& other);
	String& operator=(const String& other);

	String(String&& other) noexcept;
	String& operator=(String&& other) noexcept;

	String& operator=(const char* s);

	void free_data();

	SHMINLINE char& operator[](uint32 index) { return arr.data[index]; }

	SHMINLINE bool32 equal(const char* other) { return CString::equal(arr.data, other); }
	SHMINLINE bool32 equal(const String& other) { return CString::equal(arr.data, other.c_str()); }
	SHMINLINE bool32 operator==(const char* other) { return CString::equal(arr.data, other); }
	SHMINLINE bool32 operator==(const String& other) { return CString::equal(arr.data, other.c_str()); }

	SHMINLINE bool32 equal_i(const char* other) { return CString::equal_i(arr.data, other); }
	SHMINLINE bool32 equal_i(const String& other) { return CString::equal_i(arr.data, other.c_str()); }

	void append(char appendage);
	void append(const char* appendage);
	void append(const String& appendage);
	SHMINLINE String operator+(char appendage) { String s = *this; s.append(appendage); return s; }
	SHMINLINE String operator+(const char* appendage) { String s = *this; s.append(appendage); return s; }
	SHMINLINE String operator+(const String& appendage) { String s = *this; s.append(appendage); return s; }

	SHMINLINE void trim() { arr.count = CString::trim(arr.data); }
	SHMINLINE void mid(uint32 start, int32 length = -1) { arr.count = CString::mid(arr.data, start, length); }

	SHMINLINE int32 index_of(char c) { return CString::index_of(arr.data, c); }
	SHMINLINE bool32 is_empty() { return arr.count > 0; }

	SHMINLINE const char* c_str() const { return arr.data; }
	SHMINLINE uint32 len() const { return arr.count; }

	SHMINLINE Darray<String> split(char delimiter);

	Darray<char> arr = {};

};

namespace CString
{
	Darray<String> split(const char* s, char delimiter);
}

String mid(const String& source, uint32 start, int32 length = -1);
String trim(const String& other);

SHMINLINE Darray<String> String::split(char delimiter) 
{ 
	return CString::split(arr.data, delimiter); 
}


