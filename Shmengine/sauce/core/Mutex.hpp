#pragma once

#include "Defines.hpp"

namespace Threading
{

	struct Mutex
	{
		void* internal_data;
	};

	bool32 mutex_create(Mutex* out_mutex);
	void mutex_destroy(Mutex* mutex);

	bool32 mutex_lock(Mutex* mutex);
	bool32 mutex_unlock(Mutex* mutex);

}
