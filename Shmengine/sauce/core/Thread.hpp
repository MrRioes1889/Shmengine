#pragma once

#include "Defines.hpp"

namespace Threading
{
	struct Thread
	{
		void* internal_data;
		uint32 thread_id;
	};

	typedef uint32(*fp_thread_start)(void*);

	bool32 thread_create(fp_thread_start start_function, void* params, bool32 auto_detach, Thread* out_thread);
	void thread_destroy(Thread* thread);

	void thread_detach(Thread* thread);
	void thread_cancel(Thread* thread);
	bool32 thread_is_active(Thread* thread);
	void thread_sleep(Thread* thread, uint32 ms);

	uint64 get_thread_id();
}


