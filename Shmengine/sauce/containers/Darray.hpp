#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "core/Assert.hpp"

#define DARRAY_DEFAULT_SIZE 1
#define DARRAY_RESIZE_FACTOR 2

namespace DarrayFlag
{
	enum Value
	{
		NON_RESIZABLE = 1 << 0
	};
}

template <typename T>
struct SHMAPI Darray
{

	SHMINLINE Darray() : count(0), data(0), flags(0), allocation_tag((uint16)AllocationTag::UNKNOWN) {};
	SHMINLINE Darray(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::UNKNOWN);
	SHMINLINE ~Darray();

	SHMINLINE Darray(const Darray& other);
	SHMINLINE Darray& operator=(const Darray& other);
	SHMINLINE Darray(Darray&& other) noexcept;
	SHMINLINE Darray& operator=(Darray&& other);

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::UNKNOWN);
	SHMINLINE void free_data();

	SHMINLINE void clear();

	SHMINLINE void resize();
	SHMINLINE void resize(uint32 requested_size);

	SHMINLINE T* push(const T& obj);
	SHMINLINE T* push_steal(T& obj);
	SHMINLINE void pop();

	SHMINLINE T* insert_at(const T& obj, uint32 index);
	SHMINLINE void remove_at(uint32 index);

	SHMINLINE T* transfer_data();
	
	SHMINLINE T& operator[](uint32 index)
	{
		SHMASSERT_MSG(index + 1 <= count, "Index does not lie within bounds of Darray.");
		return data[index];
	}

	SHMINLINE const T& operator[](uint32 index) const
	{
		SHMASSERT_MSG(index + 1 <= count, "Index does not lie within bounds of Darray.");
		return data[index];
	}

	template<typename SubT>
	SHMINLINE SubT& get_as(uint32 index)
	{
		uint32 sub_t_max_count = (sizeof(T) * count) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_count, "Index does not lie within bounds of Darray.");
		return ((SubT*)data)[index];
	}

	template<typename SubT>
	SHMINLINE const SubT& get_as(uint32 index) const
	{
		uint32 sub_t_max_count = (sizeof(T) * count) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_count, "Index does not lie within bounds of Darray.");
		return ((SubT*)data)[index];
	}

	T* data = 0;
	uint32 size = 0; // Max Capacity of contained objects
	uint32 count = 0; // Count of contained objects

	uint16 flags;
	uint16 allocation_tag;
};

template<typename T>
SHMINLINE Darray<T>::Darray(uint32 reserve_count, uint32 creation_flags, AllocationTag tag)
{
	data = 0;
	init(reserve_count, creation_flags, tag);
}

template<typename T>
SHMINLINE Darray<T>::~Darray()
{
	free_data();
}

template<typename T>
SHMINLINE Darray<T>::Darray(const Darray& other)
{
	init(other.count, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.count; i++)
		push(other[i]);
}

template<typename T>
SHMINLINE Darray<T>& Darray<T>::operator=(const Darray& other)
{
	free_data();
	init(other.count, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.count; i++)
		push(other[i]);
	return *this;
}

template<typename T>
SHMINLINE Darray<T>::Darray(Darray&& other) noexcept
{
	free_data();
	data = other.data;
	size = other.size;
	count = other.count;
	flags = other.flags;
	allocation_tag = other.allocation_tag;

	other.data = 0;
	other.size = 0;
	other.count = 0;
}

template<typename T>
SHMINLINE Darray<T>& Darray<T>::operator=(Darray&& other)
{
	data = other.data;
	size = other.size;
	count = other.count;
	flags = other.flags;
	allocation_tag = other.allocation_tag;

	other.data = 0;
	other.size = 0;
	other.count = 0;
	return *this;
}

template<typename T>
SHMINLINE void Darray<T>::init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag)
{
	SHMASSERT_MSG(!data, "Cannot initialize Darray with existing data!");

	allocation_tag = (uint16)tag;
	size = reserve_count;
	count = 0;
	flags = (uint16)creation_flags;
	data = (T*)Memory::allocate(sizeof(T) * reserve_count, true, (AllocationTag)allocation_tag);
}

template<typename T>
SHMINLINE void Darray<T>::free_data()
{

	if (data)
	{
		for (uint32 i = 0; i < count; i++)
			data[i].~T();

		Memory::free_memory(data, true, (AllocationTag)allocation_tag);
	}

	size = 0;
	data = 0;
	count = 0;	

}

template<typename T>
SHMINLINE void Darray<T>::clear()
{

	for (uint32 i = 0; i < count; i++)
		data[i].~T();

	Memory::zero_memory(data, sizeof(T) * size);
	count = 0;

}

template<typename T>
inline SHMINLINE void Darray<T>::resize()
{
	resize(size * DARRAY_RESIZE_FACTOR);
}

template<typename T>
inline SHMINLINE void Darray<T>::resize(uint32 requested_size)
{
	SHMASSERT_MSG(!(flags & DarrayFlag::NON_RESIZABLE), "Darray push exceeded size, but array has been flagged as non-resizable!");
	uint32 old_size = size;
	while (size < requested_size)
		size *= DARRAY_RESIZE_FACTOR;
	uint64 allocation_size = size * sizeof(T);
	data = (T*)Memory::reallocate(allocation_size, data, true, (AllocationTag)allocation_tag);
	// TODO: Remove this and put zeroin out in reallocation function instead
	Memory::zero_memory((data + old_size), (size - old_size) * sizeof(T));
}

template<typename T>
inline SHMINLINE T* Darray<T>::push(const T& obj)
{

	if (count + 1 > size)
	{
		resize();
	}

	data[count] = obj;
	count++;
	return &data[count - 1];

}

template<typename T>
inline SHMINLINE T* Darray<T>::push_steal(T& obj)
{

	if (count + 1 > size)
	{
		resize();
	}

	Memory::copy_memory(&obj, &data[count], sizeof(T));
	Memory::zero_memory(&obj, sizeof(T));
	count++;
	return &data[count - 1];

}

template<typename T>
inline SHMINLINE void Darray<T>::pop()
{

	if (count <= 0)
		return;

	T* pop_ptr = data + (count - 1);
	(*pop_ptr).~T();
	Memory::zero_memory(pop_ptr, sizeof(T));
	count--;

}

template<typename T>
inline SHMINLINE T* Darray<T>::insert_at(const T& obj, uint32 index)
{

	SHMASSERT_MSG(index <= count, "ERROR: Index is out of darray's scope!");
	if (count + 1 > data->size)
	{
		resize();
	}

	uint32 copy_block_size = (count - index) * sizeof(T);
	T* insert_ptr = data + index;

	T* copy_source = insert_ptr;
	T* copy_dest = insert_ptr + 1;
	Memory::copy_memory(copy_source, copy_dest, copy_block_size);
	*copy_source = obj;
	count++;
	return copy_source;

}

template<typename T>
inline SHMINLINE void Darray<T>::remove_at(uint32 index)
{

	SHMASSERT_MSG(index < count, "ERROR: Index is out of darray's scope!");

	uint32 copy_block_size = (count - index - 1) * sizeof(T);
	T* remove_ptr = data + index;
	(*remove_ptr).~T();

	T* copy_source = remove_ptr + 1;
	T* copy_dest = remove_ptr;
	Memory::copy_memory(copy_source, copy_dest, copy_block_size);

	remove_ptr = data + (count - 1);
	Memory::zero_memory(remove_ptr, sizeof(T));
	count--;

}

template<typename T>
inline SHMINLINE T* Darray<T>::transfer_data()
{
	T* ret = data;

	data = 0;
	size = 0;
	count = 0;

	return ret;
}
