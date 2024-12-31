#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "core/Assert.hpp"
#include <utility>

#define DARRAY_DEFAULT_SIZE 1
#define DARRAY_RESIZE_FACTOR 2

namespace DarrayFlags
{
	enum Value
	{
		NON_RESIZABLE = 1 << 0,
		IS_STRING = 1 << 1,
		EXTERNAL_MEMORY = 1 << 2
	};
}

template <typename T>
struct Darray
{

	SHMINLINE Darray() : count(0), data(0), flags(0), allocation_tag((uint16)AllocationTag::DARRAY) {};
	SHMINLINE Darray(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::DARRAY, void* memory = 0);
	SHMINLINE ~Darray();

	SHMINLINE Darray(const Darray& other);
	SHMINLINE Darray& operator=(const Darray& other);
	SHMINLINE Darray(Darray&& other) noexcept;
	SHMINLINE Darray& operator=(Darray&& other);

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::DARRAY, void* memory = 0);
	SHMINLINE void free_data();

	SHMINLINE void steal(Darray<T>& other)
	{
		data = other.data;
		capacity = other.capacity;
		count = other.count;
		flags = other.flags;
		allocation_tag = other.allocation_tag;

		other.data = 0;
		other.capacity = 0;
		other.count = 0;
	}

	SHMINLINE void clear();

	SHMINLINE void resize();
	SHMINLINE void resize(uint32 requested_size);

	SHMINLINE void set_count(uint32 new_count)
	{
		SHMASSERT_MSG(new_count <= capacity, "New count cannot exceed current capacity of Darray.");
		count = new_count;
	}

	SHMINLINE uint32 push(const T& obj);
	SHMINLINE uint32 push(T&& obj);
	SHMINLINE uint32 push_steal(T& obj);

	template <typename... Args>
	SHMINLINE uint32 emplace(Args&&... args)
	{
		if (count + 1 > capacity)
			resize();

		T* ret = new(&data[count]) T(std::forward<Args>(args)...);
		count++;
		return count-1;
	}

	SHMINLINE void pop();

	SHMINLINE T* insert_at(const T& obj, uint32 index);
	SHMINLINE void remove_at(uint32 index);

	SHMINLINE T* transfer_data();

	SHMINLINE void copy_memory(const void* source, uint64 size, uint64 offset, uint32 imported_count);
	
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
		uint32 sub_t_max_capacity = (sizeof(T) * capacity) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_capacity, "Index does not lie within bounds of Darray.");
		return ((SubT*)data)[index];
	}

	template<typename SubT>
	SHMINLINE const SubT& get_as(uint32 index) const
	{
		uint32 sub_t_max_capacity = (sizeof(T) * capacity) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_capacity, "Index does not lie within bounds of Darray.");
		return ((SubT*)data)[index];
	}

	T* data = 0;
	uint32 capacity = 0; // Max Capacity of contained objects
	uint32 count = 0; // Count of contained objects

	uint16 flags;
	uint16 allocation_tag;
};

template<typename T>
SHMINLINE Darray<T>::Darray(uint32 reserve_count, uint32 creation_flags, AllocationTag tag, void* memory)
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
		emplace(other[i]);
}

template<typename T>
SHMINLINE Darray<T>& Darray<T>::operator=(const Darray& other)
{
	free_data();
	init(other.count, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.count; i++)
		emplace(other[i]);
	return *this;
}

template<typename T>
SHMINLINE Darray<T>::Darray(Darray&& other) noexcept
{
	free_data();
	data = other.data;
	capacity = other.capacity;
	count = other.count;
	flags = other.flags;
	allocation_tag = other.allocation_tag;

	other.data = 0;
	other.capacity = 0;
	other.count = 0;
}

template<typename T>
SHMINLINE Darray<T>& Darray<T>::operator=(Darray&& other)
{
	data = other.data;
	capacity = other.capacity;
	count = other.count;
	flags = other.flags;
	allocation_tag = other.allocation_tag;

	other.data = 0;
	other.capacity = 0;
	other.count = 0;
	return *this;
}

template<typename T>
SHMINLINE void Darray<T>::init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag, void* memory)
{
	SHMASSERT_MSG(!data, "Cannot initialize Darray with existing data!");

	if (!reserve_count)
		return;

	allocation_tag = (uint16)tag;
	capacity = reserve_count;
	count = 0;
	flags = (uint16)creation_flags;
	if (memory)
		flags |= DarrayFlags::EXTERNAL_MEMORY;
	if (creation_flags & DarrayFlags::EXTERNAL_MEMORY)
		flags |= DarrayFlags::NON_RESIZABLE;

	if (memory)
		data = (T*)memory;
	else if (flags & DarrayFlags::IS_STRING)
		data = (T*)Memory::allocate_string(sizeof(T) * reserve_count, (AllocationTag)allocation_tag);
	else
		data = (T*)Memory::allocate(sizeof(T) * reserve_count, (AllocationTag)allocation_tag);
}

template<typename T>
SHMINLINE void Darray<T>::free_data()
{

	if (data && !(flags & DarrayFlags::EXTERNAL_MEMORY))
	{
		for (uint32 i = 0; i < count; i++)
			data[i].~T();

		if (flags & DarrayFlags::IS_STRING)
			Memory::free_memory_string(data);
		else
			Memory::free_memory(data);
	}

	capacity = 0;
	data = 0;
	count = 0;	

}

template<typename T>
SHMINLINE void Darray<T>::clear()
{

	for (uint32 i = 0; i < count; i++)
		data[i].~T();

	//Memory::zero_memory(data, sizeof(T) * capacity);
	count = 0;

}

template<typename T>
inline SHMINLINE void Darray<T>::resize()
{
	resize(capacity * DARRAY_RESIZE_FACTOR);
}

template<typename T>
inline SHMINLINE void Darray<T>::resize(uint32 requested_size)
{
	SHMASSERT_MSG(!(flags & DarrayFlags::NON_RESIZABLE) && !(flags & DarrayFlags::EXTERNAL_MEMORY), "Darray push exceeded size, but array has been flagged as non-resizable!");
	uint32 old_size = capacity;
	while (capacity < requested_size)
		capacity *= DARRAY_RESIZE_FACTOR;
	uint64 allocation_size = capacity * sizeof(T);

	if (flags & DarrayFlags::IS_STRING)
		data = (T*)Memory::reallocate_string(allocation_size, data);
	else
		data = (T*)Memory::reallocate(allocation_size, data);

	Memory::zero_memory((data + old_size), (capacity - old_size) * sizeof(T));
}

template<typename T>
inline SHMINLINE uint32 Darray<T>::push(const T& obj)
{

	if (!capacity)
		init(1, 0);

	if (count + 1 > capacity)
		resize();

	data[count] = obj;
	count++;
	return count - 1;

}

template<typename T>
inline SHMINLINE uint32 Darray<T>::push(T&& obj)
{

	if (!capacity)
		init(1, 0);

	if (count + 1 > capacity)
		resize();

	data[count] = std::move(obj);
	count++;
	return count - 1;

}

template<typename T>
inline SHMINLINE uint32 Darray<T>::push_steal(T& obj)
{

	if (!capacity)
		init(1, 0);

	if (count + 1 > capacity)
		resize();

	Memory::copy_memory(&obj, &data[count], sizeof(T));
	Memory::zero_memory(&obj, sizeof(T));
	count++;
	return count - 1;

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
	capacity = 0;
	count = 0;

	return ret;
}

template<typename T>
SHMINLINE void Darray<T>::copy_memory(const void* source, uint64 size, uint64 offset, uint32 imported_count)
{
	SHMASSERT_MSG((size + offset) <= (sizeof(T) * capacity) && imported_count <= capacity, "Darray does not fit requested size and/or imported count does not fit!");
	uint8* dest = ((uint8*)data) + offset;
	Memory::copy_memory(source, dest, size);
	count = imported_count;
}
