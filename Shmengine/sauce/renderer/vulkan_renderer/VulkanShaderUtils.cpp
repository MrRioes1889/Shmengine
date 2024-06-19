#include "VulkanShaderUtils.hpp"

#include "utility/CString.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "systems/ResourceSystem.hpp"

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
	CString::print_s(file_name, MAX_FILEPATH_LENGTH, (char*)"shaders/%s.%s.spv", name, type_str);

	Resource res;
	if (!ResourceSystem::load(file_name, ResourceType::GENERIC, &res))
	{
		SHMERRORV("Unable to load resources for shader module: %s.", file_name);
		return false;
	}

	VkShaderModuleCreateInfo& module_create_info = shader_stages[stage_index].module_create_info;
	module_create_info = {};
	module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	module_create_info.codeSize = res.data_size;
	module_create_info.pCode = (uint32*)res.data;

	VK_CHECK(vkCreateShaderModule(context->device.logical_device, &module_create_info, context->allocator_callbacks, &shader_stages[stage_index].handle));

	ResourceSystem::unload(&res);

	VkPipelineShaderStageCreateInfo& stage_create_info = shader_stages[stage_index].shader_stage_create_info;
	stage_create_info = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	stage_create_info.stage = stage_flags;
	stage_create_info.module = shader_stages[stage_index].handle;
	stage_create_info.pName = "main";

	return true;

}