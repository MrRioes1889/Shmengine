#include "VulkanShaderUtils.hpp"

#include "utility/String.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "platform/FileSystem.hpp"

bool32 create_shader_module(
	VulkanContext* context,
	const char* name,
	const char* type_str,
	VkShaderStageFlagBits stage_flags,
	uint32 stage_index,
	VulkanShaderStage* shader_stages
)
{

	char file_name[MAX_FILEPATH_LENGTH];
	String::print_s(file_name, MAX_FILEPATH_LENGTH, (char*)"D:/dev/Shmengine/bin/assets/shaders/%s.%s.spv", name, type_str);

	VkShaderModuleCreateInfo& module_create_info = shader_stages[stage_index].module_create_info;
	module_create_info = {};
	module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

	FileSystem::FileHandle file;
	if (!FileSystem::file_open(file_name, FILE_MODE_READ, true, &file))
	{
		SHMERRORV("Unable to open file for shader module: %s.", file_name);
		return false;
	}

	uint64 size = 0;
	uint8* file_buffer = 0;
	if (!FileSystem::read_all_bytes(&file, &file_buffer, &size))
	{
		SHMERRORV("Unable to read file for shader module: %s.", file_name);
		return false;
	}

	module_create_info.codeSize = size;
	module_create_info.pCode = (uint32*)file_buffer;

	FileSystem::file_close(&file);

	VK_CHECK(vkCreateShaderModule(context->device.logical_device, &module_create_info, context->allocator_callbacks, &shader_stages[stage_index].handle));

	VkPipelineShaderStageCreateInfo& stage_create_info = shader_stages[stage_index].shader_stage_create_info;
	stage_create_info = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	stage_create_info.stage = stage_flags;
	stage_create_info.module = shader_stages[stage_index].handle;
	stage_create_info.pName = "main";

	/*if (file_buffer)
	{
		Memory::free_memory(file_buffer, true, AllocationTag::RAW);
		file_buffer = 0;
	}*/

	return true;

}