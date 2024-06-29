#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Assert.hpp"

template<typename T>
struct Sarray
{
	Sarray(const Sarray& other) = delete;
	Sarray(Sarray&& other) = delete;

	Sarray() : count(0), data(0), allocation_tag((uint16)AllocationTag::UNKNOWN) {};
	SHMINLINE Sarray(uint32 reserve_count, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	SHMINLINE ~Sarray();

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	SHMINLINE void free_data();

	SHMINLINE void clear();

	SHMINLINE T& operator[](uint32 index)
	{
		SHMASSERT_MSG(index + 1 <= count, "Index does not lie within bounds of Sarray.");
		return data[index];
	}

	SHMINLINE const T& operator[](uint32 index) const
	{
		SHMASSERT_MSG(index + 1 <= count, "Index does not lie within bounds of Sarray.");
		return data[index];
	}
	
	T* data = 0;
	uint32 count = 0;
	uint16 allocation_tag;
	bool8 owns_memory = false;

};

template<typename T>
SHMINLINE Sarray<T>::Sarray(uint32 reserve_count, AllocationTag tag, void* memory)
{
	data = 0;
	init(reserve_count, tag, memory);
}

template<typename T>
SHMINLINE Sarray<T>::~Sarray()
{
	if (data && owns_memory)
		Memory::free_memory(data, true, (AllocationTag)allocation_tag);
}

template<typename T>
SHMINLINE void Sarray<T>::init(uint32 reserve_count, AllocationTag tag, void* memory)
{
	SHMASSERT_MSG(!data, "Cannot init non empty sarray!");

	owns_memory = (memory == 0);
	allocation_tag = (uint16)tag;
	count = reserve_count;
	if (owns_memory)
		data = (T*)Memory::allocate(sizeof(T) * reserve_count, true, (AllocationTag)allocation_tag);
	else
		data = (T*)memory;

	clear();
}

template<typename T>
SHMINLINE void Sarray<T>::free_data()
{
	if (data)
	{
		for (uint32 i = 0; i < count; i++)
			data[i].~T();

		if (owns_memory)
			Memory::free_memory(data, true, (AllocationTag)allocation_tag);
	}
		
	data = 0;
	count = 0;
}

template<typename T>
SHMINLINE void Sarray<T>::clear()
{
	for (uint32 i = 0; i < count; i++)
		data[i].~T();

	Memory::zero_memory(data, sizeof(T) * count);
}