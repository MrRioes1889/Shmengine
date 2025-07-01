#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"

struct Id8
{
	static constexpr uint8 invalid_value = 0xFF;
	uint8 id;

	Id8() : id(invalid_value) {}
	Id8(uint8 value) { id = value; }
	
	SHMINLINE operator uint8() { return id; }
	SHMINLINE Id8 operator++(int) { Id8 old = *this; id++; return old; }
	SHMINLINE Id8 operator--(int) { Id8 old = *this; id++; return old; }

	SHMINLINE bool8 is_valid() { return id != invalid_value; }
	SHMINLINE void invalidate() { id = invalid_value; }
};

struct Id16
{
	static constexpr uint16 invalid_value = 0xFFFF;
	uint16 id;

	Id16() : id(invalid_value) {}
	Id16(uint16 value) { id = value; }
	
	SHMINLINE operator uint16() { return id; }
	SHMINLINE Id16 operator++(int) { Id16 old = *this; id++; return old; }
	SHMINLINE Id16 operator--(int) { Id16 old = *this; id--; return old; }

	SHMINLINE bool8 is_valid() { return id != invalid_value; }
	SHMINLINE void invalidate() { id = invalid_value; }
};

struct Id32
{
	static constexpr uint32 invalid_value = 0xFFFFFFFF;
	uint32 id;

	Id32() : id(invalid_value) {}
	Id32(uint32 value) { id = value; }

	SHMINLINE operator uint32() { return id; }
	SHMINLINE Id32 operator++(int) { Id32 old = *this; id++; return old; }
	SHMINLINE Id32 operator--(int) { Id32 old = *this; id--; return old; }

	SHMINLINE bool8 is_valid() { return id != invalid_value; }
	SHMINLINE void invalidate() { id = invalid_value; }
};

struct Id64
{
	static constexpr uint64 invalid_value = 0xFFFFFFFFFFFFFFFF;
	uint64 id;

	Id64() : id(invalid_value) {}
	Id64(uint64 value) { id = value; }

	SHMINLINE operator uint64() { return id; }
	SHMINLINE Id64 operator++(int) { Id64 old = *this; id++; return old; }
	SHMINLINE Id64 operator--(int) { Id64 old = *this; id--; return old; }

	SHMINLINE bool8 is_valid() { return id != invalid_value; }
	SHMINLINE void invalidate() { id = invalid_value; }
};

SHMAPI UniqueId identifier_acquire_new_id(void* owner);

SHMAPI void identifier_release_id(UniqueId id);
