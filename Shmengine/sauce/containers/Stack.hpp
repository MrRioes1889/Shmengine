#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "core/Assert.hpp"

#define STACK_DEFAULT_SIZE 1
#define STACK_RESIZE_FACTOR 2

namespace StackFlags
{
	enum Value
	{
		NonResizable = 1 << 0,
		ExternalMemory = 1 << 1
	};
}

template <typename T>
struct Stack
{

	SHMINLINE Stack() : count(0), data(0), flags(0), allocation_tag((uint16)AllocationTag::DArray) {};
	SHMINLINE Stack(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::DArray, void* memory = 0);
	SHMINLINE ~Stack();

	SHMINLINE Stack(const Stack& other);
	SHMINLINE Stack& operator=(const Stack& other);
	SHMINLINE Stack(Stack&& other) noexcept;
	SHMINLINE Stack& operator=(Stack&& other);

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag = AllocationTag::DArray, void* memory = 0);
	SHMINLINE void free_data();

	SHMINLINE uint64 get_external_size_requirement(uint32 count) { return count * sizeof(T); }

	SHMINLINE void clear();

	SHMINLINE void resize();
	SHMINLINE void resize(uint32 requested_size);

	SHMINLINE T* push(const T& obj);
	SHMINLINE void pop();

	SHMINLINE T* transfer_data();

	SHMINLINE void copy_memory(const void* source, uint64 size, uint64 offset, uint32 imported_count);

	SHMINLINE T& operator[](uint32 index)
	{
		SHMASSERT_MSG(index + 1 <= count, "Index does not lie within bounds of Stack.");
		return data[index];
	}

	SHMINLINE const T& operator[](uint32 index) const
	{
		SHMASSERT_MSG(index + 1 <= count, "Index does not lie within bounds of Stack.");
		return data[index];
	}

	template<typename SubT>
	SHMINLINE SubT& get_as(uint32 index)
	{
		uint32 sub_t_max_count = (sizeof(T) * count) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_count, "Index does not lie within bounds of Stack.");
		return ((SubT*)data)[index];
	}

	template<typename SubT>
	SHMINLINE const SubT& get_as(uint32 index) const
	{
		uint32 sub_t_max_count = (sizeof(T) * count) / sizeof(SubT);
		SHMASSERT_MSG(index + 1 <= sub_t_max_count, "Index does not lie within bounds of Stack.");
		return ((SubT*)data)[index];
	}

	T* data = 0;
	uint32 capacity = 0; // Max Capacity of contained objects
	uint32 count = 0; // Count of contained objects

	uint16 flags;
	uint16 allocation_tag;
};

template<typename T>
SHMINLINE Stack<T>::Stack(uint32 reserve_count, uint32 creation_flags, AllocationTag tag, void* memory)
{
	data = 0;
	init(reserve_count, creation_flags, tag);
}

template<typename T>
SHMINLINE Stack<T>::~Stack()
{
	free_data();
}

template<typename T>
SHMINLINE Stack<T>::Stack(const Stack& other)
{
	init(other.count, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.count; i++)
		push(other[i]);
}

template<typename T>
SHMINLINE Stack<T>& Stack<T>::operator=(const Stack& other)
{
	free_data();
	init(other.count, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.count; i++)
		push(other[i]);
	return *this;
}

template<typename T>
SHMINLINE Stack<T>::Stack(Stack&& other) noexcept
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
SHMINLINE Stack<T>& Stack<T>::operator=(Stack&& other)
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
SHMINLINE void Stack<T>::init(uint32 reserve_count, uint32 creation_flags, AllocationTag tag, void* memory)
{
	SHMASSERT_MSG(!data || ((flags & StackFlags::ExternalMemory) != 0), "Cannot initialize Stack with existing data!");

	if (!reserve_count)
		return;

	allocation_tag = (uint16)tag;
	capacity = reserve_count;
	count = 0;
	flags = (uint16)creation_flags;

	if (memory)
	{
		flags |= StackFlags::ExternalMemory | StackFlags::NonResizable;
		data = (T*)memory;
	}
	else
	{
		flags &= ~StackFlags::ExternalMemory;
		data = (T*)Memory::allocate(sizeof(T) * reserve_count, (AllocationTag)allocation_tag);
	}
}

template<typename T>
SHMINLINE void Stack<T>::free_data()
{
	if (data)
	{
		if constexpr (std::is_destructible_v<T> && !std::is_trivially_destructible_v<T>)
		{
			for (uint32 i = 0; i < count; i++)
				data[i].~T();
		}

		if (!(flags & StackFlags::ExternalMemory))
			Memory::free_memory(data);
	}


	capacity = 0;
	data = 0;
	count = 0;
}

template<typename T>
SHMINLINE void Stack<T>::clear()
{
	if constexpr (std::is_destructible_v<T> && !std::is_trivially_destructible_v<T>)
	{
		for (uint32 i = 0; i < count; i++)
			data[i].~T();
	}

	//Memory::zero_memory(data, sizeof(T) * capacity);
	count = 0;
}

template<typename T>
SHMINLINE void Stack<T>::resize()
{
	resize(capacity * STACK_RESIZE_FACTOR);
}

template<typename T>
SHMINLINE void Stack<T>::resize(uint32 requested_size)
{
	SHMASSERT_MSG(!(flags & StackFlags::NonResizable) && !(flags & StackFlags::ExternalMemory), "Stack push exceeded size, but array has been flagged as non-resizable!");
	uint32 old_size = capacity;
	while (capacity < requested_size)
		capacity *= STACK_RESIZE_FACTOR;
	uint64 allocation_size = capacity * sizeof(T);

	data = (T*)Memory::reallocate(allocation_size, data);

	Memory::zero_memory((data + old_size), (capacity - old_size) * sizeof(T));
}

template<typename T>
SHMINLINE T* Stack<T>::push(const T& obj)
{

	if (count + 1 > capacity)
	{
		resize();
	}

	data[count] = obj;
	count++;
	return &data[count - 1];

}

template<typename T>
SHMINLINE void Stack<T>::pop()
{

	if (count <= 0)
		return;

	T* pop_ptr = data + (count - 1);
	if constexpr (std::is_destructible_v<T> && !std::is_trivially_destructible_v<T>)
		(*pop_ptr).~T();
	count--;

}

template<typename T>
SHMINLINE T* Stack<T>::transfer_data()
{
	T* ret = data;

	data = 0;
	capacity = 0;
	count = 0;

	return ret;
}

template<typename T>
SHMINLINE void Stack<T>::copy_memory(const void* source, uint64 size, uint64 offset, uint32 imported_count)
{
	SHMASSERT_MSG((size + offset) <= (sizeof(T) * capacity) && imported_count <= capacity, "Stack does not fit requested size and/or imported count does not fit!");
	uint8* dest = ((uint8*)data) + offset;
	Memory::copy_memory(source, dest, size);
	count = imported_count;
}
