#pragma once

#include "Defines.hpp"

SHMAPI UniqueId identifier_acquire_new_id(void* owner);

SHMAPI void identifier_release_id(UniqueId id);
