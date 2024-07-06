#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Assert.hpp"
#include "Darray.hpp"

namespace SarrayFlag
{
	enum Value
	{
		EXTERNAL_MEMORY = 1 << 0
	};
}

template<typename T>
struct Sarray
{

	Sarray() : count(0), data(0), allocation_tag((uint16)AllocationTag::UNKNOWN), flags(0) {};
	SHMINLINE Sarray(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	SHMINLINE ~Sarray();

	SHMINLINE Sarray(const Sarray& other);
	SHMINLINE Sarray& operator=(const Sarray& other);
	SHMINLINE Sarray(Sarray&& other) noexcept;
	SHMINLINE Sarray& operator=(Sarray&& other);

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	SHMINLINE void free_data();

	SHMINLINE void clear();

	SHMINLINE T* transfer_data();

	SHMINLINE void copy_memory(const void* source, uint64 size);

	SHMINLINE void steal(Sarray<T>& other)
	{
		data = other.data;
		count = other.count;
		flags = other.flags;
		allocation_tag = other.allocation_tag;

		other.data = 0;
		other.count = 0;
	}

	SHMINLINE void steal(Darray<T>& other)
	{
		data = other.data;
		count = other.size;
		flags = 0;
		allocation_tag = other.allocation_tag;

		other.data = 0;
		other.size = 0;
		other.count = 0;
	}

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

	template<typename SubT>
	SHMINLINE SubT& get_as(uint32 index)
	{
		uint32 sub_t_max_count = (sizeof(T) * count) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_count, "Index does not lie within bounds of Sarray.");
		return ((SubT*)data)[index];
	}

	template<typename SubT>
	SHMINLINE const SubT& get_as(uint32 index) const
	{
		uint32 sub_t_max_count = (sizeof(T) * count) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_count, "Index does not lie within bounds of Sarray.");
		return ((SubT*)data)[index];
	}
	
	T* data = 0;
	uint32 count = 0;
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
	if (data && !(flags & SarrayFlag::EXTERNAL_MEMORY))
		Memory::free_memory(data, true, (AllocationTag)allocation_tag);
}

template<typename T>
SHMINLINE Sarray<T>::Sarray(const Sarray& other)
{
	init(other.count, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.count; i++)
		data[i] = other[i];
}

template<typename T>
SHMINLINE Sarray<T>& Sarray<T>::operator=(const Sarray& other)
{
	free_data();
	init(other.count, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.count; i++)
		data[i] = other[i];
	return *this;
}


template<typename T>
SHMINLINE Sarray<T>::Sarray(Sarray&& other) noexcept
{
	free_data();
	data = other.data;
	count = other.count;
	allocation_tag = other.allocation_tag;

	other.data = 0;
	other.count = 0;
}

template<typename T>
SHMINLINE Sarray<T>& Sarray<T>::operator=(Sarray&& other)
{
	data = other.data;
	count = other.count;
	allocation_tag = other.allocation_tag;

	other.data = 0;
	other.count = 0;
	return *this;
}

template<typename T>
SHMINLINE void Sarray<T>::init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag, void* memory)
{
	SHMASSERT_MSG(!data, "Cannot init non empty sarray!");

	allocation_tag = (uint16)tag;
	count = reserve_count;
	flags = (uint16)creation_flags;
	if (!memory)
		data = (T*)Memory::allocate(sizeof(T) * reserve_count, true, (AllocationTag)allocation_tag);
	else
		data = (T*)memory;

}

template<typename T>
SHMINLINE void Sarray<T>::free_data()
{
	if (data && !(flags & SarrayFlag::EXTERNAL_MEMORY))
	{
		for (uint32 i = 0; i < count; i++)
			data[i].~T();

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

template<typename T>
inline SHMINLINE T* Sarray<T>::transfer_data()
{
	T* ret = data;

	data = 0;
	count = 0;

	return ret;
}

template<typename T>
SHMINLINE void Sarray<T>::copy_memory(const void* source, uint64 size)
{
	SHMASSERT_MSG(size <= (sizeof(T) * count), "Sarray does not fit requested size!");
	Memory::copy_memory(source, data, size);
}