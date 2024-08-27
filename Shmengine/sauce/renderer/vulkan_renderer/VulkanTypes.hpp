#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"
#include "containers/Sarray.hpp"
#include "containers/Buffer.hpp"
#include "utility/Math.hpp"
#include "memory/DynamicAllocator.hpp"
#include "containers/Hashtable.hpp"

#include "renderer/RendererTypes.hpp"

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
		VulkanRenderpassState state;
		uint32 clear_flags;
		bool32 has_prev_pass;
		bool32 has_next_pass;
	};

	struct VulkanSwapchain
	{
		RenderTarget render_targets[RendererConfig::frames_count];

		VkSurfaceFormatKHR image_format;
		VkSwapchainKHR handle;
		Sarray<Texture*> render_images = {};
		Texture* depth_texture;

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
		uint8 generations[RendererConfig::frames_count];
		uint32 ids[RendererConfig::frames_count];
	};

	struct VulkanShaderDescriptorSetState
	{
		VkDescriptorSet descriptor_sets[RendererConfig::frames_count];
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

		VkDescriptorSet global_descriptor_sets[RendererConfig::frames_count];

		void* mapped_uniform_buffer;
		//Renderbuffer uniform_buffer;

		VulkanPipeline pipeline;

		VulkanShaderInstanceState instance_states[RendererConfig::shader_max_instances];

	};

	struct VulkanGeometryData
	{
		uint32 id;
		uint32 generation;
		uint32 vertex_count;
		uint32 vertex_size;
		uint64 vertex_buffer_offset;
		uint32 index_count;
		uint32 index_size;
		uint64 index_buffer_offset;
	};

	struct VulkanContext
	{
		int32(*find_memory_index)(uint32 type_filter, uint32 property_flags);
		void(*on_render_target_refresh_required)();

		VkInstance instance;
		VkAllocationCallbacks* allocator_callbacks;
		VkSurfaceKHR surface;
		VulkanDevice device;

		VulkanSwapchain swapchain;

		Hashtable<uint32> renderpass_table;
		Renderer::Renderpass registered_renderpasses[RendererConfig::renderpass_max_registered];

		Renderbuffer object_vertex_buffer;
		Renderbuffer object_index_buffer;

		Renderer::RenderTarget world_render_targets[RendererConfig::frames_count];

		VulkanGeometryData geometries[RendererConfig::max_geometry_count];

		Sarray<VulkanCommandBuffer> graphics_command_buffers = {};

		Sarray<VkSemaphore> image_available_semaphores = {};
		Sarray<VkSemaphore> queue_complete_semaphores = {};

		VkFence fences_in_flight[RendererConfig::frames_count - 1];
		VkFence images_in_flight[RendererConfig::frames_count];

#if defined(_DEBUG)
		VkDebugUtilsMessengerEXT debug_messenger;
#endif

		uint32 image_index;
		uint32 current_frame;
		bool32 recreating_swapchain;

		uint32 framebuffer_size_generation;
		uint32 framebuffer_size_last_generation;

		uint32 framebuffer_width;
		uint32 framebuffer_height;

		bool8 is_multithreaded;

		float64 frame_delta_time;
	};

}

