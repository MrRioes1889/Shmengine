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

	Sarray() : count(0), data(0) {};
	Sarray(uint32 reserve_count);
	~Sarray();

	// NOTE: Call for already instantiated arrays
	void init(uint32 reserve_count);
	void free_data();

	void clear();

	T& operator[](const uint32& index)
	{
		SHMASSERT_MSG(index + 1 <= count, "Index does not lie within bounds of Sarray.");
		return data[index];
	}
	
	T* data;
	uint32 count;

};

template<typename T>
SHMINLINE Sarray<T>::Sarray(uint32 reserve_count)
{
	init(reserve_count);
}

template<typename T>
SHMINLINE Sarray<T>::~Sarray()
{
	if (data)
		Memory::free_memory(data, true);
}

template<typename T>
SHMINLINE void Sarray<T>::init(uint32 reserve_count)
{
	//SHMASSERT_MSG(!data, "Cannot initialize Sarray with existing data!");
	if (data)
		free_data();	

	count = reserve_count;
	data = (T*)Memory::allocate(sizeof(T) * reserve_count, true);
	clear();
}

template<typename T>
SHMINLINE void Sarray<T>::free_data()
{
	if (data)
		Memory::free_memory(data, true);

	data = 0;
	count = 0;
}

template<typename T>
SHMINLINE void Sarray<T>::clear()
{
	Memory::zero_memory(data, sizeof(T) * count);
}