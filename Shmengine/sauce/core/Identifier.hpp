#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"

struct Id8
{
	static constexpr uint8 valid_bit_mask = 0x80;
	uint8 id;

	Id8() : id(0) {}
	Id8(uint8 value) { SHMASSERT_MSG(value < valid_bit_mask, "Value exceeds id range (2^7-1)"); id = value | valid_bit_mask; }
	
	operator uint8() { return id & ~valid_bit_mask; }

	bool8 is_valid() { return (id & valid_bit_mask) != 0; }
	void validate() { id = id | valid_bit_mask; }
	void invalidate() { id = id & ~valid_bit_mask; }
};

struct Id16
{
	static constexpr uint16 valid_bit_mask = 0x8000;
	uint16 id;

	Id16() : id(0) {}
	Id16(uint16 value) { SHMASSERT_MSG(value < valid_bit_mask, "Value exceeds id range (2^15-1)"); id = value | valid_bit_mask; }
	
	operator uint16() { return id & ~valid_bit_mask; }

	bool8 is_valid() { return (id & valid_bit_mask) != 0; }
	void validate() { id = id | valid_bit_mask; }
	void invalidate() { id = id & ~valid_bit_mask; }
};

struct Id32
{
	static constexpr uint32 valid_bit_mask = 0x80000000;
	uint32 id;

	Id32() : id(0) {}
	Id32(uint32 value) { SHMASSERT_MSG(value < valid_bit_mask, "Value exceeds id range (2^31-1)"); id = value | valid_bit_mask; }
	
	operator uint32() { return id & ~valid_bit_mask; }

	bool8 is_valid() { return (id & valid_bit_mask) != 0; }
	void validate() { id = id | valid_bit_mask; }
	void invalidate() { id = id & ~valid_bit_mask; }
};

struct Id64
{
	static constexpr uint64 valid_bit_mask = 0x8000000000000000;
	uint64 id;

	Id64() : id(0) {}
	Id64(uint64 value) { SHMASSERT_MSG(value < valid_bit_mask, "Value exceeds id range (2^63-1)"); id = value | valid_bit_mask; }
	
	operator uint64() { return id & ~valid_bit_mask; }

	bool8 is_valid() { return (id & valid_bit_mask) != 0; }
	void validate() { id = id | valid_bit_mask; }
	void invalidate() { id = id & ~valid_bit_mask; }
};

SHMAPI UniqueId identifier_acquire_new_id(void* owner);

SHMAPI void identifier_release_id(UniqueId id);
