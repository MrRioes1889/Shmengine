#pragma once

#include "Defines.hpp"

#include "Sarray.hpp"
#include "core/Memory.hpp"
#include "core/Assert.hpp"
#include "core/Logging.hpp"

template <typename T>
struct Hashtable
{
	Hashtable(const Hashtable& other) = delete;
	Hashtable(Hashtable&& other) = delete;
	Hashtable() : element_count(0) {};

	SHMINLINE Hashtable(uint32 count, AllocationTag tag);
	SHMINLINE Hashtable(uint32 count, void* memory);
	SHMINLINE void init(uint32 count, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);

	SHMINLINE void free_data();
	SHMINLINE ~Hashtable();

	SHMINLINE bool32 set_value(const char* name, const T& value);
	SHMINLINE T get_value(const char* name);

	SHMINLINE bool32 floodfill(const T& value);

	uint32 element_count;
	Sarray<T> buffer = {};

private:

	SHMINLINE uint32 hash_name(const char* name);
	
};

template<typename T>
SHMINLINE uint32 Hashtable<T>::hash_name(const char* name)
{
	const uint64 multiplier = 97;

	unsigned const char* us;
	uint64 hash = 0;

	for (us = (unsigned const char*)name; *us; us++)
	{
		hash = hash * multiplier + *us;
	}

	hash %= element_count;
	return (uint32)hash;
}

template <typename T>
SHMINLINE Hashtable<T>::Hashtable(uint32 count, AllocationTag tag)
{
	init(count, tag);
}

template <typename T>
SHMINLINE Hashtable<T>::Hashtable(uint32 count, void* memory)
{
	init(count, AllocationTag::UNKNOWN, memory);
}

template<typename T>
SHMINLINE void Hashtable<T>::init(uint32 count, AllocationTag tag, void* memory)
{
	SHMASSERT_MSG(count, "Element count cannot be null!");

	element_count = count;
	buffer.init(sizeof(T) * element_count, tag, memory);
}

template<typename T>
inline SHMINLINE void Hashtable<T>::free_data()
{
	buffer.free_data();
	element_count = 0;
}

template<typename T>
SHMINLINE Hashtable<T>::~Hashtable()
{
	free_data();
}

template<typename T>
SHMINLINE bool32 Hashtable<T>::set_value(const char* name, const T& value)
{
	uint32 hash = hash_name(name);
	buffer[hash] = value;
	return true;
}

template<typename T>
SHMINLINE T Hashtable<T>::get_value(const char* name)
{
	uint32 hash = hash_name(name);
	return buffer[hash];
}

template<typename T>
SHMINLINE bool32 Hashtable<T>::floodfill(const T& value)
{
	for (uint32 i = 0; i < element_count; i++)
	{
		buffer[i] = value;
	}
	return true;
}


