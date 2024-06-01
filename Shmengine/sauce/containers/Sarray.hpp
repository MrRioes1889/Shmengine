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

	Sarray() : count(0), data(0), allocation_tag(AllocationTag::UNKNOWN) {};
	SHMINLINE Sarray(uint32 reserve_count, AllocationTag tag = AllocationTag::UNKNOWN);
	SHMINLINE ~Sarray();

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, AllocationTag tag = AllocationTag::UNKNOWN);
	SHMINLINE void free_data();

	SHMINLINE void clear();

	T& operator[](const uint32& index)
	{
		SHMASSERT_MSG(index + 1 <= count, "Index does not lie within bounds of Sarray.");
		return data[index];
	}
	
	T* data;
	uint32 count;
	AllocationTag allocation_tag;

};

template<typename T>
SHMINLINE Sarray<T>::Sarray(uint32 reserve_count, AllocationTag tag)
{
	init(reserve_count, tag);
}

template<typename T>
SHMINLINE Sarray<T>::~Sarray()
{
	if (data)
		Memory::free_memory(data, true, allocation_tag);
}

template<typename T>
SHMINLINE void Sarray<T>::init(uint32 reserve_count, AllocationTag tag)
{
	//SHMASSERT_MSG(!data, "Cannot initialize Sarray with existing data!");
	if (data)
		free_data();	

	allocation_tag = tag;
	count = reserve_count;
	data = (T*)Memory::allocate(sizeof(T) * reserve_count, true, allocation_tag);
	clear();
}

template<typename T>
SHMINLINE void Sarray<T>::free_data()
{
	if (data)
		Memory::free_memory(data, true, allocation_tag);

	data = 0;
	count = 0;
}

template<typename T>
SHMINLINE void Sarray<T>::clear()
{
	Memory::zero_memory(data, sizeof(T) * count);
}