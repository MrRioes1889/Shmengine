#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Assert.hpp"
#include "Darray.hpp"

namespace SarrayFlags
{
	enum : uint8
	{
		ExternalMemory = 1 << 0
	};
	typedef uint8 Value;
}

template<typename T>
struct Sarray
{

	Sarray() : capacity(0), data(0), allocation_tag((uint16)AllocationTag::Array), flags(0) {};
	SHMINLINE Sarray(uint32 reserve_count, SarrayFlags::Value creation_flags, AllocationTag tag = AllocationTag::Array, void* memory = 0);
	SHMINLINE ~Sarray();

	SHMINLINE Sarray(const Sarray& other);
	SHMINLINE Sarray& operator=(const Sarray& other);
	SHMINLINE Sarray(Sarray&& other) noexcept;
	SHMINLINE Sarray& operator=(Sarray&& other);

	// NOTE: Call for already instantiated arrays
	SHMINLINE void init(uint32 reserve_count, SarrayFlags::Value creation_flags, AllocationTag tag = AllocationTag::Array, void* memory = 0);
	SHMINLINE void free_data();

	SHMINLINE uint64 get_external_size_requirement(uint32 count) { return count * sizeof(T); }

	SHMINLINE void resize(uint32 new_count, void* memory = 0);
	SHMINLINE void clear();

	SHMINLINE T* transfer_data();

	SHMINLINE void copy_memory(const void* source, uint32 copy_count, uint32 array_offset);

	SHMINLINE void zero_memory()
	{
		Memory::zero_memory(data, this->size());
	}

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

	SHMINLINE uint64 size() const
	{
		return capacity * sizeof(T);
	}
	
	T* data = 0;
	uint32 capacity = 0;
	uint16 allocation_tag;
	SarrayFlags::Value flags;

};

template<typename T>
SHMINLINE Sarray<T>::Sarray(uint32 reserve_count, SarrayFlags::Value creation_flags, AllocationTag tag, void* memory)
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
SHMINLINE void Sarray<T>::init(uint32 reserve_count, SarrayFlags::Value creation_flags, AllocationTag tag, void* memory)
{
	SHMASSERT_MSG(!data || ((flags & SarrayFlags::ExternalMemory) != 0), "Cannot init non empty sarray!");

	if (!reserve_count)
		return;

	allocation_tag = (uint16)tag;
	capacity = reserve_count;
	flags = (uint16)creation_flags;

	if (memory)
	{
		flags |= SarrayFlags::ExternalMemory;
		data = (T*)memory;
	}
	else
	{
		flags &= ~SarrayFlags::ExternalMemory;
		data = (T*)Memory::allocate(sizeof(T) * reserve_count, (AllocationTag)allocation_tag);
	}
}

template<typename T>
SHMINLINE void Sarray<T>::free_data()
{
	if (data)
	{
		if constexpr (std::is_destructible_v<T> && !std::is_trivially_destructible_v<T>)
		{
			for (uint32 i = 0; i < capacity; i++)
				data[i].~T();
		}

		if (!(flags & SarrayFlags::ExternalMemory))
			Memory::free_memory(data);
	}
		
	data = 0;
	capacity = 0;
}


template<typename T>
SHMINLINE void Sarray<T>::resize(uint32 new_count, void* memory)
{
	SHMASSERT_MSG(data, "Cannot resize not initialized Sarray.");

	if (data && !(flags & SarrayFlags::ExternalMemory))
		data = (T*)Memory::reallocate(new_count * sizeof(T), data);
	else
		data = (T*)memory;

	if (new_count > capacity)
		Memory::zero_memory((data + capacity), (new_count - capacity) * sizeof(T));

	capacity = new_count;
}

template<typename T>
SHMINLINE void Sarray<T>::clear()
{
	if constexpr (std::is_destructible_v<T> && !std::is_trivially_destructible_v<T>)
	{
		for (uint32 i = 0; i < capacity; i++)
			data[i].~T();
	}

	Memory::zero_memory(data, sizeof(T) * capacity);
}

template<typename T>
SHMINLINE T* Sarray<T>::transfer_data()
{
	T* ret = data;

	data = 0;
	capacity = 0;

	return ret;
}

template<typename T>
SHMINLINE void Sarray<T>::copy_memory(const void* source, uint32 copy_count, uint32 dest_offset)
{
	SHMASSERT_MSG((copy_count + dest_offset) <= capacity, "Sarray does not fit requested size and/or imported count does not fit!");
	T* dest = data + dest_offset;
	Memory::copy_memory(source, dest, copy_count * sizeof(T));
}

template <typename DstType>
struct SarrayRef
{
	SarrayRef() = delete;
	template <typename SrcType>
	SarrayRef(const Sarray<SrcType>* arr)
	{
		capacity = (uint32)(arr->size() / sizeof(DstType));
		data = (DstType*)arr->data;
	}

	SarrayRef(void* buffer, uint64 buffer_size)
	{
		capacity = (uint32)(buffer_size / sizeof(DstType));
		data = (DstType*)buffer;
	}

	SHMINLINE DstType& operator[](uint32 index)
	{
		SHMASSERT_MSG(index + 1 <= capacity, "Index does not lie within bounds of Sarray.");
		return data[index];
	}

	SHMINLINE const DstType& operator[](uint32 index) const
	{
		SHMASSERT_MSG(index + 1 <= capacity, "Index does not lie within bounds of Sarray.");
		return data[index];
	}

	DstType* data;
	uint32 capacity;
};

