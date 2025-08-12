#pragma once

#include "Defines.hpp"

#include "Sarray.hpp"
#include "core/Identifier.hpp"
#include "core/Memory.hpp"
#include "core/Assert.hpp"
#include "core/Logging.hpp"
#include "utility/CString.hpp"

SHMINLINE uint32 hash_key(const char* key, uint32 hash_limit)
{
	const uint64 multiplier = 97;

	unsigned const char* us;
	uint64 hash = 0;

	for (us = (unsigned const char*)key; *us; us++)
	{
		hash = hash * multiplier + *us;
	}

	hash %= hash_limit;
	return (uint32)hash;
}

namespace HashtableOAFlags
{
	enum : uint8
	{
		ExternalMemory = 1 << 0
	};
	typedef uint8 Value;
}

template <typename ObjectT>
struct HashtableOA
{
	HashtableOA(const HashtableOA& other) = delete;
	HashtableOA(HashtableOA&& other) = delete;
	HashtableOA() : flags(0), buffer({}) {}

	SHMINLINE uint64 get_external_size_requirement(uint32 count) { return count * sizeof(ObjectT); }

	SHMINLINE HashtableOA(uint32 count, HashtableOAFlags::Value creation_flags, AllocationTag tag = AllocationTag::Dict)
	{
		init(count, creation_flags, tag);
	}

	SHMINLINE HashtableOA(uint32 count, HashtableOAFlags::Value creation_flags, void* memory)
	{
		init(count, creation_flags, AllocationTag::Dict, memory);
	}

	SHMINLINE void init(uint32 count, HashtableOAFlags::Value creation_flags, AllocationTag tag = AllocationTag::Dict, void* memory = 0)
	{
		SHMASSERT_MSG(count, "Element count cannot be null!");

		flags = creation_flags;

		if (memory)
			flags |= HashtableOAFlags::ExternalMemory;
		else
			flags &= ~HashtableOAFlags::ExternalMemory;

		buffer.init(count, 0, tag, memory);
	}

	SHMINLINE void free_data()
	{
		buffer.free_data();
	}

	SHMINLINE ~HashtableOA()
	{
		free_data();
	}

	SHMINLINE ObjectT& operator[](uint32 index)
	{
		return buffer[index];
	}

	SHMINLINE uint32 get_capacity()
	{
		return buffer.capacity;
	}

	SHMINLINE bool8 set_value(const char* name, const ObjectT& value)
	{
		uint32 hash = hash_key(name, buffer.capacity);
		buffer[hash] = value;
		return true;
	}

	SHMINLINE ObjectT get_value(const char* name)
	{
		uint32 hash = hash_key(name, buffer.capacity);
		return buffer[hash];
	}

	SHMINLINE ObjectT& get_ref(const char* name)
	{
		uint32 hash = hash_key(name, buffer.capacity);
		return buffer[hash];
	}

	SHMINLINE bool8 floodfill(const ObjectT& value)
	{
		for (uint32 i = 0; i < buffer.capacity; i++)
			buffer[i] = value;

		return true;
	}


private:

	HashtableOAFlags::Value flags;
	Sarray<ObjectT> buffer;
	
};


namespace HashtableCHFlags
{
	enum : uint8
	{
		ExternalMemory = 1 << 0,
	};
	typedef uint8 Value;
}

// Hashtable using a coalesced hashing scheme (colliding keys stored in same array with offsets)
template <typename ObjectT, uint32 key_buffer_size>
struct HashtableCH
{
	typedef Id32 NodeIndex;
	struct KeyNode
	{
		char key_string[key_buffer_size];
		NodeIndex next_index;
	};

	HashtableCH(const HashtableCH& other) = delete;
	HashtableCH(HashtableCH&& other) = delete;
	HashtableCH() : flags(0), key_arr({}), object_arr({}), hashed_capacity(0) {}

	SHMINLINE uint64 get_external_size_requirement(uint32 hashed_count, uint32 collision_buffer_count) { return (hashed_count + collision_buffer_count) * (sizeof(ObjectT) + sizeof(KeyNode)); }

	SHMINLINE HashtableCH(uint32 count, uint32 collision_buffer_count, HashtableCHFlags::Value creation_flags, AllocationTag tag = AllocationTag::Dict, void* memory = 0)
	{
		init(count, collision_buffer_count, creation_flags, tag, memory);
	}

	SHMINLINE void init(uint32 count, uint32 collision_buffer_count, HashtableCHFlags::Value creation_flags, AllocationTag tag = AllocationTag::Dict, void* memory = 0)
	{
		SHMASSERT_MSG(count, "Element count cannot be null!");

		flags = creation_flags;
		hashed_capacity = count;

		uint32 total_count = count + collision_buffer_count;
		if (memory)
		{
			flags |= HashtableCHFlags::ExternalMemory;
		}
		else
		{
			flags &= ~HashtableCHFlags::ExternalMemory;
			memory = Memory::allocate(get_external_size_requirement(total_count), tag);
		}

		object_arr.init(total_count, 0, tag, memory);
		key_arr.init(total_count, 0, tag, PTR_BYTES_OFFSET(memory, object_arr.size()));
	}

	SHMINLINE void destroy()
	{
		if (object_arr.data && !(flags & HashtableCHFlags::ExternalMemory))
			Memory::free_memory(object_arr.data);

		key_arr.free_data();
		object_arr.free_data();
		hashed_capacity = 0;
	}

	SHMINLINE ~HashtableCH()
	{
		destroy();
	}

	SHMINLINE ObjectT* set_value(const char* key, const ObjectT& value)
	{
		ObjectT* object = insert_key(key);
		if (!object)
			return 0;

		(*object) = value;
		return object;
	}

	ObjectT* insert_key(const char* key)
	{		
		NodeIndex hash = hash_key(key, hashed_capacity);
		KeyNode* prev_entry = 0;
		NodeIndex node_index = lookup(key, hash, &prev_entry);
		if (!node_index.is_valid())
			node_index = insert(key, hash, prev_entry);

		return node_index.is_valid() ? &object_arr[node_index] : 0;
	}

	void remove_entry(const char* key)
	{
		NodeIndex hash = hash_key(key, hashed_capacity);
		KeyNode* prev_entry = 0;
		NodeIndex node_index = lookup(key, hash, &prev_entry);
		if (!node_index.is_valid())
			return;

		remove(node_index, prev_entry);		
	}

	ObjectT* get(const char* key)
	{
		uint32 hash = hash_key(key, hashed_capacity);
		KeyNode* prev_node = 0;
		NodeIndex node_index = lookup(key, hash, &prev_node);
		return node_index.is_valid() ? &object_arr[node_index] : 0;
	}

private:

	HashtableCHFlags::Value flags;
	uint32 hashed_capacity;
	Sarray<KeyNode> key_arr;
	Sarray<ObjectT> object_arr;

	NodeIndex lookup(const char* key, NodeIndex index, KeyNode** prev_node)
	{
		KeyNode* current_node = 0;
		current_node = &key_arr[index];

		if (current_node->key && CString::equal(key, current_node->key))
			return index;

		(*prev_node) = current_node;
		if (current_node->next_index > 0)
			return lookup(key, (*prev_node)->next_index, prev_node);

		return NodeIndex::invalid_value;
	}

	NodeIndex insert(const char* key, NodeIndex hash, KeyNode* prev_node)
	{
		KeyNode* new_node = 0;
		NodeIndex insert_index = NodeIndex::invalid_value;

		if (!key_arr[hash].key)
		{
			new_node = &key_arr[hash];
			insert_index = hash;
		}
		else
		{
			insert_index = hashed_capacity;
			for (;; insert_index++)
			{
				if (insert_index >= key_arr.capacity)
				{
					if (flags & HashtableCHFlags::ExternalMemory || insert_index >= NodeIndex::invalid_value)
						return NodeIndex::invalid_value;

					key_arr.resize(SHMIN((uint32)(key_arr.capacity * 1.5f), (uint32)NodeIndex::invalid_value));
					object_arr.resize(key_arr.capacity);
				}

				if (!key_arr[insert_index].key_string[0])
					break;
			}

			prev_node->next_index = insert_index;
			new_node = &key_arr[insert_index];
		}

		CString::copy(key, new_node->key, key_buffer_size);
		return insert_index;
	}

	void remove(NodeIndex index, KeyNode* prev_node)
	{
		KeyNode* node = &key_arr[index];

		if (prev_node)
		{
			prev_node->next_index = node->next_index;
			node->next_index = 0;
		}

		node->key_string[0] = 0;
		if constexpr (std::is_destructible_v<ObjectT> && !std::is_trivially_destructible_v<ObjectT>)
			object_arr[index].~ObjectT();
	}
};


namespace HashtableRHFlags
{
	enum : uint8
	{
		ExternalMemory = 1 << 0
	};
	typedef uint8 Value;
}

// Hashtable using a "Robin Hood" hashing scheme
template <typename ObjectT, uint32 key_buffer_size>
struct HashtableRH
{
	typedef Id32 NodeIndex;
	struct KeyNode
	{
		uint16 psl;
		char key_string[key_buffer_size];
	};

	HashtableRH(const HashtableRH& other) = delete;
	HashtableRH(HashtableRH&& other) = delete;
	HashtableRH() : flags(0), key_arr({}), object_arr({}), key_count(0) {}

	SHMINLINE uint64 get_external_size_requirement(uint32 count) { return count * (sizeof(KeyNode) + sizeof(ObjectT)); }

	SHMINLINE HashtableRH(uint32 count, HashtableRHFlags::Value creation_flags, AllocationTag tag = AllocationTag::Dict, void* memory = 0)
	{
		init(count, creation_flags, tag, memory);
	}

	void init(uint32 count, HashtableRHFlags::Value creation_flags, AllocationTag tag = AllocationTag::Dict, void* memory = 0)
	{
		SHMASSERT_MSG(count, "Element count cannot be null!");

		flags = creation_flags;
		key_count = 0;

		if (memory)
		{
			flags |= HashtableRHFlags::ExternalMemory;
		}
		else
		{
			flags &= ~HashtableRHFlags::ExternalMemory;
			memory = Memory::allocate(get_external_size_requirement(count), tag);
		}

		object_arr.init(count, 0, tag, memory);
		key_arr.init(count, 0, tag, PTR_BYTES_OFFSET(memory, object_arr.size()));
	}

	void destroy()
	{
		if (object_arr.data && !(flags & HashtableRHFlags::ExternalMemory))
			Memory::free_memory(object_arr.data);

		key_arr.free_data();
		object_arr.free_data();
		key_count = 0;
	}

	SHMINLINE ~HashtableRH()
	{
		destroy();
	}

	SHMINLINE ObjectT* set_value(const char* key, const ObjectT& value)
	{
		ObjectT* object = insert_key(key);
		if (!object)
			return 0;

		(*object) = value;
		return object;
	}

	ObjectT* insert_key(const char* key)
	{		
		NodeIndex hash = hash_key(key, key_arr.capacity);
		hash_cache = hash;
		NodeIndex node_index = lookup(key, hash);
		if (!node_index.is_valid())
			node_index = insert(key, hash);

		return node_index.is_valid() ? &object_arr[node_index] : 0;
	}

	void remove_entry(const char* key)
	{
		NodeIndex hash = hash_key(key, key_arr.capacity);
		hash_cache = hash;
		NodeIndex node_index = lookup(key, hash);
		if (!node_index.is_valid())
			return;

		remove(node_index);		
	}

	ObjectT* get(const char* key)
	{
		NodeIndex hash = hash_key(key, key_arr.capacity);
		hash_cache = hash;
		NodeIndex node_index = lookup(key, hash);
		return node_index.is_valid() ? &object_arr[node_index] : 0;
	}

	SHMINLINE bool8 is_full() { return key_count >= key_arr.capacity; }
	SHMINLINE uint32 get_key_count() { return key_count; }

private:

	HashtableRHFlags::Value flags;
	uint32 key_count;
	NodeIndex hash_cache;
	Sarray<KeyNode> key_arr;
	Sarray<ObjectT> object_arr;

	NodeIndex lookup(const char* key, NodeIndex hash)
	{
		uint16 lookup_psl = 0;
		NodeIndex index = hash;

		for (;; index = (index + 1) % key_arr.capacity, lookup_psl++)
		{
			KeyNode* node = &key_arr[index];
			if (lookup_psl > node->psl)
				break;

			if (node->key_string[0] && CString::equal(key, node->key_string))
				return index;
		}

		return NodeIndex::invalid_value;
	}
	
	SHMINLINE void swap_nodes(NodeIndex insert_index, KeyNode* key_node, ObjectT* object_node)
	{
		KeyNode tmp_key;
		Memory::copy_memory(key_node, &tmp_key, sizeof(key_arr[0]));
		Memory::copy_memory(&key_arr[insert_index], key_node, sizeof(key_arr[0]));
		Memory::copy_memory(&tmp_key, &key_arr[insert_index], sizeof(key_arr[0]));

		// NOTE: Using generic buffer and memcopies to avoid unnecessary constructor/destructor calls
		char tmp[sizeof(ObjectT) + 1] = {};
		Memory::copy_memory(object_node, tmp, sizeof(object_arr[0]));
		Memory::copy_memory(&object_arr[insert_index], object_node, sizeof(object_arr[0]));
		Memory::copy_memory(tmp, &object_arr[insert_index], sizeof(object_arr[0]));
	}

	NodeIndex insert(const char* key, NodeIndex hash)
	{
		if (key_count >= key_arr.capacity)
			return NodeIndex::invalid_value;

		KeyNode insert_key_node = {};
		CString::copy(key, insert_key_node.key_string, key_buffer_size);
		insert_key_node.psl = 0;

		ObjectT insert_object = {};
		NodeIndex index = hash;
		NodeIndex first_insert_index = NodeIndex::invalid_value;

		for (;; index = (index + 1) % key_arr.capacity, insert_key_node.psl++)
		{
			KeyNode* node = &key_arr[index];

			if (!node->key_string[0])
			{
				Memory::copy_memory(&insert_key_node, node, sizeof(key_arr[0]));
				Memory::copy_memory(&insert_object, &object_arr[index], sizeof(object_arr[0]));
				if (!first_insert_index.is_valid())
					first_insert_index = index;

				key_count++;
				return first_insert_index;
			}

			if (insert_key_node.psl > node->psl)
			{
				swap_nodes(index, &insert_key_node, &insert_object);
				if (!first_insert_index.is_valid())
					first_insert_index = index;
			}
		}
	}

	void remove(NodeIndex remove_index)
	{
		if constexpr (std::is_destructible_v<ObjectT> && !std::is_trivially_destructible_v<ObjectT>)
			object_arr[remove_index].~ObjectT();
		key_arr[remove_index].psl = 0;
		key_arr[remove_index].key_string[0] = 0;

		uint32 backshift_count = 0;
		uint32 backshift_start = (remove_index + 1) % key_arr.capacity;
		for (uint32 index = backshift_start;; index = (index + 1) % key_arr.capacity)
		{
			KeyNode* key_node = &key_arr[index];
			if (!key_node->psl)
				break;

			key_node->psl--;
			backshift_count++;
		}
		uint32 clear_index = (remove_index + backshift_count) % key_arr.capacity;

		if (remove_index + backshift_count < key_arr.capacity)
		{
			Memory::copy_memory(&key_arr[backshift_start], &key_arr[remove_index], backshift_count * sizeof(key_arr[0]));
			Memory::copy_memory(&object_arr[backshift_start], &object_arr[remove_index], backshift_count * sizeof(object_arr[0]));
		}
		else
		{
			Memory::copy_memory(&key_arr[backshift_start], &key_arr[remove_index], (key_arr.capacity - remove_index - 1) * sizeof(key_arr[0]));
			Memory::copy_memory(&object_arr[backshift_start], &object_arr[remove_index], (object_arr.capacity - remove_index - 1) * sizeof(object_arr[0]));
			Memory::copy_memory(&key_arr[0], &key_arr[key_arr.capacity - 1], sizeof(key_arr[0]));
			Memory::copy_memory(&object_arr[0], &object_arr[object_arr.capacity - 1], sizeof(object_arr[0]));

			backshift_count -= key_arr.capacity - remove_index;
			Memory::copy_memory(&key_arr[1], &key_arr[0], backshift_count * sizeof(key_arr[0]));
			Memory::copy_memory(&object_arr[1], &object_arr[0], backshift_count * sizeof(object_arr[0]));
		}

		Memory::zero_memory(&object_arr[clear_index], sizeof(object_arr[0]));
		key_arr[clear_index].psl = 0;
		key_arr[clear_index].key_string[0] = 0;
		key_count--;
	}
};
