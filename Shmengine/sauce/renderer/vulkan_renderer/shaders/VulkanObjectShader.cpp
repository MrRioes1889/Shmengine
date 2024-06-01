#include "VulkanObjectShader.hpp"

#include "../VulkanPipeline.hpp"
#include "../VulkanShaderUtils.hpp"
#include "core/Logging.hpp"

#define BUILTIN_SHADER_NAME_OBJECT "Builtin.ObjectShader"

bool32 vulkan_object_shader_create(VulkanContext* context, VulkanObjectShader* out_shader)
{
	const char stage_type_strs[OBJECT_SHADER_STAGE_COUNT][5] = { "vert", "frag" };
	VkShaderStageFlagBits stage_types[OBJECT_SHADER_STAGE_COUNT] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };

	for (uint32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; i++)
	{
		if (!create_shader_module(context, BUILTIN_SHADER_NAME_OBJECT, stage_type_strs[i], stage_types[i], i, out_shader->stages))
		{
			SHMERRORV("Unable to create %s shader module for '%s'.", stage_type_strs[i], BUILTIN_SHADER_NAME_OBJECT);
			return false;
		}
	}

	// TODO: Descriptors
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (float32)context->framebuffer_height;
    viewport.width = (float32)context->framebuffer_width;
    viewport.height = -(float32)context->framebuffer_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor;
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = context->framebuffer_width;
    scissor.extent.height = context->framebuffer_height;

    // Attributes
    uint32 offset = 0;
    const int32 attribute_count = 1;
    VkVertexInputAttributeDescription attribute_descriptions[attribute_count];
    // Position
    VkFormat formats[attribute_count] = {
        VK_FORMAT_R32G32B32_SFLOAT
    };
    uint32 sizes[attribute_count] = {
        sizeof(Math::Vec3f)
    };
    for (uint32 i = 0; i < attribute_count; ++i) {
        attribute_descriptions[i].binding = 0;   // binding index - should match binding desc
        attribute_descriptions[i].location = i;  // attrib location
        attribute_descriptions[i].format = formats[i];
        attribute_descriptions[i].offset = offset;
        offset += sizes[i];
    }

    // TODO: Desciptor set layouts.

    // Stages
    // NOTE: Should match the number of shader->stages.
    VkPipelineShaderStageCreateInfo stage_create_infos[OBJECT_SHADER_STAGE_COUNT] = {};
    for (uint32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i) {
        stage_create_infos[i] = out_shader->stages[i].shader_stage_create_info;
    }

    if (!vulkan_pipeline_create(
        context,
        &context->main_renderpass,
        attribute_count,
        attribute_descriptions,
        0,
        0,
        OBJECT_SHADER_STAGE_COUNT,
        stage_create_infos,
        viewport,
        scissor,
        false,
        &out_shader->pipeline)) {

        SHMERROR("Failed to load graphics pipeline for object shader.");
        return false;
    }

	return true;

}

void vulkan_object_shader_destroy(VulkanContext* context, VulkanObjectShader* shader)
{

    vulkan_pipeline_destroy(context, &shader->pipeline);

	for (uint32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; i++)
	{
		vkDestroyShaderModule(context->device.logical_device, shader->stages[i].handle, context->allocator_callbacks);
		shader->stages[i].handle = 0;
	}

}

void vulkan_object_shader_use(VulkanContext* context, VulkanObjectShader* shader)
{

    uint32 image_index = context->image_index;
    vulkan_pipeline_bind(&context->graphics_command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, &shader->pipeline);

}
