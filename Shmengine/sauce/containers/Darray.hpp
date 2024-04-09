#pragma once

#include "Defines.hpp"

#define DARRAY_RESIZE_FACTOR 2

namespace Memory
{

	template<typename T>
	struct Darray
	{
		Darray() = delete;
		~Darray() = delete;

		void resize();

		T* push(T& obj);
		void pop();

		T* insert_at(T& obj, uint32 index);
		void remove_at(uint32 index);

		uint32 size; // Max Capacity of contained objects
		uint32 count; // Count of contained objects
		uint32 stride; // Size of single object
		T* data;
	};

	template<typename T>
	SHMAPI Darray<T>* darray_create(uint32 prealloc_count);
	template<typename T>
	SHMAPI void darray_destroy(Darray<T>* array);


}