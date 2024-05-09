#pragma once

#include "Defines.hpp"
#include "core/Logging.hpp"
#include "core/Assert.hpp"

template<typename T>
struct Sarray
{

	Sarray() = delete;

	Sarray(uint32 reserve_count);
	~Sarray();

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
	Memory::free_memory(data, true);
}
