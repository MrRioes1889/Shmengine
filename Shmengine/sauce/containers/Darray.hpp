#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "core/Assert.hpp"

#define DARRAY_DEFAULT_SIZE 1
#define DARRAY_RESIZE_FACTOR 2

template <typename T>
struct Darray
{

	Darray(const Darray& other) = delete;
	Darray(Darray&& other) = delete;

	SHMINLINE Darray() : count(0), data(0), allocation_tag(AllocationTag::UNKNOWN) {};
	SHMINLINE Darray(uint32 reserve_count, AllocationTag tag = AllocationTag::UNKNOWN);
	SHMINLINE ~Darray();

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, AllocationTag tag = AllocationTag::UNKNOWN);
	SHMINLINE void free_data();

	SHMINLINE void clear();

	SHMINLINE void resize();

	SHMINLINE T* push(const T& obj);
	SHMINLINE void pop();

	SHMINLINE T* insert_at(const T& obj, uint32 index);
	SHMINLINE void remove_at(uint32 index);

	T& operator[](const uint32& index)
	{
		SHMASSERT_MSG(index + 1 <= count, "Index does not lie within bounds of Darray.");
		return data[index];
	}

	T* data;
	uint32 size; // Max Capacity of contained objects
	uint32 count; // Count of contained objects

	AllocationTag allocation_tag;
};

template<typename T>
SHMINLINE Darray<T>::Darray(uint32 reserve_count, AllocationTag tag)
{
	data = 0;
	init(reserve_count, tag);
}

template<typename T>
SHMINLINE Darray<T>::~Darray()
{
	if (data)
		Memory::free_memory(data, true, allocation_tag);
}

template<typename T>
SHMINLINE void Darray<T>::init(uint32 reserve_count, AllocationTag tag)
{
	//SHMASSERT_MSG(!data, "Cannot initialize Darray with existing data!");
	if (data)
		free_data();

	allocation_tag = tag;
	size = reserve_count;
	count = 0;
	data = (T*)Memory::allocate(sizeof(T) * reserve_count, true, allocation_tag);
	clear();
}

template<typename T>
SHMINLINE void Darray<T>::free_data()
{
	if (data)
		Memory::free_memory(data, true, allocation_tag);

	data = 0;
	count = 0;
}

template<typename T>
SHMINLINE void Darray<T>::clear()
{
	Memory::zero_memory(data, sizeof(T) * size);
	count = 0;
}

template<typename T>
inline SHMINLINE void Darray<T>::resize()
{
	uint64 requested_size = size * sizeof(T) * DARRAY_RESIZE_FACTOR;
	data = (T*)Memory::reallocate(requested_size, data, true, allocation_tag);
	size = size * DARRAY_RESIZE_FACTOR;
}

template<typename T>
inline SHMINLINE T* Darray<T>::push(const T& obj)
{

	if (count + 1 > size)
	{
		resize();
	}

	T* push_ptr = data + count;
	Memory::copy_memory(&obj, push_ptr, sizeof(T));
	count++;
	return push_ptr;

}

template<typename T>
inline SHMINLINE void Darray<T>::pop()
{

	if (count <= 0)
		return;

	T* pop_ptr = data + (count - 1);
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
	Memory::copy_memory(&obj, copy_source, sizeof(T));
	count++;

	return insert_ptr;

}

template<typename T>
inline SHMINLINE void Darray<T>::remove_at(uint32 index)
{

	SHMASSERT_MSG(index < count, "ERROR: Index is out of darray's scope!");

	uint32 copy_block_size = (count - index - 1) * sizeof(T);
	T* remove_ptr = data + index;

	T* copy_source = remove_ptr + 1;
	T* copy_dest = remove_ptr;
	Memory::copy_memory(copy_source, copy_dest, copy_block_size);

	remove_ptr = data + (count - 1);
	Memory::zero_memory(remove_ptr, sizeof(T));
	count--;

}

