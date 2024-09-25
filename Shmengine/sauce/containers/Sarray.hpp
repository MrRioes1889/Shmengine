#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Assert.hpp"
#include "Darray.hpp"

namespace SarrayFlags
{
	enum Value
	{
		EXTERNAL_MEMORY = 1 << 0
	};
}

template<typename T>
struct Sarray
{

	Sarray() : capacity(0), data(0), allocation_tag((uint16)AllocationTag::ARRAY), flags(0) {};
	SHMINLINE Sarray(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::ARRAY, void* memory = 0);
	SHMINLINE ~Sarray();

	SHMINLINE Sarray(const Sarray& other);
	SHMINLINE Sarray& operator=(const Sarray& other);
	SHMINLINE Sarray(Sarray&& other) noexcept;
	SHMINLINE Sarray& operator=(Sarray&& other);

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::ARRAY, void* memory = 0);
	SHMINLINE void free_data();

	SHMINLINE void resize(uint32 new_count, void* memory = 0);
	SHMINLINE void clear();

	SHMINLINE T* transfer_data();

	SHMINLINE void copy_memory(const void* source, uint64 size, uint64 offset = 0);

	SHMINLINE void steal(Sarray<T>& other)
	{
		data = other.data;
		capacity = other.capacity;
		flags = other.flags;
		allocation_tag = other.allocation_tag;

		other.data = 0;
		other.capacity = 0;
	}

	SHMINLINE void steal(Darray<T>& other)
	{
		data = other.data;
		capacity = other.capacity;
		flags = 0;
		allocation_tag = other.allocation_tag;

		other.data = 0;
		other.capacity = 0;
		other.count = 0;
	}

	SHMINLINE T& operator[](uint64 index)
	{
		SHMASSERT_MSG(index + 1 <= capacity, "Index does not lie within bounds of Sarray.");
		return data[index];
	}

	SHMINLINE const T& operator[](uint64 index) const
	{
		SHMASSERT_MSG(index + 1 <= capacity, "Index does not lie within bounds of Sarray.");
		return data[index];
	}

	template<typename SubT>
	SHMINLINE SubT& get_as(uint32 index)
	{
		uint32 sub_t_max_count = (sizeof(T) * capacity) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_count, "Index does not lie within bounds of Sarray.");
		return ((SubT*)data)[index];
	}

	template<typename SubT>
	SHMINLINE const SubT& get_as(uint32 index) const
	{
		uint32 sub_t_max_count = (sizeof(T) * capacity) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_count, "Index does not lie within bounds of Sarray.");
		return ((SubT*)data)[index];
	}
	
	T* data = 0;
	uint32 capacity = 0;
	uint16 allocation_tag;
	uint16 flags;

};

template<typename T>
SHMINLINE Sarray<T>::Sarray(uint32 reserve_count, uint32 creation_flags, AllocationTag tag, void* memory)
{
	data = 0;
	init(reserve_count, creation_flags, tag, memory);
}

template<typename T>
SHMINLINE Sarray<T>::~Sarray()
{
	free_data();
}

template<typename T>
SHMINLINE Sarray<T>::Sarray(const Sarray& other)
{
	init(other.capacity, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.capacity; i++)
		data[i] = other[i];
}

template<typename T>
SHMINLINE Sarray<T>& Sarray<T>::operator=(const Sarray& other)
{
	free_data();
	init(other.capacity, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.capacity; i++)
		data[i] = other[i];
	return *this;
}


template<typename T>
SHMINLINE Sarray<T>::Sarray(Sarray&& other) noexcept
{	
	data = other.data;
	capacity = other.capacity;
	allocation_tag = other.allocation_tag;

	other.data = 0;
	other.capacity = 0;
}

template<typename T>
SHMINLINE Sarray<T>& Sarray<T>::operator=(Sarray&& other)
{
	free_data();
	data = other.data;
	capacity = other.capacity;
	allocation_tag = other.allocation_tag;

	other.data = 0;
	other.capacity = 0;
	return *this;
}

template<typename T>
SHMINLINE void Sarray<T>::init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag, void* memory)
{
	SHMASSERT_MSG(!data || ((flags & SarrayFlags::EXTERNAL_MEMORY) != 0), "Cannot init non empty sarray!");

	if (!reserve_count)
		return;

	allocation_tag = (uint16)tag;
	capacity = reserve_count;
	flags = (uint16)creation_flags;
	if (!memory)
		data = (T*)Memory::allocate(sizeof(T) * reserve_count, (AllocationTag)allocation_tag);
	else
		data = (T*)memory;

}

template<typename T>
SHMINLINE void Sarray<T>::free_data()
{
	if (data && !(flags & SarrayFlags::EXTERNAL_MEMORY))
	{
		for (uint32 i = 0; i < capacity; i++)
			data[i].~T();

		Memory::free_memory(data);
	}
		
	data = 0;
	capacity = 0;
}


template<typename T>
SHMINLINE void Sarray<T>::resize(uint32 new_count, void* memory)
{
	if (data && !(flags & SarrayFlags::EXTERNAL_MEMORY))
		data = Memory::reallocate(new_count * sizeof(T), memory);
	else
		data = memory;

	capacity = new_count;
}

template<typename T>
SHMINLINE void Sarray<T>::clear()
{
	for (uint32 i = 0; i < capacity; i++)
		data[i].~T();

	Memory::zero_memory(data, sizeof(T) * capacity);
}

template<typename T>
inline SHMINLINE T* Sarray<T>::transfer_data()
{
	T* ret = data;

	data = 0;
	capacity = 0;

	return ret;
}

template<typename T>
SHMINLINE void Sarray<T>::copy_memory(const void* source, uint64 size, uint64 offset)
{
	SHMASSERT_MSG((size + offset) <= (sizeof(T) * capacity), "Sarray does not fit requested size!");
	uint8* dest = ((uint8*)data) + offset;
	Memory::copy_memory(source, dest, size);
}