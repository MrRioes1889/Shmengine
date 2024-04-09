#include "Darray.hpp"
#include "core/Memory.hpp"

namespace Memory
{
	template<typename T>
	Darray<T>* darray_create(uint32 prealloc_count)
	{
		uint32 stride = sizeof(T);
		uint64 total_size = sizeof(Darray<T>) + (stride * prealloc_count);

		Darray<T>* arr = (Darray<T>*)allocate(total_size, true);
		arr->stride = stride;
		arr->size = prealloc_count;
		arr->count = 0;
		arr->data = arr + 1;
	}

	template<typename T>
	void darray_destroy(Darray<T>* array)
	{
		free_memory(array, true);
	}

	template<typename T>
	void Darray<T>::resize()
	{
		uint64 total_size = sizeof(Darray<T>) + (stride * size);
	}

	template<typename T>
	T* Darray<T>::push(T& obj)
	{
		return nullptr;
	}

	template<typename T>
	void Darray<T>::pop()
	{
	}

	template<typename T>
	T* Darray<T>::insert_at(T& obj, uint32 index)
	{
		return nullptr;
	}

	template<typename T>
	void Darray<T>::remove_at(uint32 index)
	{
	}
}