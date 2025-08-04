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

enum class StorageReturnCode
{
	OutOfMemory = 0,
	NewEntry,
	AlreadyExisted
};

enum class StorageSlotState : uint8
{
	Empty = 0,
	Creating,
	Available,
	Destroying
};

template <typename ObjectT, typename IdentifierT>
struct LinearStorage
{
	
	SHMINLINE uint64 get_external_size_requirement(uint32 count) { return objects.get_external_size_requirement(count) + slot_states.get_external_size_requirement(count); }

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
		void* slot_states_data = PTR_BYTES_OFFSET(memory, objects.size());
		slot_states.init(count, 0, tag, slot_states_data);

		object_count = 0;
		first_empty_index = 0;
	}

	void destroy()
	{
		if (objects.data && !(flags & StorageFlags::ExternalMemory))
			Memory::free_memory(objects.data);

		objects.free_data();
		slot_states.free_data();
	}

	StorageReturnCode acquire(IdentifierT* out_id, ObjectT** out_creation_ptr)
	{
		out_id->invalidate();
		*out_creation_ptr = 0;

		if (object_count >= objects.capacity)
			return StorageReturnCode::OutOfMemory;

		IdentifierT id = first_empty_index;
		for (IdentifierT i = first_empty_index + 1; i < slot_states.capacity; i++)
		{
			if (slot_states[i] == StorageSlotState::Empty)
			{
				first_empty_index = i;
				break;
			}
		}

		slot_states[id] = StorageSlotState::Creating;
		*out_id = id;
		*out_creation_ptr = &objects[id];
		object_count++;

		return StorageReturnCode::NewEntry;
	}

	void release(IdentifierT id, ObjectT** out_destruction_ptr)
	{
		*out_destruction_ptr = 0;
		if (slot_states[id] == StorageSlotState::Empty)
			return;

		*out_destruction_ptr = &objects[id];
		slot_states[id] = StorageSlotState::Destroying;
	}

	void verify_write(IdentifierT id)
	{
		StorageSlotState current_state = slot_states[id];
		if (current_state == StorageSlotState::Creating)
		{
			slot_states[id] = StorageSlotState::Available;
		}
		else if (current_state == StorageSlotState::Destroying)
		{
			slot_states[id] = StorageSlotState::Empty;
			object_count--;
			if (id < first_empty_index) 
				first_empty_index = id;
		}
	}

	void revert_write(IdentifierT id)
	{
		StorageSlotState current_state = slot_states[id];
		if (current_state == StorageSlotState::Creating)
		{
			slot_states[id] = StorageSlotState::Empty;
			object_count--;
			if (id < first_empty_index) 
				first_empty_index = id;
		}
		else if (current_state == StorageSlotState::Destroying)
		{
			slot_states[id] = StorageSlotState::Available;
		}
	}

	SHMINLINE ObjectT* get_object(IdentifierT id)
	{
		if (slot_states[id] != StorageSlotState::Available)
			return 0;

		return &objects[id];
	}

	StorageFlags::Value flags;
	IdentifierT first_empty_index;
	uint32 object_count;
	Sarray<ObjectT> objects;
	Sarray<StorageSlotState> slot_states;
	
	struct Iterator
	{
		Iterator(LinearStorage* target) : storage(target), cursor(0), counter(0), object_count(target->object_count) { }

		SHMINLINE IdentifierT get_next() 
		{
			if (counter >= object_count)
				return IdentifierT::invalid_value;

			while (storage->slot_states[cursor] == StorageSlotState::Empty)
				cursor++;

			IdentifierT id = cursor;
			cursor++;
			counter++;
			return id;
		}

		LinearStorage* storage;
		IdentifierT cursor;
		uint32 object_count;
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
	
	SHMINLINE uint64 get_external_size_requirement(uint32 count) { return objects.get_external_size_requirement(count) + slot_states.get_external_size_requirement(count) + lookup_table.get_external_size_requirement(count); }

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
		slot_states.init(count, 0, tag, slot_states_data);
		void* hashtable_data = PTR_BYTES_OFFSET(slot_states_data, slot_states.size());
		lookup_table.init(count, 0, tag, hashtable_data);

		object_count = 0;
		first_empty_index = 0;
	}

	void destroy()
	{
		if (objects.data && !(flags & StorageFlags::ExternalMemory))
			Memory::free_memory(objects.data);

		objects.free_data();
		slot_states.free_data();
		lookup_table.destroy();
	}

	StorageReturnCode acquire(const char* key, IdentifierT* out_id, ObjectT** out_creation_ptr)
	{
		out_id->invalidate();
		*out_creation_ptr = 0;

		if (object_count >= objects.capacity)
			return StorageReturnCode::OutOfMemory;

		if (IdentifierT* existing_id = lookup_table.get(key))
		{
			*out_id = *existing_id;
			return StorageReturnCode::AlreadyExisted;
		}

		IdentifierT new_id = first_empty_index;
		for (IdentifierT i = first_empty_index + 1; i < slot_states.capacity; i++)
		{
			if (slot_states[i] == StorageSlotState::Empty)
			{
				first_empty_index = i;
				break;
			}
		}

		IdentifierT* ret_id = lookup_table.set_value(key, new_id);
		slot_states[*ret_id] = StorageSlotState::Creating;
		*out_id = *ret_id;
		*out_creation_ptr = &objects[*ret_id];
		object_count++;

		return StorageReturnCode::NewEntry;
	}

	void release(const char* key, IdentifierT* out_id, ObjectT** out_destruction_ptr)
	{
		out_id->invalidate();
		*out_destruction_ptr = 0;
		IdentifierT* id = lookup_table.get(key);
		if (!id)
			return;

		*out_id = *id;
		*out_destruction_ptr = &objects[*id];
		slot_states[*id] = StorageSlotState::Destroying;
		lookup_table.remove_entry(key);
	}

	void verify_write(IdentifierT id)
	{
		StorageSlotState current_state = slot_states[id];
		if (current_state == StorageSlotState::Creating)
		{
			slot_states[id] = StorageSlotState::Available;
		}
		else if (current_state == StorageSlotState::Destroying)
		{
			slot_states[id] = StorageSlotState::Empty;
			object_count--;
			if (id < first_empty_index) 
				first_empty_index = id;
		}
	}

	void revert_write(IdentifierT id)
	{
		StorageSlotState current_state = slot_states[id];
		if (current_state == StorageSlotState::Creating)
		{
			slot_states[id] = StorageSlotState::Empty;
			object_count--;
			if (id < first_empty_index) 
				first_empty_index = id;
		}
		else if (current_state == StorageSlotState::Destroying)
		{
			slot_states[id] = StorageSlotState::Available;
		}
	}

	SHMINLINE ObjectT* get_object(IdentifierT id)
	{
		if (slot_states[id] != StorageSlotState::Available)
			return 0;

		return &objects[id];
	}

	SHMINLINE ObjectT* get_object(const char* key)
	{
		IdentifierT* id = lookup_table.get(key);
		if (!id || slot_states[*id] != StorageSlotState::Available)
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
	Sarray<StorageSlotState> slot_states;
	HashtableRH<IdentifierT, lookup_key_buffer_size> lookup_table;
	
	struct Iterator
	{
		Iterator(LinearHashedStorage* target) : storage(target), cursor(0), counter(0), object_count(target->object_count) { }

		SHMINLINE ObjectT* get_next() 
		{
			if (counter >= object_count)
				return 0;

			while (storage->slot_states[cursor] == StorageSlotState::Empty)
				cursor++;

			ObjectT* obj = &storage->objects[cursor];
			cursor++;
			counter++;
			return obj;
		}

		LinearHashedStorage* storage;
		uint32 cursor;
		uint32 object_count;
		uint32 counter;
	};

	Iterator get_iterator()
	{
		return Iterator(this);
	}
};
