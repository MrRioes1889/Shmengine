#include "Identifier.hpp"

#include "containers/Darray.hpp"

static Darray<void*> owners;

UniqueId identifier_acquire_new_id(void* owner)
{
	if (!owners.data)
	{
		owners.init(128, 0);
		owners.push(0);
	}

	for (uint32 i = 1; i < owners.count; i++)
	{
		if (owners[i] == 0)
		{
			owners[i] = owner;
			return i;
		}
	}

	owners.emplace(owner);
	return owners.count - 1;
		
}

void identifier_release_id(UniqueId id)
{
	if (!id || id >= owners.count)
		return;

	owners[id] = 0;
}
