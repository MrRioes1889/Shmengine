#pragma once

#include "Defines.hpp"

typedef uint32 UniqueId;

UniqueId identifier_acquire_new_id(void* owner);

void identifier_release_id(UniqueId id);
