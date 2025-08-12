#pragma once

#include "Sarray.hpp"
#include "core/Logging.hpp"
#include "core/Assert.hpp"

typedef SarrayFlags::Value RingQueueFlags;

template<typename T>
struct RingQueue
{

	RingQueue() : arr({}), count(0), flags(0), head_index(0), tail_index(0) {};
	SHMINLINE RingQueue(uint32 reserve_count, RingQueueFlags creation_flags, AllocationTag tag = AllocationTag::RingQueue, void* memory = 0);
	SHMINLINE ~RingQueue();

	SHMINLINE RingQueue(const RingQueue& other);
	SHMINLINE RingQueue& operator=(const RingQueue& other);
	SHMINLINE RingQueue(RingQueue&& other) noexcept;
	SHMINLINE RingQueue& operator=(RingQueue&& other);

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, RingQueueFlags creation_flags, AllocationTag tag = AllocationTag::RingQueue, void* memory = 0);
	SHMINLINE void free_data();

	SHMINLINE uint64 get_external_size_requirement(uint32 count) { return count * sizeof(T); }

	SHMINLINE void clear();

	SHMINLINE void enqueue(const T& value);
	SHMINLINE T* dequeue();
	SHMINLINE T* peek();

	Sarray<T> arr;
	uint32 count;
	uint32 flags;

	uint32 head_index;
	uint32 tail_index;

};

template<typename T>
SHMINLINE RingQueue<T>::RingQueue(uint32 reserve_count, RingQueueFlags creation_flags, AllocationTag tag, void* memory)
{
	init(reserve_count, creation_flags, tag, memory);
}

template<typename T>
SHMINLINE RingQueue<T>::~RingQueue()
{
	free_data();
}

template<typename T>
SHMINLINE RingQueue<T>::RingQueue(const RingQueue& other)
{
	init(other.arr.capacity, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.count; i++)
		arr[i] = other.arr[i];

	count = other.count;
	head_index = other.head_index;
	tail_index = other.tail_index;
}

template<typename T>
SHMINLINE RingQueue<T>& RingQueue<T>::operator=(const RingQueue& other)
{
	free_data();
	init(other.arr.capacity, other.flags, (AllocationTag)other.allocation_tag);
	for (uint32 i = 0; i < other.count; i++)
		arr[i] = other.arr[i];

	count = other.count;
	head_index = other.head_index;
	tail_index = other.tail_index;
	return *this;
}


template<typename T>
SHMINLINE RingQueue<T>::RingQueue(RingQueue&& other) noexcept
{
	arr.steal(other.arr);
	count = other.count;
	flags = other.flags;
	head_index = other.head_index;
	tail_index = other.tail_index;

	other.count = 0;
	other.flags = 0;
	other.head_index = 0;
	other.tail_index = 0;
}

template<typename T>
SHMINLINE RingQueue<T>& RingQueue<T>::operator=(RingQueue&& other)
{
	free_data();
	arr.steal(other.arr);
	count = other.count;
	flags = other.flags;
	head_index = other.head_index;
	tail_index = other.tail_index;

	other.count = 0;
	other.flags = 0;
	other.head_index = 0;
	other.tail_index = 0;
	return *this;
}

template<typename T>
SHMINLINE void RingQueue<T>::init(uint32 reserve_count, RingQueueFlags creation_flags, AllocationTag tag, void* memory)
{
	arr.init(reserve_count, creation_flags, tag, memory);
	count = 0;
	flags = creation_flags;
	head_index = 0;
	tail_index = 0;
}

template<typename T>
SHMINLINE void RingQueue<T>::free_data()
{
	arr.free_data();

	count = 0;
	head_index = 0;
	tail_index = 0;
}

template<typename T>
SHMINLINE void RingQueue<T>::clear()
{
	count = 0;
	head_index = 0;
	tail_index = 0;
}

template<typename T>
SHMINLINE void RingQueue<T>::enqueue(const T& value)
{
	SHMASSERT_MSG(count < arr.capacity, "RingQueue is full.");
	arr[tail_index++] = value;
	if (tail_index >= arr.capacity)
		tail_index = 0;

	count++;
}

template<typename T>
SHMINLINE T* RingQueue<T>::dequeue()
{
	if (count == 0)
		return 0;

	T* ret = &arr[head_index++];
	if (head_index >= arr.capacity)
		head_index = 0;

	count--;
	return ret;
}

template<typename T>
SHMINLINE T* RingQueue<T>::peek()
{
	if (count == 0)
		return 0;

	T* ret = &arr[head_index];
	return ret;
}