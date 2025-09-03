#pragma once
#include "Hashtable.hpp"
#include "core/Mutex.hpp"
#include "core/Identifier.hpp"

namespace StorageFlags
{
	enum : uint8
	{
		ExternalMemory = 1 << 0,
	};
	typedef uint8 Value;
}

template <typename ObjectT, typename IdentifierT>
struct LinearStorage
{
	
	SHMINLINE uint64 get_external_size_requirement(uint32 count) { return objects.get_external_size_requirement(count) + occupied_flags.get_external_size_requirement(count); }

	SHMINLINE LinearStorage(uint32 count, AllocationTag tag = AllocationTag::Dict, void* memory = 0)
	{
		init(count, tag, memory);
	}

	SHMINLINE ~LinearStorage()
	{
		destroy();
	}

	void init(uint32 count, AllocationTag tag = AllocationTag::Dict, void* memory = 0)
	{
		SHMASSERT_MSG(count && (uint64)count < (uint64)IdentifierT::invalid_value, "Element count cannot be null and cannot exceed identifier max valid value!");

		flags = 0;

		if (memory)
		{
			flags |= StorageFlags::ExternalMemory;
		}
		else
		{
			flags &= ~StorageFlags::ExternalMemory;
			memory = Memory::allocate(get_external_size_requirement(count), tag);
		}

		objects.init(count, 0, tag, memory);
		void* flags_data = PTR_BYTES_OFFSET(memory, objects.size());
		occupied_flags.init(count, 0, tag, flags_data);

		object_count = 0;
		first_empty_index = 0;
	}

	void destroy()
	{
		if (objects.data && !(flags & StorageFlags::ExternalMemory))
			Memory::free_memory(objects.data);

		objects.free_data();
		occupied_flags.free_data();
	}

	void resize(uint32 new_count)
	{
		if (new_count <= objects.capacity || (flags & StorageFlags::ExternalMemory))
			return;

		void* data = Memory::reallocate(get_external_size_requirement(new_count), objects.data);
		objects.resize(new_count, data);
		void* new_flags_data = PTR_BYTES_OFFSET(data, objects.size());
		Memory::copy_memory(occupied_flags.data, new_flags_data, occupied_flags.size());
		Memory::zero_memory(occupied_flags.data, occupied_flags.size());
		occupied_flags.init(new_count, new_flags_data);
	}

	void acquire(IdentifierT* out_id, ObjectT** out_creation_ptr)
	{
		out_id->invalidate();
		*out_creation_ptr = 0;

		if (object_count >= objects.capacity)
			return;

		IdentifierT id = first_empty_index;
		for (IdentifierT i = first_empty_index + 1; i < occupied_flags.capacity; i++)
		{
			if (!occupied_flags[i])
			{
				first_empty_index = i;
				break;
			}
		}

		occupied_flags[id] = true;
		*out_id = id;
		*out_creation_ptr = &objects[id];
		object_count++;
	}

	void release(IdentifierT id, ObjectT** out_destruction_ptr)
	{
		*out_destruction_ptr = 0;
		if (!id.is_valid() || !occupied_flags[id])
			return;

		*out_destruction_ptr = &objects[id];
		occupied_flags[id] = false;
		object_count--;
	}

	SHMINLINE ObjectT* get_object(IdentifierT id)
	{
		if (!id.is_valid() || !occupied_flags[id])
			return 0;

		return &objects[id];
	}

	StorageFlags::Value flags;
	IdentifierT first_empty_index;
	uint32 object_count;
	Sarray<ObjectT> objects;
	Sarray<bool8> occupied_flags;
	
	struct Iterator
	{
		Iterator(LinearStorage* target) : storage(target), cursor(0), counter(0), available_count(target->object_count) { }

		SHMINLINE IdentifierT get_next() 
		{
			if (counter >= available_count)
				return IdentifierT::invalid_value;

			while (!storage->occupied_flags[cursor])
				cursor++;

			IdentifierT id = cursor;
			cursor++;
			counter++;
			return id;
		}

		LinearStorage* storage;
		IdentifierT cursor;
		uint32 available_count;
		uint32 counter;
	};

	Iterator get_iterator()
	{
		return Iterator(this);
	}
};

template <typename ObjectT, typename IdentifierT, uint32 lookup_key_buffer_size>
struct LinearHashedStorage
{
	
	SHMINLINE uint64 get_external_size_requirement(uint32 count) { return objects.get_external_size_requirement(count) + occupied_flags.get_external_size_requirement(count) + lookup_table.get_external_size_requirement(count); }

	SHMINLINE LinearHashedStorage(uint32 count, AllocationTag tag = AllocationTag::Dict, void* memory = 0)
	{
		init(count, tag, memory);
	}

	SHMINLINE ~LinearHashedStorage()
	{
		destroy();
	}

	void init(uint32 count, AllocationTag tag = AllocationTag::Dict, void* memory = 0)
	{
		SHMASSERT_MSG(count && (uint64)count < (uint64)IdentifierT::invalid_value, "Element count cannot be null and cannot exceed identifier max valid value!");

		flags = 0;

		if (memory)
		{
			flags |= StorageFlags::ExternalMemory;
		}
		else
		{
			flags &= ~StorageFlags::ExternalMemory;
			memory = Memory::allocate(get_external_size_requirement(count), tag);
		}

		objects.init(count, 0, tag, memory);
		void* slot_states_data = PTR_BYTES_OFFSET(memory, objects.size());
		occupied_flags.init(count, 0, tag, slot_states_data);
		void* hashtable_data = PTR_BYTES_OFFSET(slot_states_data, occupied_flags.size());
		lookup_table.init(count, 0, tag, hashtable_data);

		object_count = 0;
		first_empty_index = 0;
	}

	void destroy()
	{
		if (objects.data && !(flags & StorageFlags::ExternalMemory))
			Memory::free_memory(objects.data);

		objects.free_data();
		occupied_flags.free_data();
		lookup_table.destroy();
	}

	void acquire(const char* key, IdentifierT* out_id, ObjectT** out_creation_ptr)
	{
		out_id->invalidate();
		*out_creation_ptr = 0;

		if (object_count >= objects.capacity)
			return;

		if (IdentifierT* existing_id = lookup_table.get(key))
		{
			*out_id = *existing_id;
			return;
		}

		IdentifierT new_id = first_empty_index;
		for (IdentifierT i = first_empty_index + 1; i < occupied_flags.capacity; i++)
		{
			if (!occupied_flags[i])
			{
				first_empty_index = i;
				break;
			}
		}

		IdentifierT* ret_id = lookup_table.set_value(key, new_id);
		occupied_flags[*ret_id] = true;
		*out_id = *ret_id;
		*out_creation_ptr = &objects[*ret_id];
		object_count++;
	}

	void release(const char* key, IdentifierT* out_id, ObjectT** out_destruction_ptr)
	{
		out_id->invalidate();
		*out_destruction_ptr = 0;
		IdentifierT* id_ptr = lookup_table.get(key);
		if (!id_ptr)
			return;

		IdentifierT id = *id_ptr;
		*out_id = id;

		*out_destruction_ptr = &objects[id];
		occupied_flags[id] = false;
		lookup_table.remove_entry(key);
		object_count--;
	}

	SHMINLINE ObjectT* get_object(IdentifierT id)
	{
		if (!id.is_valid() || !occupied_flags[id])
			return 0;

		return &objects[id];
	}

	SHMINLINE ObjectT* get_object(const char* key)
	{
		IdentifierT* id = lookup_table.get(key);
		if (!id || !occupied_flags[*id])
			return 0;

		return &objects[*id];
	}

	SHMINLINE IdentifierT get_id(const char* key)
	{
		IdentifierT* id = lookup_table.get(key);
		if (!id)
			return IdentifierT::invalid_value;
		
		return *id;
	}

	StorageFlags::Value flags;
	IdentifierT first_empty_index;
	uint32 object_count;
	Sarray<ObjectT> objects;
	Sarray<bool8> occupied_flags;
	HashtableRH<IdentifierT, lookup_key_buffer_size> lookup_table;
	
	struct Iterator
	{
		Iterator(LinearHashedStorage* target) : storage(target), cursor(0), counter(0), available_count(target->object_count) { }

		SHMINLINE ObjectT* get_next() 
		{
			if (counter >= available_count)
				return 0;

			while (!storage->occupied_flags[cursor])
				cursor++;

			ObjectT* obj = &storage->objects[cursor];
			cursor++;
			counter++;
			return obj;
		}

		LinearHashedStorage* storage;
		uint32 cursor;
		uint32 available_count;
		uint32 counter;
	};

	Iterator get_iterator()
	{
		return Iterator(this);
	}
};
