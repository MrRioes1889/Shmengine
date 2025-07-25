#include "core/Thread.hpp"
#include "core/Mutex.hpp"
#include "core/Logging.hpp"
#include "platform/Platform.hpp"

#if _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Platform
{

	int32 get_processor_count()
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		SHMINFOV("%u processor cores detected.", sysinfo.dwNumberOfProcessors);
		return sysinfo.dwNumberOfProcessors;
	}

}


namespace Threading
{

	bool8 thread_create(FP_thread_start start_function, void* params, bool8 auto_detach, Thread* out_thread)
	{

		out_thread->internal_data = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)start_function, params, 0, (DWORD*)&out_thread->thread_id);
		if (!out_thread->internal_data)
			return false;

		if (auto_detach)
			CloseHandle(out_thread->internal_data);

		SHMDEBUGV("Starting process on thread id: %u", out_thread->thread_id);
		return true;

	}

	void thread_destroy(Thread* thread)
	{
		DWORD exit_code;
		GetExitCodeThread(thread->internal_data, &exit_code);
		CloseHandle(thread->internal_data);
		thread->internal_data = 0;
		thread->thread_id = 0;
	}

	void thread_detach(Thread* thread)
	{
		CloseHandle(thread->internal_data);
		thread->internal_data = 0;
	}

	/*void thread_cancel(Thread* thread)
	{
		TerminateThread(thread->internal_data, 0);
		thread->internal_data = 0;
	}*/

	bool8 thread_is_active(Thread* thread)
	{
		DWORD exit_code = WaitForSingleObject(thread->internal_data, 0);
		if (exit_code == WAIT_TIMEOUT)
			return true;
		else
			return false;
		
	}

	void thread_sleep(Thread* thread, uint32 ms)
	{
		Platform::sleep(ms);
	}

	uint64 get_thread_id()
	{
		return GetCurrentThreadId();
	}

	bool8 mutex_create(Mutex* out_mutex)
	{
		out_mutex->internal_data = CreateMutex(0, 0, 0);
		if (!out_mutex->internal_data) {
			SHMERROR("Unable to create mutex.");
			return false;
		}

		return true;
	}

	void mutex_destroy(Mutex* mutex)
	{
		CloseHandle(mutex->internal_data);
		mutex->internal_data = 0;
	}

	bool8 mutex_lock(Mutex* mutex)
	{
		DWORD result = WaitForSingleObject(mutex->internal_data, INFINITE);
		switch (result) {
		case WAIT_OBJECT_0:
		{
			return true;
		}
		case WAIT_ABANDONED:
		{
			SHMERROR("Mutex lock failed.");
			return false;
		}			
		}
		return true;
	}

	bool8 mutex_unlock(Mutex* mutex)
	{
		int32 result = ReleaseMutex(mutex->internal_data);
		if (!result)
			SHMERROR("Mutex unlock failed.");

		return result != 0;
	}

}

#endif