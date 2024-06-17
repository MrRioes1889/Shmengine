#include "VulkanMaterialShader.hpp"

#include "utility/Math.hpp"
#include "core/Logging.hpp"

#include "../VulkanPipeline.hpp"
#include "../VulkanShaderUtils.hpp"
#include "../VulkanBuffer.hpp"

#include "systems/TextureSystem.hpp"

namespace Renderer::Vulkan
{

    bool32 material_shader_create(VulkanContext* context, VulkanMaterialShader* out_shader)
    {

        const char stage_type_strs[VulkanMaterialShader::shader_stage_count][5] = { "vert", "frag" };
        VkShaderStageFlagBits stage_types[VulkanMaterialShader::shader_stage_count] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };

        for (uint32 i = 0; i < VulkanMaterialShader::shader_stage_count; i++)
        {
            if (!create_shader_module(context, VulkanMaterialShader::builtin_shader_name, stage_type_strs[i], stage_types[i], i, out_shader->stages))
            {
                SHMERRORV("Unable to create %s shader module for '%s'.", stage_type_strs[i], VulkanMaterialShader::builtin_shader_name);
                return false;
            }
        }

        // Global Desciptors.
        VkDescriptorSetLayoutBinding global_ubo_layout_binding;
        global_ubo_layout_binding.binding = 0;
        global_ubo_layout_binding.descriptorCount = 1;
        global_ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        global_ubo_layout_binding.pImmutableSamplers = 0;
        global_ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo global_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        global_layout_info.bindingCount = 1;
        global_layout_info.pBindings = &global_ubo_layout_binding;
        VK_CHECK(vkCreateDescriptorSetLayout(context->device.logical_device, &global_layout_info, context->allocator_callbacks, &out_shader->global_descriptor_set_layout));

        VkDescriptorPoolSize global_pool_size;
        global_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        global_pool_size.descriptorCount = context->swapchain.images.count;

        VkDescriptorPoolCreateInfo global_pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        global_pool_info.poolSizeCount = 1;
        global_pool_info.pPoolSizes = &global_pool_size;
        global_pool_info.maxSets = context->swapchain.images.count;
        VK_CHECK(vkCreateDescriptorPool(context->device.logical_device, &global_pool_info, context->allocator_callbacks, &out_shader->global_descriptor_pool));

        out_shader->sampler_uses[0] = TextureUse::MAP_DIFFUSE;

        // Local/Object Descriptors
        VkDescriptorType descriptor_types[VulkanMaterialShader::descriptor_count] = {
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        };

        VkDescriptorSetLayoutBinding bindings[VulkanMaterialShader::descriptor_count] = {};
        for (uint32 i = 0; i < VulkanMaterialShader::descriptor_count; i++)
        {
            bindings[i].binding = i;
            bindings[i].descriptorCount = 1;
            bindings[i].descriptorType = descriptor_types[i];
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layout_info.bindingCount = VulkanMaterialShader::descriptor_count;
        layout_info.pBindings = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(context->device.logical_device, &layout_info, context->allocator_callbacks, &out_shader->object_descriptor_set_layout));

        const uint32 object_pool_size_count = 2;
        VkDescriptorPoolSize object_pool_sizes[object_pool_size_count];
        object_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        object_pool_sizes[0].descriptorCount = VulkanConfig::max_material_count;

        object_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        object_pool_sizes[1].descriptorCount = VulkanMaterialShader::sampler_count * VulkanConfig::max_material_count;

        VkDescriptorPoolCreateInfo object_pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        object_pool_info.poolSizeCount = object_pool_size_count;
        object_pool_info.pPoolSizes = object_pool_sizes;
        object_pool_info.maxSets = VulkanConfig::max_material_count;
        object_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        VK_CHECK(vkCreateDescriptorPool(context->device.logical_device, &object_pool_info, context->allocator_callbacks, &out_shader->object_descriptor_pool));

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
        uint32 attribute_offset = 0;
        VkVertexInputAttributeDescription attribute_descriptions[VulkanMaterialShader::attribute_count];
        // Position
        VkFormat formats[VulkanMaterialShader::attribute_count] = {
            VK_FORMAT_R32G32B32_SFLOAT,
            VK_FORMAT_R32G32_SFLOAT
        };
        uint32 sizes[VulkanMaterialShader::attribute_count] = {
            sizeof(Math::Vec3f),
            sizeof(Math::Vec2f)         // Size of texture coordinates
        };
        for (uint32 i = 0; i < VulkanMaterialShader::attribute_count; ++i) {
            attribute_descriptions[i].binding = 0;   // binding index - should match binding desc
            attribute_descriptions[i].location = i;  // attrib location
            attribute_descriptions[i].format = formats[i];
            attribute_descriptions[i].offset = attribute_offset;
            attribute_offset += sizes[i];
        }

        // Descriptor set layouts
        const uint32 descr_set_layout_count = 2;
        VkDescriptorSetLayout descr_set_layouts[descr_set_layout_count] = { out_shader->global_descriptor_set_layout, out_shader->object_descriptor_set_layout };

        // Stages
        VkPipelineShaderStageCreateInfo stage_create_infos[VulkanMaterialShader::shader_stage_count] = {};
        for (uint32 i = 0; i < VulkanMaterialShader::shader_stage_count; ++i) {
            stage_create_infos[i] = out_shader->stages[i].shader_stage_create_info;
        }

        if (!pipeline_create(
            context,
            &context->world_renderpass,
            sizeof(Vertex3D),
            VulkanMaterialShader::attribute_count,
            attribute_descriptions,
            descr_set_layout_count,
            descr_set_layouts,
            VulkanMaterialShader::shader_stage_count,
            stage_create_infos,
            viewport,
            scissor,
            false,
            true,
            &out_shader->pipeline)) {

            SHMERROR("Failed to load graphics pipeline for object shader.");
            return false;
        }

        // Creating global layout buffer
        uint32 device_local_bits = context->device.supports_device_local_host_visible ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
        if (!buffer_create(
            context,
            sizeof(VulkanMaterialShader::GlobalUBO) * 3,
            (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
            device_local_bits | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true,
            &out_shader->global_uniform_buffer))
        {
            SHMERROR("Vulkan buffer creation failed for object shader.");
            return false;
        }

        VkDescriptorSetLayout global_layouts[3] = { out_shader->global_descriptor_set_layout, out_shader->global_descriptor_set_layout, out_shader->global_descriptor_set_layout };

        VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        alloc_info.descriptorPool = out_shader->global_descriptor_pool;
        alloc_info.descriptorSetCount = 3;
        alloc_info.pSetLayouts = global_layouts;
        VK_CHECK(vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, out_shader->global_descriptor_sets));

        // Creating object layout buffer
        if (!buffer_create(
            context,
            sizeof(VulkanMaterialShader::InstanceUBO) * VulkanConfig::max_material_count,
            (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true,
            &out_shader->object_uniform_buffer))
        {
            SHMERROR("Vulkan buffer creation failed for material shader.");
            return false;
        }

        return true;

    }

    void material_shader_destroy(VulkanContext* context, VulkanMaterialShader* shader)
    {

        VkDevice& device = context->device.logical_device;

        buffer_destroy(context, &shader->object_uniform_buffer);
        buffer_destroy(context, &shader->global_uniform_buffer);

        pipeline_destroy(context, &shader->pipeline);

        vkDestroyDescriptorPool(device, shader->object_descriptor_pool, context->allocator_callbacks);
        vkDestroyDescriptorSetLayout(device, shader->object_descriptor_set_layout, context->allocator_callbacks);

        vkDestroyDescriptorPool(device, shader->global_descriptor_pool, context->allocator_callbacks);
        vkDestroyDescriptorSetLayout(device, shader->global_descriptor_set_layout, context->allocator_callbacks);

        for (uint32 i = 0; i < VulkanMaterialShader::shader_stage_count; i++)
        {
            shader->stages[i].shader_code_buffer.free_data();
            vkDestroyShaderModule(device, shader->stages[i].handle, context->allocator_callbacks);
            shader->stages[i].handle = 0;
        }

    }

    void material_shader_use(VulkanContext* context, VulkanMaterialShader* shader)
    {

        uint32 image_index = context->image_index;
        pipeline_bind(&context->graphics_command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, &shader->pipeline);

    }

    void material_shader_update_global_state(VulkanContext* context, VulkanMaterialShader* shader)
    {

        uint32 image_index = context->image_index;
        VkCommandBuffer& command_buffer = context->graphics_command_buffers[image_index].handle;
        VkDescriptorSet& global_descriptor = shader->global_descriptor_sets[image_index];

        // Configure the descriptors for the given index.
        uint32 range = sizeof(VulkanMaterialShader::GlobalUBO);
        uint64 offset = sizeof(VulkanMaterialShader::GlobalUBO) * image_index;

        // Copy data to buffer
        buffer_load_data(context, &shader->global_uniform_buffer, offset, range, 0, &shader->global_ubo);

        VkDescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = shader->global_uniform_buffer.handle;
        bufferInfo.offset = offset;
        bufferInfo.range = range;

        // Update descriptor sets.
        VkWriteDescriptorSet descriptor_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptor_write.dstSet = shader->global_descriptor_sets[image_index];
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(context->device.logical_device, 1, &descriptor_write, 0, 0);

        // Bind the global descriptor set to be updated.
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline.layout, 0, 1, &global_descriptor, 0, 0);

    }

    void material_shader_set_model(VulkanContext* context, VulkanMaterialShader* shader, const Math::Mat4& model)
    {
        VkCommandBuffer& command_buffer = context->graphics_command_buffers[context->image_index].handle;
        vkCmdPushConstants(command_buffer, shader->pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Math::Mat4), &model);
    }

    void material_shader_apply_material(VulkanContext* context, VulkanMaterialShader* shader, Material* material)
    {

        uint32 image_index = context->image_index;
        VkCommandBuffer& command_buffer = context->graphics_command_buffers[image_index].handle;

        VulkanMaterialShader::InstanceState& instance_state = shader->instance_states[material->internal_id];
        VkDescriptorSet object_descriptor_set = instance_state.descriptor_sets[image_index];

        VkWriteDescriptorSet descriptor_writes[VulkanMaterialShader::descriptor_count] = {};
        uint32 new_descriptor_count = 0;
        uint32 descriptor_index = 0;

        uint32 range = sizeof(VulkanMaterialShader::InstanceUBO);
        uint64 offset = sizeof(VulkanMaterialShader::InstanceUBO) * material->internal_id;
        VulkanMaterialShader::InstanceUBO instance_ubo;

        instance_ubo.diffuse_color = material->diffuse_color;

        buffer_load_data(context, &shader->object_uniform_buffer, offset, range, 0, &instance_ubo);

        uint32& global_ubo_generation = instance_state.descriptor_states[descriptor_index].generations[image_index];
        if (global_ubo_generation == INVALID_OBJECT_ID || global_ubo_generation != material->generation)
        {
            VkDescriptorBufferInfo buffer_info;
            buffer_info.buffer = shader->object_uniform_buffer.handle;
            buffer_info.offset = offset;
            buffer_info.range = range;

            VkWriteDescriptorSet descriptor = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            descriptor.dstSet = object_descriptor_set;
            descriptor.dstBinding = descriptor_index;
            descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor.descriptorCount = 1;
            descriptor.pBufferInfo = &buffer_info;

            descriptor_writes[new_descriptor_count] = descriptor;
            new_descriptor_count++;

            global_ubo_generation = material->generation;
        }
        descriptor_index++;

        const uint32 sampler_count = 1;
        VkDescriptorImageInfo image_infos[1];
        for (uint32 sampler_index = 0; sampler_index < sampler_count; sampler_index++)
        {
            Texture* t = 0;

            switch (shader->sampler_uses[sampler_index])
            {
            case TextureUse::MAP_DIFFUSE:
            {
                t = material->diffuse_map.texture;
                break;
            }
            default:
            {
                SHMFATAL("Unable to bind sampler to unknown use.");
                return;
            }
            }

            uint32& descriptor_generation = instance_state.descriptor_states[descriptor_index].generations[image_index];
            uint32& descriptor_id = instance_state.descriptor_states[descriptor_index].ids[image_index];

            if (t->generation == INVALID_OBJECT_ID)
            {
                t = TextureSystem::get_default_texture();
                descriptor_generation = INVALID_OBJECT_ID;
                descriptor_id = INVALID_OBJECT_ID;
            }

            if (t && (descriptor_id != t->id || descriptor_generation != t->generation || descriptor_generation == INVALID_OBJECT_ID))
            {
                VulkanTextureData* internal_data = (VulkanTextureData*)t->buffer.data;

                image_infos[sampler_index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                image_infos[sampler_index].imageView = internal_data->image.view;
                image_infos[sampler_index].sampler = internal_data->sampler;

                VkWriteDescriptorSet descriptor = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                descriptor.dstSet = object_descriptor_set;
                descriptor.dstBinding = descriptor_index;
                descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptor.descriptorCount = 1;
                descriptor.pImageInfo = &image_infos[sampler_index];

                descriptor_writes[new_descriptor_count] = descriptor;
                new_descriptor_count++;

                if (t->generation != INVALID_OBJECT_ID)
                {
                    descriptor_generation = t->generation;
                    descriptor_id = t->id;
                }

                descriptor_index++;
            }
        }

        if (new_descriptor_count > 0)
        {
            vkUpdateDescriptorSets(context->device.logical_device, new_descriptor_count, descriptor_writes, 0, 0);
        }

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline.layout, 1, 1, &object_descriptor_set, 0, 0);

    }

    bool32 material_shader_acquire_resources(VulkanContext* context, VulkanMaterialShader* shader, Material* material)
    {
        // TODO: free list
        material->internal_id = shader->object_uniform_buffer_index;
        shader->object_uniform_buffer_index++;

        VulkanMaterialShader::InstanceState& instance_state = shader->instance_states[material->internal_id];
        for (uint32 i = 0; i < VulkanMaterialShader::descriptor_count; ++i) {
            for (uint32 j = 0; j < 3; ++j) {
                instance_state.descriptor_states[i].generations[j] = INVALID_OBJECT_ID;
                instance_state.descriptor_states[i].ids[j] = INVALID_OBJECT_ID;
            }
        }

        // Allocate descriptor sets.
        VkDescriptorSetLayout layouts[3] = {
            shader->object_descriptor_set_layout,
            shader->object_descriptor_set_layout,
            shader->object_descriptor_set_layout };

        VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        alloc_info.descriptorPool = shader->object_descriptor_pool;
        alloc_info.descriptorSetCount = 3;  // one per frame
        alloc_info.pSetLayouts = layouts;
        VkResult result = vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, instance_state.descriptor_sets);
        if (result != VK_SUCCESS) {
            SHMERROR("Error allocating descriptor sets in shader!");
            return false;
        }

        return true;

    }

    void material_shader_release_resources(VulkanContext* context, VulkanMaterialShader* shader, Material* material)
    {

        vkDeviceWaitIdle(context->device.logical_device);

        VulkanMaterialShader::InstanceState& instance_state = shader->instance_states[material->internal_id];

        const uint32 descriptor_set_count = 3;
        // Release object descriptor sets.
        VkResult result = vkFreeDescriptorSets(context->device.logical_device, shader->object_descriptor_pool, descriptor_set_count, instance_state.descriptor_sets);
        if (result != VK_SUCCESS) {
            SHMERROR("Error freeing object shader descriptor sets!");
        }

        for (uint32 i = 0; i < VulkanMaterialShader::descriptor_count; ++i) {
            for (uint32 j = 0; j < 3; ++j) {
                instance_state.descriptor_states[i].generations[j] = INVALID_OBJECT_ID;
                instance_state.descriptor_states[i].ids[j] = INVALID_OBJECT_ID;
            }
        }

        material->internal_id = INVALID_OBJECT_ID;

    }

}
