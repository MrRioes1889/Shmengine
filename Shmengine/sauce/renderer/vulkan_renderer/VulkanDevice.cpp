#include "VulkanDevice.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/CString.hpp"
#include "containers/Sarray.hpp"

namespace Renderer::Vulkan
{

	struct VulkanPhysicalDeviceRequirements
	{
		Sarray<const char*> device_extension_names;

		bool32 graphics;
		bool32 present;
		bool32 compute;
		bool32 transfer;	
		bool32 sampler_anisotropy;
		bool32 discrete_gpu;
	};

	struct VulkanPhysicalDeviceQueueFamilyInfo
	{
		int32 graphics_family_index;
		int32 present_family_index;
		int32 compute_family_index;
		int32 transfer_family_index;
	};

	static bool32 select_physical_device(VulkanContext* context);
	static bool32 physical_device_meets_requirements(
		VkPhysicalDevice device,
		VkSurfaceKHR surface,
		const VkPhysicalDeviceProperties* properties,
		const VkPhysicalDeviceFeatures* features,
		VulkanPhysicalDeviceRequirements* requirements,
		VulkanPhysicalDeviceQueueFamilyInfo* out_queue_family_info,
		VulkanSwapchainSupportInfo* out_swapchain_support
	);

	bool32 vulkan_device_create(VulkanContext* context)
	{
		if (!select_physical_device(context))
			return false;

		SHMINFO("Creating logical device...");
		// NOTE: Do not create additional queues for shared indices
		bool32 present_shares_graphics_queue = context->device.graphics_queue_index == context->device.present_queue_index;
		bool32 transfer_shares_graphics_queue = context->device.graphics_queue_index == context->device.transfer_queue_index;
		uint32 index_count = 1;
		if (!present_shares_graphics_queue)
			index_count++;
		if (!transfer_shares_graphics_queue)
			index_count++;

		Sarray<uint32> indices(index_count, 0);
		uint32 index = 0;
		indices[index++] = context->device.graphics_queue_index;
		if (!present_shares_graphics_queue)
			indices[index++] = context->device.present_queue_index;
		if (!transfer_shares_graphics_queue)
			indices[index++] = context->device.transfer_queue_index;

		// TODO: Figure out what queue priority is used for. Declaring static queue priority for now, since pointers to it are needed.  
		static float32 queue_priority = 1.0f;
		Sarray<VkDeviceQueueCreateInfo> queue_create_infos(index_count, 0);
		for (uint32 i = 0; i < index_count; i++)
		{
			queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_infos[i].queueFamilyIndex = indices[i];
			queue_create_infos[i].queueCount = 1 + (indices[i] == (uint32)context->device.graphics_queue_index);
			queue_create_infos[i].flags = 0;
			queue_create_infos[i].pNext = 0;
			queue_create_infos[i].pQueuePriorities = &queue_priority;
		}

		// Request device features
		// TODO: should be config driven
		VkPhysicalDeviceFeatures device_features = {};
		device_features.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		device_create_info.queueCreateInfoCount = index_count;
		device_create_info.pQueueCreateInfos = queue_create_infos.data;
		device_create_info.pEnabledFeatures = &device_features;
		device_create_info.enabledExtensionCount = 1;
		const char* extension_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		device_create_info.ppEnabledExtensionNames = &extension_names;

		// NOTE: Device Layers deprecated for current vulkan versions
		device_create_info.enabledLayerCount = 0;
		device_create_info.ppEnabledLayerNames = 0;

		VK_CHECK(vkCreateDevice(context->device.physical_device, &device_create_info, context->allocator_callbacks, &context->device.logical_device));
		SHMINFO("Logical device created");

		// NOTE: Retrieving queues
		vkGetDeviceQueue(context->device.logical_device, context->device.graphics_queue_index, 0, &context->device.graphics_queue);
		vkGetDeviceQueue(context->device.logical_device, context->device.present_queue_index, 0, &context->device.present_queue);
		vkGetDeviceQueue(context->device.logical_device, context->device.transfer_queue_index, 0, &context->device.transfer_queue);
		SHMINFO("Queues retrieved.");

		VkCommandPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		pool_create_info.queueFamilyIndex = context->device.graphics_queue_index;
		pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VK_CHECK(vkCreateCommandPool(context->device.logical_device, &pool_create_info, context->allocator_callbacks, &context->device.graphics_command_pool));
		SHMINFO("Graphics command pool created.");

		return true;
	}

	void vulkan_device_destroy(VulkanContext* context)
	{

		context->device.graphics_queue = 0;
		context->device.present_queue = 0;
		context->device.transfer_queue = 0;

		SHMDEBUG("Destroying graphics command pool...");
		if (context->device.graphics_command_pool)
		{
			vkDestroyCommandPool(context->device.logical_device, context->device.graphics_command_pool, context->allocator_callbacks);
			context->device.graphics_command_pool = 0;
		}

		SHMDEBUG("Destroying logical device...");
		if (context->device.logical_device)
		{
			vkDestroyDevice(context->device.logical_device, context->allocator_callbacks);
			context->device.logical_device = 0;
		}

		SHMDEBUG("Releasing physical device resources...");
		context->device.physical_device = 0;

		if (context->device.swapchain_support.formats)
		{
			Memory::free_memory(context->device.swapchain_support.formats);
			context->device.swapchain_support.formats = 0;
			context->device.swapchain_support.format_count = 0;
		}

		if (context->device.swapchain_support.present_modes)
		{
			Memory::free_memory(context->device.swapchain_support.present_modes);
			context->device.swapchain_support.present_modes = 0;
			context->device.swapchain_support.present_mode_count = 0;
		}

		context->device.swapchain_support.capabilities = {};
		context->device.graphics_queue_index = -1;
		context->device.present_queue_index = -1;
		context->device.transfer_queue_index = -1;

	}

	void vulkan_device_query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo* out_support_info)
	{

		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &out_support_info->capabilities));

		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &out_support_info->format_count, 0));

		if (out_support_info->format_count)
		{
			if (!out_support_info->formats)
				out_support_info->formats =
				(VkSurfaceFormatKHR*)Memory::allocate(sizeof(VkSurfaceFormatKHR) * out_support_info->format_count, AllocationTag::RENDERER);

			VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &out_support_info->format_count, out_support_info->formats));
		}

		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &out_support_info->present_mode_count, 0));

		if (out_support_info->present_mode_count)
		{
			if (!out_support_info->present_modes)
				out_support_info->present_modes =
				(VkPresentModeKHR*)Memory::allocate(sizeof(VkPresentModeKHR) * out_support_info->present_mode_count, AllocationTag::RENDERER);

			VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &out_support_info->present_mode_count, out_support_info->present_modes));
		}
	}

	bool32 vulkan_device_detect_depth_format(VulkanDevice* device)
	{
		const uint32 candidate_count = 3;
		VkFormat candidates[candidate_count] =
		{
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT
		};

		uint8 sizes[candidate_count] =
		{
			4,
			4,
			3
		};

		uint32 flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
		for (uint32 i = 0; i < candidate_count; i++)
		{
			VkFormatProperties properties;
			vkGetPhysicalDeviceFormatProperties(device->physical_device, candidates[i], &properties);

			if (((properties.linearTilingFeatures & flags) == flags) || ((properties.optimalTilingFeatures & flags) == flags))
			{
				device->depth_format = candidates[i];
				device->depth_channel_count = sizes[i];
				return true;
			}
		}

		device->depth_format = VK_FORMAT_UNDEFINED;
		return false;
	}

	static bool32 select_physical_device(VulkanContext* context)
	{
		uint32 physical_device_count = 0;
		VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, 0));
		if (!physical_device_count)
		{
			SHMFATAL("No physical devices which support Vulkan were found.");
			return false;
		}

		VulkanPhysicalDeviceRequirements requirements = { .device_extension_names = Sarray<const char*>(1, 0) };
		requirements.graphics = true;
		requirements.present = true;
		requirements.transfer = true;
		requirements.compute = true;
		requirements.sampler_anisotropy = true;
		requirements.discrete_gpu = true;
		requirements.device_extension_names[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;


		//NOTE: Assigning placeholder char pointers, since decltype cant handle string literals
		/*const char* swapchain_extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		darray_push(requirements.device_extension_names, swapchain_extension_name);*/

		Sarray<VkPhysicalDevice> physical_devices(physical_device_count, 0);
		VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices.data));
		for (uint32 i = 0; i < physical_device_count; i++)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physical_devices[i], &properties);

			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceFeatures(physical_devices[i], &features);

			VkPhysicalDeviceMemoryProperties memory_properties;
			vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &memory_properties);

			context->device.supports_device_local_host_visible = false;
			for (uint32 j = 0; j < memory_properties.memoryTypeCount; j++)
			{
				if ((memory_properties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && (memory_properties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
				{
					context->device.supports_device_local_host_visible = true;
					break;
				}
			}

			VulkanPhysicalDeviceQueueFamilyInfo queue_info = {};
			bool32 meets_requirements = physical_device_meets_requirements(
				physical_devices[i],
				context->surface,
				&properties,
				&features,
				&requirements,
				&queue_info,
				&context->device.swapchain_support);

			if (meets_requirements)
			{
				switch (properties.deviceType)
				{
				default:
				case VK_PHYSICAL_DEVICE_TYPE_OTHER:
					SHMINFO("GPU type is Unknown.");
					break;
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
					SHMINFO("GPU type is Integrated.");
					break;
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
					SHMINFO("GPU type is Discrete.");
					break;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
					SHMINFO("GPU type is Virtual.");
					break;
				case VK_PHYSICAL_DEVICE_TYPE_CPU:
					SHMINFO("GPU type is CPU.");
					break;
				}

				SHMINFOV("GPU driver version: %i.%i.%i",
					VK_VERSION_MAJOR(properties.driverVersion),
					VK_VERSION_MINOR(properties.driverVersion),
					VK_VERSION_PATCH(properties.driverVersion));

				SHMINFOV("Vulkan API version: %i.%i.%i",
					VK_VERSION_MAJOR(properties.apiVersion),
					VK_VERSION_MINOR(properties.apiVersion),
					VK_VERSION_PATCH(properties.apiVersion));

				for (uint32 j = 0; j < memory_properties.memoryHeapCount; j++)
				{
					float32 memory_size_gib =
						(((float32)memory_properties.memoryHeaps[j].size) / 1024.0f / 1024.0f / 1024.0f);
					if (memory_properties.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
						SHMINFOV("Local GPU memory: %f GiB", memory_size_gib);
					else
						SHMINFOV("Shared system memory: %f GiB", memory_size_gib);
				}

				context->device.physical_device = physical_devices[i];
				context->device.graphics_queue_index = queue_info.graphics_family_index;
				context->device.present_queue_index = queue_info.present_family_index;
				context->device.transfer_queue_index = queue_info.transfer_family_index;

				context->device.properties = properties;
				context->device.features = features;
				context->device.memory = memory_properties;

				break;
			}

		}

		if (!context->device.physical_device)
		{
			SHMERROR("No physical device meeting the requirements found.");
			return false;
		}

		SHMINFO("Physical device selected.");
		return true;

	}

	static bool32 physical_device_meets_requirements(
		VkPhysicalDevice device,
		VkSurfaceKHR surface,
		const VkPhysicalDeviceProperties* properties,
		const VkPhysicalDeviceFeatures* features,
		VulkanPhysicalDeviceRequirements* requirements,
		VulkanPhysicalDeviceQueueFamilyInfo* out_queue_info,
		VulkanSwapchainSupportInfo* out_swapchain_support
	)
	{

		out_queue_info->graphics_family_index = (uint32)-1;
		out_queue_info->compute_family_index = (uint32)-1;
		out_queue_info->present_family_index = (uint32)-1;
		out_queue_info->transfer_family_index = (uint32)-1;

		if (requirements->discrete_gpu && properties->deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			SHMINFO("Device is not a discrete GPU, and one is required. Skipping device.");
			return false;
		}

		uint32 queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, 0);
		Sarray<VkQueueFamilyProperties> queue_families(queue_family_count, 0);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data);

		SHMINFO("Graphics | Present | Compute | Transfer | Name");
		uint8 min_transfer_score = 255;
		for (uint32 i = 0; i < queue_family_count; i++)
		{
			uint8 transfer_score = 0;

			if (out_queue_info->graphics_family_index < 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				out_queue_info->graphics_family_index = i;
				transfer_score++;

				VkBool32 supports_present = false;
				VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supports_present));
				if (supports_present) {
					out_queue_info->present_family_index = i;
					transfer_score++;
				}
			}

			if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				out_queue_info->compute_family_index = i;
				transfer_score++;
			}

			if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				if (transfer_score <= min_transfer_score)
				{
					min_transfer_score = transfer_score;
					out_queue_info->transfer_family_index = i;
				}
			}
		}

		if (out_queue_info->present_family_index < 0) {
			for (uint32 i = 0; i < queue_family_count; ++i) {
				VkBool32 supports_present = VK_FALSE;
				VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supports_present));
				if (supports_present) {
					out_queue_info->present_family_index = i;

					if (out_queue_info->present_family_index != out_queue_info->graphics_family_index) {
						SHMWARNV("Warning: Different queue index used for present vs graphics: %u.", i);
					}
					break;
				}
			}
		}

		SHMINFOV("	%i |	%i |	%i |	%i | %s",
			out_queue_info->graphics_family_index != -1,
			out_queue_info->present_family_index != -1,
			out_queue_info->compute_family_index != -1,
			out_queue_info->transfer_family_index != -1,
			properties->deviceName);

		if (
			(!requirements->graphics || out_queue_info->graphics_family_index != -1) &&
			(!requirements->present || out_queue_info->present_family_index != -1) &&
			(!requirements->compute || out_queue_info->compute_family_index != -1) &&
			(!requirements->transfer || out_queue_info->transfer_family_index != -1)
			)
		{
			SHMINFO("Device meets queue requirements.");
			SHMTRACEV("Graphics Family index: %i", out_queue_info->graphics_family_index);
			SHMTRACEV("Present Family index: %i", out_queue_info->present_family_index);
			SHMTRACEV("Compute Family index: %i", out_queue_info->compute_family_index);
			SHMTRACEV("Transfer Family index: %i", out_queue_info->transfer_family_index);

			vulkan_device_query_swapchain_support(device, surface, out_swapchain_support);

			if (!out_swapchain_support->format_count || !out_swapchain_support->present_mode_count)
			{
				if (out_swapchain_support->formats)
					Memory::free_memory(out_swapchain_support->formats);

				if (out_swapchain_support->present_modes)
					Memory::free_memory(out_swapchain_support->present_modes);

				SHMINFO("Required swapchain support not present. Skipping device.");
				return false;
			}

			if (requirements->device_extension_names.data)
			{
				uint32 required_extension_count = requirements->device_extension_names.capacity;
				uint32 available_extension_count = 0;
				VkExtensionProperties* available_extensions = 0;
				VK_CHECK(vkEnumerateDeviceExtensionProperties(device, 0, &available_extension_count, 0));
				if (!available_extension_count)
				{
					SHMINFO("No available extensions found. Skipping Device.");
					return false;
				}
				available_extensions =
					(VkExtensionProperties*)Memory::allocate(sizeof(VkExtensionProperties) * available_extension_count, AllocationTag::RENDERER);
				VK_CHECK(vkEnumerateDeviceExtensionProperties(device, 0, &available_extension_count, available_extensions));

				for (uint32 i = 0; i < required_extension_count; i++)
				{
					bool32 found = false;
					for (uint32 j = 0; i < available_extension_count; j++)
					{
						if (CString::equal(requirements->device_extension_names[i], available_extensions[j].extensionName))
						{
							found = true;
							break;
						}
					}

					if (!found)
					{
						SHMINFOV("Required extension not found: '%s'. Skipping Device.", requirements->device_extension_names[i]);
						Memory::free_memory(available_extensions);
						return false;
					}
				}

				Memory::free_memory(available_extensions);

			}

			if (requirements->sampler_anisotropy && !features->samplerAnisotropy)
			{
				SHMINFO("Device does not support sampler anisotropy. Skipping Device.");
				return false;
			}

			return true;
		}

		return false;
	}

}
