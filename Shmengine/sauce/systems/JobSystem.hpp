#pragma once

#include "Defines.hpp"

namespace JobSystem
{

	typedef bool32 (*fp_job_start)(void*, void*);

	typedef void (*fp_job_on_complete)(void*);

	namespace JobType
	{
		enum Value
		{
			GENERAL = 1 << 1,
			RESOURCE_LOAD = 1 << 2,
			GPU_RESOURCE = 1 << 3,
		};
	}

	enum class JobPriority
	{
		LOW,
		NORMAL,
		HIGH
	};

	struct JobInfo
	{
		JobType::Value type;
		JobPriority priority;

		fp_job_start entry_point;

		fp_job_on_complete on_success;
		fp_job_on_complete on_failure;

		uint32 params_size;
		uint32 results_size;

		void* params;
		void* results;
	};

	struct Config
	{
		static const uint32 max_job_results_count = 512;
		uint32 job_thread_count;
	};

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config, uint32 type_masks[]);
	void system_shutdown();

	void update();

	SHMAPI void submit(JobInfo info);

	SHMAPI JobInfo job_create(fp_job_start entry_point, fp_job_on_complete on_success, fp_job_on_complete on_failure, void* params, uint32 params_size, uint32 results_size, JobType::Value type = JobType::GENERAL, JobPriority priority = JobPriority::NORMAL);

}