#pragma once

#include "Defines.hpp"
#include "core/Subsystems.hpp"

struct FrameData;

namespace JobSystem
{

	typedef bool32 (*FP_job_start)(void*, void*);
	typedef void (*FP_job_on_complete)(void*);

	namespace JobTypeFlags
	{
		enum : uint8
		{
			General = 1 << 1,
			ResourceLoad = 1 << 2,
			GPUResource = 1 << 3,
		};
		typedef uint8 Value;
	}

	enum class JobPriority : uint8
	{
		Low,
		Normal,
		High
	};

	struct JobInfo
	{
		JobTypeFlags::Value type_flags;
		JobPriority priority;

		FP_job_start entry_point;

		FP_job_on_complete on_success;
		FP_job_on_complete on_failure;

		uint32 params_size;
		uint32 results_size;

		void* params;
		void* results;
	};

	struct SystemConfig
	{
		static const uint32 max_job_results_count = 512;
		uint32 job_thread_count;
		JobTypeFlags::Value* type_flags;
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	bool32 update(void* state, const FrameData* frame_data);

	SHMAPI void submit(JobInfo info);

	SHMAPI JobInfo job_create(FP_job_start entry_point, FP_job_on_complete on_success, FP_job_on_complete on_failure, uint32 params_size, uint32 results_size, JobTypeFlags::Value type = JobTypeFlags::General, JobPriority priority = JobPriority::Normal);

}
