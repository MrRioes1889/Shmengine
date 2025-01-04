#pragma once

#include <Defines.hpp>
#include <core/Assert.hpp>
#include <containers/Sarray.hpp>
#include <containers/Buffer.hpp>
#include <utility/Math.hpp>
#include <memory/DynamicAllocator.hpp>
#include <containers/Hashtable.hpp>
		 
#include <renderer/RendererTypes.hpp>

#include <vulkan/vulkan.h>

#define VK_CHECK(x)						\
	{									\
		SHMASSERT((x) == VK_SUCCESS);		\
	}	

namespace Renderer::Vulkan
{

	struct VulkanBuffer
	{
		VkBuffer handle;
		VkDeviceMemory memory;
		void* mapped_memory;
		bool32 is_locked;
		VkBufferUsageFlags usage;
		int32 memory_index;
		uint32 memory_property_flags;
		VkMemoryRequirements memory_requirements;
	};

	struct VulkanSwapchainSupportInfo
	{
		VkSurfaceCapabilitiesKHR capabilities;
		VkSurfaceFormatKHR* formats;
		VkPresentModeKHR* present_modes;
		uint32 format_count;
		uint32 present_mode_count;
	};

	struct VulkanDevice
	{
		VkPhysicalDevice physical_device;
		VkDevice logical_device;
		VulkanSwapchainSupportInfo swapchain_support;

		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;
		VkPhysicalDeviceMemoryProperties memory;

		VkCommandPool graphics_command_pool;

		VkQueue graphics_queue;
		VkQueue present_queue;
		VkQueue transfer_queue;
		int32 graphics_queue_index;
		int32 present_queue_index;
		int32 transfer_queue_index;

		VkFormat depth_format;
		uint8 depth_channel_count;

		bool8 supports_device_local_host_visible;
	};

	struct VulkanImage
	{
		VkImage handle;
		VkDeviceMemory memory;
		VkImageView view;
		VkImageLayout layout;
		uint32 width;
		uint32 height;
		VkMemoryRequirements memory_requirements;
		VkMemoryPropertyFlags memory_flags;
	};

	enum class VulkanRenderpassState
	{
		READY,
		RECORDING,
		IN_RENDER_PASS,
		RECORDING_ENDED,
		SUBMITTED,
		NOT_ALLOCATED
	};

	struct VulkanRenderpass
	{
		VkRenderPass handle;
		float32 depth;
		uint32 stencil;	
		uint32 clear_flags;
		VulkanRenderpassState state;
	};

	struct VulkanSwapchain
	{
		RenderTarget render_targets[RendererConfig::framebuffer_count];

		VkSurfaceFormatKHR image_format;
		VkSwapchainKHR handle;
		Sarray<Texture> render_textures;
		Sarray<Texture> depth_textures;

		uint32 max_frames_in_flight;
	};

	enum class VulkanCommandBufferState
	{
		READY,
		RECORDING,
		IN_RENDER_PASS,
		RECORDING_ENDED,
		SUBMITTED,
		NOT_ALLOCATED
	};

	struct VulkanCommandBuffer
	{
		VkCommandBuffer handle;
		VulkanCommandBufferState state;
	};

	struct VulkanShaderStageConfig
	{
		static const uint32 max_filename_length = 255;
		VkShaderStageFlagBits stage;
		char filename[max_filename_length];
	};

	struct VulkanDescriptorSetConfig
	{
		uint8 sampler_binding_index;
		uint8 binding_count;
		VkDescriptorSetLayoutBinding bindings[RendererConfig::shader_max_bindings];
	};

	struct VulkanShaderStage
	{
		VkShaderModuleCreateInfo module_create_info;
		VkShaderModule handle;
		VkPipelineShaderStageCreateInfo shader_stage_create_info;
	};

	struct VulkanShaderConfig
	{
		uint32 stage_count;
		uint16 max_descriptor_set_count;
		uint16 descriptor_set_count;

		VulkanShaderStageConfig stages[RendererConfig::shader_max_stages];

		VkDescriptorPoolSize pool_sizes[2];

		VulkanDescriptorSetConfig descriptor_sets[2];

		VkVertexInputAttributeDescription attributes[RendererConfig::shader_max_attributes];

		ShaderFaceCullMode cull_mode;
	};

	struct VulkanDescriptorState
	{
		uint8 generations[RendererConfig::framebuffer_count];
		uint32 ids[RendererConfig::framebuffer_count];
	};

	struct VulkanShaderDescriptorSetState
	{
		VkDescriptorSet descriptor_sets[RendererConfig::framebuffer_count];
		VulkanDescriptorState descriptor_states[RendererConfig::shader_max_bindings];
	};

	struct VulkanShaderInstanceState
	{
		uint32 id;
		// TODO: Think about whether the offset serves a purpose
		uint64 offset;

		VulkanShaderDescriptorSetState descriptor_set_state;
		Sarray<TextureMap*> instance_texture_maps;

	};

	struct VulkanPipelineConfig
	{

		uint32 vertex_stride;
		uint32 attribute_count;
		uint32 descriptor_set_layout_count;
		uint32 stage_count;
		uint32 push_constant_range_count;

		VulkanRenderpass* renderpass;	
		VkVertexInputAttributeDescription* attribute_descriptions;	
		VkDescriptorSetLayout* descriptor_set_layouts;
		VkPipelineShaderStageCreateInfo* stages;
		Range* push_constant_ranges;

		VkViewport viewport;
		VkRect2D scissor;

		Renderer::ShaderFaceCullMode cull_mode;
		bool32 is_wireframe;
		uint32 shader_flags;	
	};

	struct VulkanPipeline
	{
		VkPipeline handle;
		VkPipelineLayout layout;
	};

	struct VulkanShader
	{

		uint32 id;
		uint32 instance_count;

		uint8 global_uniform_count;
		uint8 global_uniform_sampler_count;
		uint8 instance_uniform_count;
		uint8 instance_uniform_sampler_count;
		uint8 local_uniform_count;

		VulkanShaderConfig config;

		VulkanRenderpass* renderpass;

		VulkanShaderStage stages[RendererConfig::shader_max_stages];

		VkDescriptorPool descriptor_pool;

		VkDescriptorSetLayout descriptor_set_layouts[2];

		VkDescriptorSet global_descriptor_sets[RendererConfig::framebuffer_count];

		void* mapped_uniform_buffer;
		//Renderbuffer uniform_buffer;

		VulkanPipeline pipeline;

		VulkanShaderInstanceState instance_states[RendererConfig::shader_max_instances];

	};

	struct VulkanContext
	{
		int32(*find_memory_index)(uint32 type_filter, uint32 property_flags);	

		VkInstance instance;
		VkAllocationCallbacks* allocator_callbacks;
		VkSurfaceKHR surface;
		VulkanDevice device;

		VulkanSwapchain swapchain;

		Renderer::RenderTarget world_render_targets[RendererConfig::framebuffer_count];

		Sarray<VulkanCommandBuffer> graphics_command_buffers = {};

		Sarray<VkSemaphore> image_available_semaphores = {};
		Sarray<VkSemaphore> queue_complete_semaphores = {};

		VkFence framebuffer_fences[RendererConfig::framebuffer_count - 1];

#if defined(_DEBUG)
		VkDebugUtilsMessengerEXT debug_messenger;

		PFN_vkSetDebugUtilsObjectNameEXT debug_set_utils_object_name;
		PFN_vkSetDebugUtilsObjectTagEXT debug_set_utils_object_tag;
		PFN_vkCmdBeginDebugUtilsLabelEXT debug_begin_utils_label;
		PFN_vkCmdEndDebugUtilsLabelEXT debug_end_utils_label;
#endif

		Math::Vec4f viewport_rect;
		Math::Rect2Di scissor_rect;

		uint32 bound_framebuffer_index;
		uint32 bound_sync_object_index;
		
		uint32 framebuffer_size_generation;
		uint32 framebuffer_size_last_generation;

		uint32 framebuffer_width;
		uint32 framebuffer_height;	

		bool8 config_changed;
		bool8 recreating_swapchain;
		bool8 is_multithreaded;
	};

}

