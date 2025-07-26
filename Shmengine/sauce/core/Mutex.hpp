#pragma once

#include "Defines.hpp"

namespace Threading
{

	typedef uint8* Mutex;

	SHMAPI bool8 mutex_create(Mutex* out_mutex);
	SHMAPI void mutex_destroy(Mutex* mutex);

	SHMAPI bool8 mutex_lock(Mutex mutex);
	SHMAPI bool8 mutex_unlock(Mutex mutex);

}
