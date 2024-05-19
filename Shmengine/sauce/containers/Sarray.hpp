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

	Sarray() : stride(0), size(0), data(0) {};
	Sarray(uint32 reserve_count);
	~Sarray();

	// NOTE: Call for already instantiated arrays
	void init(uint32 reserve_count);

	T& operator[](const uint32& index)
	{
		SHMASSERT_MSG(index + 1 <= size, "Index does not lie within bounds of Sarray.");
		return data[index];
	}

	uint32 stride;
	uint32 size;
	T* data;	

};

template<typename T>
inline Sarray<T>::Sarray(uint32 reserve_count)
{
	stride = sizeof(T);
	size = reserve_count;
	data = (T*)Memory::allocate(sizeof(T) * reserve_count, true);
}

template<typename T>
inline Sarray<T>::~Sarray()
{
	if (data)
		Memory::free_memory(data, true);
}

template<typename T>
inline void Sarray<T>::init(uint32 reserve_count)
{
	SHMASSERT_MSG(!data, "Cannot initialize Sarray with existing data!");

	stride = sizeof(T);
	size = reserve_count;
	data = (T*)Memory::allocate(sizeof(T) * reserve_count, true);
}
