#include "JobSystem.hpp"

#include "core/Thread.hpp"
#include "core/Mutex.hpp"
#include "core/Logging.hpp"
#include "core/FrameData.hpp"
#include "containers/RingQueue.hpp"

#define MT_ENABLED 1

namespace JobSystem
{

	struct JobThread
	{
		uint32 index;
		uint32 type_mask;
		Threading::Thread thread;
		JobInfo info;
		Threading::Mutex info_mutex; 
	};

	struct JobResultEntry
	{
		uint32 id;
		uint32 params_size;

		void* params;
		FP_job_on_complete on_complete;
	};

	struct SystemState
	{
		SystemConfig config;
		bool32 is_running;

		JobThread job_threads[16];

		RingQueue<JobInfo> low_prio_queue;
		RingQueue<JobInfo> normal_prio_queue;
		RingQueue<JobInfo> high_prio_queue;

		Threading::Mutex low_prio_queue_mutex;
		Threading::Mutex normal_prio_queue_mutex;
		Threading::Mutex high_prio_queue_mutex;

		JobResultEntry pending_results[SystemConfig::max_job_results_count];
		Threading::Mutex results_mutex;
	};

	static SystemState* system_state = 0;

	static void store_result(FP_job_on_complete callback, uint32 param_size, void* params);
	static uint32 job_thread_run(void* params);
	static void process_queue(RingQueue<JobInfo>& queue, Threading::Mutex* queue_mutex);

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{

		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->config = *sys_config;
		system_state->is_running = true;

		system_state->low_prio_queue.init(1024, 0);
		system_state->normal_prio_queue.init(1024, 0);
		system_state->high_prio_queue.init(1024, 0);

		for (uint32 i = 0; i < SystemConfig::max_job_results_count; i++)
			system_state->pending_results[i].id = INVALID_ID;

		SHMDEBUGV("Main thread id is: %u", Threading::get_thread_id());
		SHMDEBUGV("Spawning %u job threads.", system_state->config.job_thread_count);

		for (uint32 i = 0; i < system_state->config.job_thread_count; i++)
		{
			system_state->job_threads[i].index = i;
			system_state->job_threads[i].type_mask = sys_config->type_masks[i];
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

		for (uint32 i = 0; i < system_state->config.job_thread_count; i++)
		{
			Threading::thread_destroy(&system_state->job_threads[i].thread);
		}
			
		Threading::mutex_destroy(&system_state->results_mutex);
		Threading::mutex_destroy(&system_state->low_prio_queue_mutex);
		Threading::mutex_destroy(&system_state->normal_prio_queue_mutex);
		Threading::mutex_destroy(&system_state->high_prio_queue_mutex);

		system_state = 0;

	}


	bool32 update(void* state, const FrameData* frame_data)
	{

		if (!system_state->is_running)
			return true;

		process_queue(system_state->low_prio_queue, &system_state->low_prio_queue_mutex);
		process_queue(system_state->normal_prio_queue, &system_state->normal_prio_queue_mutex);
		process_queue(system_state->high_prio_queue, &system_state->high_prio_queue_mutex);

		for (uint32 i = 0; i < SystemConfig::max_job_results_count; i++)
		{

			Threading::mutex_lock(&system_state->results_mutex);
			JobResultEntry entry = system_state->pending_results[i];
			Threading::mutex_unlock(&system_state->results_mutex);

			if (entry.id != INVALID_ID)
			{
				entry.on_complete(entry.params);

				if (entry.params)
					Memory::free_memory(entry.params);

				Threading::mutex_lock(&system_state->results_mutex);
				system_state->pending_results[i].id = INVALID_ID;
				Threading::mutex_unlock(&system_state->results_mutex);
			}

		}

		return true;

	}

	SHMAPI void submit(JobInfo info)
	{

#if MT_ENABLED
		uint32 thread_count = system_state->config.job_thread_count;
		RingQueue<JobInfo>* queue = &system_state->normal_prio_queue;
		Threading::Mutex* queue_mutex = &system_state->normal_prio_queue_mutex;

		if (info.priority == JobPriority::HIGH)
		{
			queue = &system_state->high_prio_queue;
			queue_mutex = &system_state->high_prio_queue_mutex;

			for (uint32 i = 0; i < thread_count; ++i) {
				JobThread* thread = &system_state->job_threads[i];
				if (system_state->job_threads[i].type_mask & info.type) {
					bool32 found = false;

					Threading::mutex_lock(&thread->info_mutex);
					if (!system_state->job_threads[i].info.entry_point) {
						SHMTRACEV("Job immediately submitted on thread %u", system_state->job_threads[i].index);
						system_state->job_threads[i].info = info;
						found = true;
					}
					Threading::mutex_unlock(&thread->info_mutex);

					if (found) {
						return;
					}
				}
			}
		}

		if (info.priority == JobPriority::LOW) {
			queue = &system_state->low_prio_queue;
			queue_mutex = &system_state->low_prio_queue_mutex;
		}

		Threading::mutex_lock(queue_mutex);
		queue->enqueue(info);
		Threading::mutex_unlock(queue_mutex);
		//SHMTRACE("Job queued.");
#else
		bool32 result = info.entry_point(info.params, info.results);
		if (result)
			info.on_success(info.results);
		else
			info.on_failure(info.results);

		if (info.params)
		{
			Memory::free_memory(info.params);
			info.params = 0;
		}
		if (info.results)
		{
			Memory::free_memory(info.results);
			info.results = 0;
		}

#endif

	}

	JobInfo job_create(FP_job_start entry_point, FP_job_on_complete on_success, FP_job_on_complete on_failure, uint32 params_size, uint32 results_size, JobType::Value type, JobPriority priority)
	{

		JobInfo info = {};
		info.entry_point = entry_point;
		info.on_success = on_success;
		info.on_failure = on_failure;
		info.type = type;
		info.priority = priority;

		info.params_size = params_size;
		if (params_size)
		{
			info.params = Memory::allocate(params_size, AllocationTag::JOB);
		}		
		else
		{
			info.params = 0;
		}			

		info.results_size = results_size;
		if (results_size)
			info.results = Memory::allocate(results_size, AllocationTag::JOB);
		else
			info.results = 0;

		return info;

	}

	static void store_result(FP_job_on_complete callback, uint32 param_size, void* params)
	{

		JobResultEntry entry;
		entry.id = INVALID_ID;
		entry.params_size = param_size;
		entry.on_complete = callback;
		entry.params = 0;
		if (entry.params_size)
		{
			entry.params = Memory::allocate(entry.params_size, AllocationTag::JOB);
			Memory::copy_memory(params, entry.params, entry.params_size);
		}

		Threading::mutex_lock(&system_state->results_mutex);
		for (uint32 i = 0; i < SystemConfig::max_job_results_count; i++)
		{
			if (system_state->pending_results[i].id == INVALID_ID)
			{
				system_state->pending_results[i] = entry;
				system_state->pending_results[i].id = i;
				break;
			}
		}
		Threading::mutex_unlock(&system_state->results_mutex);

	}

	static uint32 job_thread_run(void* params)
	{

		uint32 index = *(uint32*)params;
		JobThread* thread = &system_state->job_threads[index];
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

			Threading::mutex_lock(&thread->info_mutex);
			JobInfo info = thread->info;
			Threading::mutex_unlock(&thread->info_mutex);

			if (info.entry_point)
			{
				bool32 result = info.entry_point(info.params, info.results);

				if (result && info.on_success)
					store_result(info.on_success, info.results_size, info.results);
				else if (!result && info.on_failure)
					store_result(info.on_failure, info.results_size, info.results);

				// Clear the param data and result data.
				if (info.params)
				{
					Memory::free_memory(info.params);
					info.params = 0;
				}				
				if (info.results)
				{
					Memory::free_memory(info.results);
					info.results = 0;
				}
					

				// Lock and reset the thread's info object
				Threading::mutex_lock(&thread->info_mutex);
				thread->info = {};
				Threading::mutex_unlock(&thread->info_mutex);
			}

			if (system_state->is_running)
				Threading::thread_sleep(&thread->thread, 10);
		}

		Threading::mutex_destroy(&thread->info_mutex);
		return 1;

	}

	static void process_queue(RingQueue<JobInfo>& queue, Threading::Mutex* queue_mutex)
	{

		uint32 thread_count = system_state->config.job_thread_count;

		while (queue.count > 0)
		{
			JobInfo* info = queue.peek();

			bool32 thread_found = false;
			for (uint32 i = 0; i < thread_count; i++)
			{
				JobThread* thread = &system_state->job_threads[i];
				if (!(thread->type_mask & info->type))
					continue;

				Threading::mutex_lock(&thread->info_mutex);
				if (!thread->info.entry_point)
				{
					Threading::mutex_lock(queue_mutex);
					thread->info = *queue.dequeue();
					Threading::mutex_unlock(queue_mutex);
					//SHMTRACEV("Assigning job to thread: %u", thread->index);
					thread_found = true;
				}
				Threading::mutex_unlock(&thread->info_mutex);

				if (thread_found)
					break;
			}

			if (!thread_found)
				break;

		}

	}

}