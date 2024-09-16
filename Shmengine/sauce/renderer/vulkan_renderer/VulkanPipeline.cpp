#include "VulkanPipeline.hpp"
#include "VulkanUtils.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"

namespace Renderer::Vulkan
{

    bool32 pipeline_create(VulkanContext* context, const VulkanPipelineConfig* config, VulkanPipeline* out_pipeline)
    {

        VkPipelineViewportStateCreateInfo vp_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        vp_state_create_info.viewportCount = 1;
        vp_state_create_info.pViewports = &config->viewport;
        vp_state_create_info.scissorCount = 1;
        vp_state_create_info.pScissors = &config->scissor;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer_create_info = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer_create_info.depthClampEnable = VK_FALSE;
        rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;
        rasterizer_create_info.polygonMode = config->is_wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        rasterizer_create_info.lineWidth = 1.0f;
        switch (config->cull_mode)
        {
        case ShaderFaceCullMode::NONE:
        {
            rasterizer_create_info.cullMode = VK_CULL_MODE_NONE;
            break;
        }
        case ShaderFaceCullMode::FRONT:
        {
            rasterizer_create_info.cullMode = VK_CULL_MODE_FRONT_BIT;
            break;
        }
        case ShaderFaceCullMode::BACK:
        {
            rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
            break;
        }
        case ShaderFaceCullMode::BOTH:
        {
            rasterizer_create_info.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
            break;
        }
        }
        rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer_create_info.depthBiasEnable = VK_FALSE;
        rasterizer_create_info.depthBiasConstantFactor = 0.0f;
        rasterizer_create_info.depthBiasClamp = 0.0f;
        rasterizer_create_info.depthBiasSlopeFactor = 0.0f;

        // Multisampling.
        VkPipelineMultisampleStateCreateInfo multisampling_create_info = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampling_create_info.sampleShadingEnable = VK_FALSE;
        multisampling_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_create_info.minSampleShading = 1.0f;
        multisampling_create_info.pSampleMask = 0;
        multisampling_create_info.alphaToCoverageEnable = VK_FALSE;
        multisampling_create_info.alphaToOneEnable = VK_FALSE;

        // Depth and stencil testing.
        VkPipelineDepthStencilStateCreateInfo depth_stencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };      
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
        color_blend_attachment_state.blendEnable = VK_TRUE;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

        color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        color_blend_state_create_info.logicOpEnable = VK_FALSE;
        color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount = 1;
        color_blend_state_create_info.pAttachments = &color_blend_attachment_state;

        // Dynamic state
        const uint32 dynamic_state_count = 3;
        VkDynamicState dynamic_states[dynamic_state_count] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_LINE_WIDTH };

        VkPipelineDynamicStateCreateInfo dynamic_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamic_state_create_info.dynamicStateCount = dynamic_state_count;
        dynamic_state_create_info.pDynamicStates = dynamic_states;

        // Vertex input
        VkVertexInputBindingDescription binding_description;
        binding_description.binding = 0;  // Binding index
        binding_description.stride = config->vertex_stride;
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  // Move to next data entry for each vertex.

        // Attributes
        VkPipelineVertexInputStateCreateInfo vertex_input_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &binding_description;
        vertex_input_info.vertexAttributeDescriptionCount = config->attribute_count;
        vertex_input_info.pVertexAttributeDescriptions = config->attribute_descriptions;

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipeline_layout_create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

        //Push Constants
        VkPushConstantRange ranges[RendererConfig::shader_max_push_const_ranges];
        if (config->push_constant_range_count > 0) {
            if (config->push_constant_range_count > RendererConfig::shader_max_push_const_ranges) {
                SHMERRORV("vulkan_graphics_pipeline_create: cannot have more than 32 push constant ranges. Passed count: %i", config->push_constant_range_count);
                return false;
            }
          
            for (uint32 i = 0; i < config->push_constant_range_count; ++i) {
                ranges[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                ranges[i].offset = (uint32)config->push_constant_ranges[i].offset;
                ranges[i].size = (uint32)config->push_constant_ranges[i].size;
            }
            pipeline_layout_create_info.pushConstantRangeCount = config->push_constant_range_count;
            pipeline_layout_create_info.pPushConstantRanges = ranges;
        }
        else {
            pipeline_layout_create_info.pushConstantRangeCount = 0;
            pipeline_layout_create_info.pPushConstantRanges = 0;
        }

        // Descriptor set layouts
        pipeline_layout_create_info.setLayoutCount = config->descriptor_set_layout_count;
        pipeline_layout_create_info.pSetLayouts = config->descriptor_set_layouts;

        // Create the pipeline layout.
        VK_CHECK(vkCreatePipelineLayout(
            context->device.logical_device,
            &pipeline_layout_create_info,
            context->allocator_callbacks,
            &out_pipeline->layout));

        // Pipeline create
        VkGraphicsPipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipeline_create_info.stageCount = config->stage_count;
        pipeline_create_info.pStages = config->stages;
        pipeline_create_info.pVertexInputState = &vertex_input_info;
        pipeline_create_info.pInputAssemblyState = &input_assembly;

        pipeline_create_info.pViewportState = &vp_state_create_info;
        pipeline_create_info.pRasterizationState = &rasterizer_create_info;
        pipeline_create_info.pMultisampleState = &multisampling_create_info;
        pipeline_create_info.pDepthStencilState = (config->shader_flags & ShaderFlags::DEPTH_TEST) ? &depth_stencil : 0;
        pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
        pipeline_create_info.pDynamicState = &dynamic_state_create_info;
        pipeline_create_info.pTessellationState = 0;

        pipeline_create_info.layout = out_pipeline->layout;

        pipeline_create_info.renderPass = config->renderpass->handle;
        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_create_info.basePipelineIndex = -1;

        VkResult result = vkCreateGraphicsPipelines(
            context->device.logical_device,
            VK_NULL_HANDLE,
            1,
            &pipeline_create_info,
            context->allocator_callbacks,
            &out_pipeline->handle);

        if (vulkan_result_is_success(result)) {
            SHMDEBUG("Graphics pipeline created!");
            return true;
        }

        SHMERRORV("vkCreateGraphicsPipelines failed with %s.", vulkan_result_string(result, true));
        return false;

    }

    void pipeline_destroy(VulkanContext* context, VulkanPipeline* pipeline)
    {
        // Destroy pipeline
        if (pipeline->handle) {
            vkDestroyPipeline(context->device.logical_device, pipeline->handle, context->allocator_callbacks);
            pipeline->handle = 0;
        }

        // Destroy layout
        if (pipeline->layout) {
            vkDestroyPipelineLayout(context->device.logical_device, pipeline->layout, context->allocator_callbacks);
            pipeline->layout = 0;
        }
    }

    void pipeline_bind(VulkanCommandBuffer* command_buffer, VkPipelineBindPoint bind_point, VulkanPipeline* pipeline)
    {
        vkCmdBindPipeline(command_buffer->handle, bind_point, pipeline->handle);
    }

}

