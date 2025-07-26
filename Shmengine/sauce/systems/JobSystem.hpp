#pragma once

#include "Defines.hpp"
#include "core/Subsystems.hpp"

struct FrameData;

namespace JobSystem
{

	typedef bool8 (*FP_job_start)(uint32 thread_index, void* user_data);
	typedef void (*FP_job_on_complete)(void* results);

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

		uint32 user_data_size;
		void* user_data;
	};

	struct SystemConfig
	{
		static const uint32 max_job_results_count = 512;
		uint32 job_thread_count;
		JobTypeFlags::Value* type_flags;
	};

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	bool8 update(void* state, const FrameData* frame_data);

	SHMAPI void submit(JobInfo info);

	SHMAPI JobInfo job_create(FP_job_start entry_point, FP_job_on_complete on_success, FP_job_on_complete on_failure, uint32 user_data_size, JobTypeFlags::Value type = JobTypeFlags::General, JobPriority priority = JobPriority::Normal);

}
