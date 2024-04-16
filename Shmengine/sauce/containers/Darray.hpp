#pragma once

#include "Defines.hpp"

#define DARRAY_DEFAULT_SIZE 1
#define DARRAY_RESIZE_FACTOR 2

struct SHMAPI Darray
{
	uint32 size; // Max Capacity of contained objects
	uint32 count; // Count of contained objects
	uint32 stride; // Size of single object
};

#define darray_create(type) \
	(type*)_darray_create(sizeof(type), DARRAY_DEFAULT_SIZE)

#define darray_create_and_reserve(type, prealloc_count) \
	(type*)_darray_create(sizeof(type), prealloc_count)

#define darray_push(array, obj) \
{ \
	TYPEOF(obj) value = obj; \
	array = (TYPEOF(obj)*)_darray_push(array, &value); \
}

#define darray_insert_at(array, obj, index) \
{ \
	TYPEOF(obj) value = obj; \
	array = (TYPEOF(obj)*)_darray_insert_at(array, &value, index); \
}

SHMAPI void* _darray_create(uint32 stride, uint32 prealloc_count);
SHMAPI void darray_destroy(void* array);

//void darray_resize(void* array);

SHMAPI void* _darray_push(void* array, const void* obj);
SHMAPI void darray_pop(void* array);

SHMAPI void* _darray_insert_at(void* array, const void* obj, uint32 index);
SHMAPI void darray_remove_at(void* array, uint32 index);

SHMAPI void darray_clear(void* array);

SHMAPI uint32 darray_size(void* array);
SHMAPI uint32 darray_count(void* array);
