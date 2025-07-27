#include "JobSystem.hpp"

#include "core/Thread.hpp"
#include "core/Mutex.hpp"
#include "core/Logging.hpp"
#include "core/FrameData.hpp"
#include "containers/RingQueue.hpp"

#include "optick.h"

#define MT_ENABLED 0

namespace JobSystem
{

	struct JobThread
	{
		uint32 system_id;
		uint32 index;
		JobTypeFlags::Value type_flags;
		Threading::Thread thread;
		JobInfo info;
		Threading::Mutex info_mutex; 
	};

	struct JobResultEntry
	{
		uint32 id;
		uint32 user_data_size;
		void* user_data;
		FP_job_on_complete on_complete;
	};

	struct SystemState
	{
		bool8 is_running;

		Sarray<JobThread> job_threads;

		RingQueue<JobInfo> low_prio_queue;
		RingQueue<JobInfo> normal_prio_queue;
		RingQueue<JobInfo> high_prio_queue;

		Threading::Mutex low_prio_queue_mutex;
		Threading::Mutex normal_prio_queue_mutex;
		Threading::Mutex high_prio_queue_mutex;

		uint32 pending_results_count;
		JobResultEntry pending_results[SystemConfig::max_job_results_count];
		Threading::Mutex results_mutex;
	};

	static SystemState* system_state = 0;

	static void store_result(FP_job_on_complete callback, uint32 user_data_size, void* user_data);
	static uint32 job_thread_run(void* params);
	static void process_queue(RingQueue<JobInfo>& queue, Threading::Mutex queue_mutex);

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{

		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->is_running = true;

		uint64 thread_array_size = system_state->job_threads.get_external_size_requirement(sys_config->job_thread_count);
		void* thread_array_data = allocator_callback(allocator, thread_array_size);
		system_state->job_threads.init(sys_config->job_thread_count, 0, AllocationTag::Array, thread_array_data);

		system_state->pending_results_count = 0;

		system_state->low_prio_queue.init(1024, 0);
		system_state->normal_prio_queue.init(1024, 0);
		system_state->high_prio_queue.init(1024, 0);

		for (uint32 i = 0; i < SystemConfig::max_job_results_count; i++)
			system_state->pending_results[i].id = Constants::max_u32;

		SHMDEBUGV("Main thread id is: %u", Threading::get_thread_id());
		SHMDEBUGV("Spawning %u job threads.", system_state->job_threads.capacity);

		for (uint32 i = 0; i < system_state->job_threads.capacity; i++)
		{
			system_state->job_threads[i].index = i;
			system_state->job_threads[i].type_flags = sys_config->type_flags[i];
			if (!Threading::thread_create(job_thread_run, &system_state->job_threads[i].index, false, &system_state->job_threads[i].thread))
			{
				SHMFATAL("Failed creating requested count of job threads!");
				return false;
			}
		}

		if (!Threading::mutex_create(&system_state->results_mutex) ||
			!Threading::mutex_create(&system_state->low_prio_queue_mutex) ||
			!Threading::mutex_create(&system_state->normal_prio_queue_mutex) ||
			!Threading::mutex_create(&system_state->high_prio_queue_mutex))
		{
			SHMERROR("Failed creating job system mutexes!");
			return false;
		}

		return true;

	}

	void system_shutdown(void* state)
	{

		system_state->is_running = false;

		system_state->low_prio_queue.free_data();
		system_state->normal_prio_queue.free_data();
		system_state->high_prio_queue.free_data();

		for (uint32 i = 0; i < system_state->job_threads.capacity; i++)
		{
			Threading::thread_destroy(&system_state->job_threads[i].thread);
		}
			
		Threading::mutex_destroy(&system_state->results_mutex);
		Threading::mutex_destroy(&system_state->low_prio_queue_mutex);
		Threading::mutex_destroy(&system_state->normal_prio_queue_mutex);
		Threading::mutex_destroy(&system_state->high_prio_queue_mutex);

		system_state = 0;

	}


	bool8 update(void* state, const FrameData* frame_data)
	{

		if (!system_state->is_running)
			return true;

		process_queue(system_state->low_prio_queue, system_state->low_prio_queue_mutex);
		process_queue(system_state->normal_prio_queue, system_state->normal_prio_queue_mutex);
		process_queue(system_state->high_prio_queue, system_state->high_prio_queue_mutex);

		for (uint32 i = 0; i < SystemConfig::max_job_results_count && system_state->pending_results_count > 0; i++)
		{

			Threading::mutex_lock(system_state->results_mutex);
			JobResultEntry entry = system_state->pending_results[i];
			Threading::mutex_unlock(system_state->results_mutex);

			if (entry.id != Constants::max_u32)
			{
				Threading::mutex_lock(system_state->results_mutex);
				system_state->pending_results[i].id = Constants::max_u32;
				Threading::mutex_unlock(system_state->results_mutex);

				system_state->pending_results_count--;
				entry.on_complete(entry.user_data);

				if (entry.user_data)
					Memory::free_memory(entry.user_data);	
			}

		}

		return true;

	}

	SHMAPI void submit(JobInfo info)
	{

#if MT_ENABLED
		uint32 thread_count = system_state->job_threads.capacity;
		RingQueue<JobInfo>* queue = &system_state->normal_prio_queue;
		Threading::Mutex queue_mutex = system_state->normal_prio_queue_mutex;

		if (info.priority == JobPriority::High)
		{
			queue = &system_state->high_prio_queue;
			queue_mutex = system_state->high_prio_queue_mutex;

			for (uint32 i = 0; i < thread_count; ++i) {
				JobThread* thread = &system_state->job_threads[i];
				if (system_state->job_threads[i].type_flags & info.type_flags) {
					bool8 found = false;

					Threading::mutex_lock(thread->info_mutex);
					if (!system_state->job_threads[i].info.entry_point) {
						SHMTRACEV("Job immediately submitted on thread %u", system_state->job_threads[i].index);
						system_state->job_threads[i].info = info;
						found = true;
					}
					Threading::mutex_unlock(thread->info_mutex);

					if (found) {
						return;
					}
				}
			}
		}

		if (info.priority == JobPriority::Low) {
			queue = &system_state->low_prio_queue;
			queue_mutex = system_state->low_prio_queue_mutex;
		}

		Threading::mutex_lock(queue_mutex);
		queue->enqueue(info);
		Threading::mutex_unlock(queue_mutex);
		//SHMTRACE("Job queued.");
#else
		bool8 result = info.entry_point(0, info.user_data);

		if (result && info.on_success)
			info.on_success(info.user_data);
		else if (!result && info.on_failure)
			info.on_failure(info.user_data);

		Memory::free_memory(info.user_data);
#endif

	}

	JobInfo job_create(FP_job_start entry_point, FP_job_on_complete on_success, FP_job_on_complete on_failure, uint32 user_data_size, JobTypeFlags::Value type_flags, JobPriority priority)
	{
		JobInfo info = {};
		info.entry_point = entry_point;
		info.on_success = on_success;
		info.on_failure = on_failure;
		info.type_flags = type_flags;
		info.priority = priority;

		info.user_data_size = user_data_size;
		if (user_data_size)
			info.user_data = Memory::allocate(user_data_size, AllocationTag::Job);
		else
			info.user_data = 0;

		return info;
	}

	static void store_result(FP_job_on_complete callback, uint32 user_data_size, void* user_data)
	{
		JobResultEntry entry;
		entry.id = Constants::max_u32;
		entry.user_data_size = user_data_size;
		entry.on_complete = callback;
		entry.user_data = user_data;

		Threading::mutex_lock(system_state->results_mutex);
		for (uint32 i = 0; i < SystemConfig::max_job_results_count; i++)
		{
			if (system_state->pending_results[i].id == Constants::max_u32)
			{
				system_state->pending_results[i] = entry;
				system_state->pending_results[i].id = i;
				system_state->pending_results_count++;
				break;
			}
		}
		Threading::mutex_unlock(system_state->results_mutex);
	}

	static uint32 job_thread_run(void* params)
	{
		uint32 thread_index = *(uint32*)params;
		JobThread* thread = &system_state->job_threads[thread_index];
		uint32 thread_id = thread->thread.thread_id;

		if (!Threading::mutex_create(&thread->info_mutex))
		{
			SHMERROR("Failed to create job thread mutex mutex!");
			return 0;
		}

		while (true)
		{
			if (!system_state || !system_state->is_running)
				break;

			Threading::mutex_lock(thread->info_mutex);
			JobInfo info = thread->info;
			Threading::mutex_unlock(thread->info_mutex);

			if (info.entry_point)
			{
				bool8 result = info.entry_point(thread_index, info.user_data);

				if (result && info.on_success)
					store_result(info.on_success, info.user_data_size, info.user_data);
				else if (!result && info.on_failure)
					store_result(info.on_failure, info.user_data_size, info.user_data);
				else if (info.user_data)
					Memory::free_memory(info.user_data);

				// Lock and reset the thread's info object
				Threading::mutex_lock(thread->info_mutex);
				thread->info = {};
				Threading::mutex_unlock(thread->info_mutex);
			}

			if (system_state->is_running)
				Threading::thread_sleep(&thread->thread, 10);
		}

		Threading::mutex_destroy(&thread->info_mutex);
		return 1;
	}

	static void process_queue(RingQueue<JobInfo>& queue, Threading::Mutex queue_mutex)
	{

		uint32 thread_count = system_state->job_threads.capacity;

		while (queue.count > 0)
		{
			JobInfo* info = queue.peek();

			bool8 thread_found = false;
			for (uint32 i = 0; i < thread_count; i++)
			{
				JobThread* thread = &system_state->job_threads[i];
				if (!(thread->type_flags & info->type_flags))
					continue;

				Threading::mutex_lock(thread->info_mutex);
				if (!thread->info.entry_point)
				{
					Threading::mutex_lock(queue_mutex);
					thread->info = *queue.dequeue();
					Threading::mutex_unlock(queue_mutex);
					//SHMTRACEV("Assigning job to thread: %u", thread->index);
					thread_found = true;
				}
				Threading::mutex_unlock(thread->info_mutex);

				if (thread_found)
					break;
			}

			if (!thread_found)
				break;

		}

	}

}