#include "Darray.hpp"
#include "core/Memory.hpp"
#include "core/Assert.hpp"

static Darray* get_darray_data(void* array)
{
	return (((Darray*)array) - 1);
}

void* _darray_create(uint32 stride, uint32 prealloc_count)
{
	uint64 total_size = sizeof(Darray) + (stride * prealloc_count);

	Darray* arr = (Darray*)Memory::allocate(total_size, true);
	arr->stride = stride;
	arr->size = prealloc_count;
	arr->count = 0;
	return arr + 1;
}

void darray_destroy(void* array)
{
	Memory::free_memory((((Darray*)array) - 1), true);
}

static void darray_resize(void*& array)
{
	Darray* data = get_darray_data(array);
	uint64 requested_size = sizeof(Darray) + (data->stride * data->size * DARRAY_RESIZE_FACTOR);
	data = (Darray*)Memory::reallocate(requested_size, data, true);
	data->size = data->size * DARRAY_RESIZE_FACTOR;
	array = data + 1;
}

void* _darray_push(void* array, const void* obj)
{
	Darray* data = get_darray_data(array);
	if (data->count + 1 > data->size)
	{
		darray_resize(array);
		data = get_darray_data(array);
	}
		
	uint8* byte_ptr = (uint8*)array;
	byte_ptr += data->stride * data->count;
	Memory::copy_memory(obj, (void*)byte_ptr, data->stride);
	data->count++;
	return array;
}

void darray_pop(void* array)
{
	Darray* data = get_darray_data(array);
	if (data->count <= 0)
		return;

	uint8* byte_ptr = (uint8*)array;
	byte_ptr += data->stride * (data->count - 1);
	Memory::zero_memory((void*)byte_ptr, data->stride);
	data->count--;
}

void* _darray_insert_at(void* array, const void* obj, uint32 index)
{
	Darray* data = get_darray_data(array);
	SHMASSERT_MSG(index <= data->count, "ERROR: Index is out of darray's scope!");
	if (data->count + 1 > data->size)
	{
		darray_resize(array);
		data = get_darray_data(array);
	}

	uint32 copy_block_size = (data->count - index) * data->stride;
	uint8* byte_ptr = (uint8*)array;
	byte_ptr += data->stride * index;

	void* copy_source = byte_ptr;
	void* copy_dest = byte_ptr + data->stride;
	Memory::copy_memory(copy_source, copy_dest, copy_block_size);
	Memory::copy_memory(obj, copy_source, data->stride);
	data->count++;

	return array;
}

void darray_remove_at(void* array, uint32 index)
{
	Darray* data = get_darray_data(array);
	SHMASSERT_MSG(index < data->count, "ERROR: Index is out of darray's scope!");

	uint32 copy_block_size = (data->count - index - 1) * data->stride;
	uint8* byte_ptr = (uint8*)array;
	byte_ptr += data->stride * index;

	void* copy_source = byte_ptr + data->stride;
	void* copy_dest = byte_ptr;
	Memory::copy_memory(copy_source, copy_dest, copy_block_size);

	byte_ptr = (uint8*)array;
	byte_ptr += data->stride * (data->count - 1);
	Memory::zero_memory((void*)byte_ptr, data->stride);
	data->count--;
}

void darray_clear(void* array)
{
	Darray* data = get_darray_data(array);
	uint64 data_size = data->stride * data->count;
	Memory::zero_memory(array, data_size);
	data->count = 0;
}

uint32 darray_size(void* array)
{
	Darray* data = get_darray_data(array);
	return data->size;
}

uint32 darray_count(void* array)
{
	Darray* data = get_darray_data(array);
	return data->count;
}
