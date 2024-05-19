#include "VulkanBackend.hpp"

#include "VulkanTypes.hpp"
#include "VulkanDevice.hpp"
#include "VulkanPlatform.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanRenderpass.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanFramebuffer.hpp"
#include "VulkanFence.hpp"

#include "core/Logging.hpp"
#include "containers/Darray.hpp"
#include "utility/string/String.hpp"

#include "core/Application.hpp"

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data
);

namespace Renderer
{

#if _WIN32
	static const uint32 extension_count = 2 + _DEBUG;
	static const char* extension_names[extension_count] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		"VK_KHR_win32_surface",
#if defined(_DEBUG) 
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
	};

#else
	throw an error
#endif	

#if defined(_DEBUG)
	static const uint32 validation_layer_count = 1;
	static const char* validation_layer_names[validation_layer_count] = 
	{
		"VK_LAYER_KHRONOS_validation"
	};
#else
	static const uint32 validation_layer_count = 0;
	static const char** validation_layer_names = 0;
#endif

	static void create_command_buffers(Backend* backend);
	static void regenerate_framebuffers(Backend* backend, VulkanSwapchain* swapchain, VulkanRenderpass* renderpass);
	int32 find_memory_index(uint32 type_filter, uint32 property_flags);

	static VulkanContext context = {};
	static uint32 cached_framebuffer_width = 0;
	static uint32 cached_framebuffer_height = 0;

	bool32 vulkan_init(Backend* backend, const char* application_name, Platform::PlatformState* plat_state)
	{

		context.find_memory_index = find_memory_index;

		//TODO: Replace with own memory allocation callbacks!
		context.allocator_callbacks = 0;

		Application::get_framebuffer_size(&cached_framebuffer_width, &cached_framebuffer_height);
		context.framebuffer_width = (cached_framebuffer_width) ? cached_framebuffer_width : 800;
		context.framebuffer_height = (cached_framebuffer_height) ? cached_framebuffer_height : 600;
		cached_framebuffer_width = 0;
		cached_framebuffer_height = 0;

		VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app_info.apiVersion = VK_API_VERSION_1_2;
		app_info.pApplicationName = application_name;
		app_info.applicationVersion = VK_MAKE_VERSION(0,0,1);
		app_info.pEngineName = "Shmengine";
		app_info.engineVersion = VK_MAKE_VERSION(0,0,1);

		VkInstanceCreateInfo inst_create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		inst_create_info.pApplicationInfo = &app_info;

#if defined(_DEBUG)
		SHMDEBUG("Required vulkan extensions:");

		for (uint32 i = 0; i < extension_count; i++)
			SHMDEBUG(extension_names[i]);


		SHMDEBUG("Vulkan Validation layers enabled.");

		uint32 available_layer_count = 0;
		VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
		VkLayerProperties* available_layers = darray_create_and_reserve(VkLayerProperties, available_layer_count);
		VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));

		for (uint32 i = 0; i < validation_layer_count; i++)
		{
			SHMDEBUGV("Searching for layer: %s...", validation_layer_names[i]);
			bool32 found = false;
			for (uint32 j = 0; j < available_layer_count; j++)
			{
				if (String::strings_eq((char*)validation_layer_names[i], available_layers[j].layerName))
				{
					found = true;
					SHMDEBUG("Found.");
					break;
				}
			}

			if (!found)
			{
				SHMFATALV("Failed to find required vulkan validation layers: %s!", validation_layer_names[i]);
				return false;
			}
		}

		darray_destroy(available_layers);
		SHMDEBUG("All required vulkan validation layers present.");
#endif

		inst_create_info.enabledExtensionCount = extension_count;
		inst_create_info.ppEnabledExtensionNames = extension_names;

		inst_create_info.enabledLayerCount = validation_layer_count;
		inst_create_info.ppEnabledLayerNames = validation_layer_names;

		//NOTE: The second agument for this function is meant to contain callbacks for custom memory allocation.
		VK_CHECK(vkCreateInstance(&inst_create_info, context.allocator_callbacks, &context.instance));

#if defined(_DEBUG)
		SHMDEBUG("Creating Vulkan Debugger...");
		uint32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

		VkDebugUtilsMessengerCreateInfoEXT debugger_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debugger_create_info.messageSeverity = log_severity;
		debugger_create_info.messageType = 
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debugger_create_info.pfnUserCallback = vk_debug_callback;

		PFN_vkCreateDebugUtilsMessengerEXT func =
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT");
		SHMASSERT_MSG(func, "Failed to create vulkan debug messenger!");
		VK_CHECK(func(context.instance, &debugger_create_info, context.allocator_callbacks, &context.debug_messenger));
		SHMDEBUG("Vulkan debug messenger created.");
#endif

		SHMDEBUG("Creating vulkan surface...");
		if (!Platform::create_vulkan_surface(plat_state, &context))
		{
			SHMERROR("Failed to create vulkan surface");
			return false;
		}
		SHMDEBUG("Vulkan surface created.");

		SHMDEBUG("Creating vulkan device...");
		if (!vulkan_device_create(&context))
		{
			SHMERROR("Failed to create vulkan device.");
			return false;
		}
		SHMDEBUG("Vulkan device created.");

		vulkan_swapchain_create(&context, context.framebuffer_width, context.framebuffer_height, &context.swapchain);

		vulkan_renderpass_create(
			&context, 
			&context.main_renderpass,
			{0, 0}, 
			{context.framebuffer_width, context.framebuffer_height},
			{0.0f, 0.0f, 0.2f, 1.0f}, 
			1.0f, 
			0);

		context.swapchain.framebuffers.init(context.swapchain.images.count);
		regenerate_framebuffers(backend, &context.swapchain, &context.main_renderpass);

		create_command_buffers(backend);

		context.image_available_semaphores.init(context.swapchain.max_frames_in_flight);
		context.queue_complete_semaphores.init(context.swapchain.max_frames_in_flight);
		context.fences_in_flight.init(context.swapchain.max_frames_in_flight);

		for (uint32 i = 0; i < context.swapchain.max_frames_in_flight; i++)
		{
			VkSemaphoreCreateInfo sem_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			vkCreateSemaphore(context.device.logical_device, &sem_create_info, context.allocator_callbacks, &context.image_available_semaphores[i]);
			vkCreateSemaphore(context.device.logical_device, &sem_create_info, context.allocator_callbacks, &context.queue_complete_semaphores[i]);

			vulkan_fence_create(&context, true, &context.fences_in_flight[i]);
		}

		context.images_in_flight.init(context.swapchain.max_frames_in_flight);
		context.images_in_flight.clear();

		SHMINFO("Vulkan instance initialized successfully!");
		return true;
	}

	void vulkan_shutdown(Backend* backend)
	{		

		vkDeviceWaitIdle(context.device.logical_device);

		SHMDEBUG("Destroying vulkan semaphores and fences...");
		for (uint32 i = 0; i < context.swapchain.max_frames_in_flight; i++)
		{
			if (context.image_available_semaphores[i])
				vkDestroySemaphore(context.device.logical_device, context.image_available_semaphores[i], context.allocator_callbacks);
			context.image_available_semaphores[i] = 0;

			if (context.queue_complete_semaphores[i])
				vkDestroySemaphore(context.device.logical_device, context.queue_complete_semaphores[i], context.allocator_callbacks);
			context.queue_complete_semaphores[i] = 0;

			vulkan_fence_destroy(&context, &context.fences_in_flight[i]);
		}

		context.image_available_semaphores.free_data();
		context.queue_complete_semaphores.free_data();
		context.fences_in_flight.free_data();

		SHMDEBUG("Destroying vulkan framebuffers...");
		for (uint32 i = 0; i < context.swapchain.framebuffers.count; i++)
			vullkan_framebuffer_destroy(&context, &context.swapchain.framebuffers[i]);

		SHMDEBUG("Destroying vulkan renderpass...");
		vulkan_renderpass_destroy(&context, &context.main_renderpass);

		SHMDEBUG("Destroying vulkan swapchain...");
		vulkan_swapchain_destroy(&context, &context.swapchain);

		SHMDEBUG("Destroying vulkan device...");
		vulkan_device_destroy(&context);

		SHMDEBUG("Destroying vulkan surface...");
		if (context.surface)
		{
			vkDestroySurfaceKHR(context.instance, context.surface, context.allocator_callbacks);
			context.surface = 0;
		}	

		if (context.debug_messenger)
		{
			SHMDEBUG("Destroying vulkan debugger...");
			PFN_vkDestroyDebugUtilsMessengerEXT func =
				(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT");
			func(context.instance, context.debug_messenger, context.allocator_callbacks);
		}

		SHMDEBUG("Destroying vulkan instance...");
		vkDestroyInstance(context.instance, context.allocator_callbacks);
	}

	void vulkan_on_resized(Backend* backend, uint32 width, uint32 height)
	{

	}

	bool32 vulkan_begin_frame(Backend* backend, float32 delta_time)
	{
		return true;
	}

	bool32 vulkan_end_frame(Backend* backend, float32 delta_time)
	{
		return true;
	}

	int32 find_memory_index(uint32 type_filter, uint32 property_flags)
	{
		VkPhysicalDeviceMemoryProperties memory_properties;
		vkGetPhysicalDeviceMemoryProperties(context.device.physical_device, &memory_properties);

		for (uint32 i = 0; i < memory_properties.memoryTypeCount; i++)
		{
			if ((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & property_flags))
				return i;
		}

		SHMWARN("Unable to find suitable memory type!");
		return -1;
	}

	static void create_command_buffers(Renderer::Backend* backend)
	{
		if (!context.graphics_command_buffers.data)
		{
			context.graphics_command_buffers.init(context.swapchain.images.count);
			for (uint32 i = 0; i < context.graphics_command_buffers.count; i++)
				context.graphics_command_buffers[i] = {};
		}

		for (uint32 i = 0; i < context.graphics_command_buffers.count; i++)
		{
			if (context.graphics_command_buffers[i].handle)
				vulkan_command_buffer_free(&context, context.device.graphics_command_pool, &context.graphics_command_buffers[i]);
			context.graphics_command_buffers[i] = {};
			vulkan_command_buffer_allocate(&context, context.device.graphics_command_pool, true, &context.graphics_command_buffers[i]);
		}
			
		SHMDEBUG("Command buffers created.");
	}

	static void regenerate_framebuffers(Backend* backend, VulkanSwapchain* swapchain, VulkanRenderpass* renderpass)
	{

		for (uint32 i = 0; i < swapchain->images.count; i++)
		{
			const uint32 attachment_count = 2;
			VkImageView attachments[attachment_count] = { swapchain->views[i], swapchain->depth_attachment.view };

			vulkan_framebuffer_create(&context,	renderpass,	attachment_count, attachments, &context.swapchain.framebuffers[i]);
		}

	}

}



VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data
)
{
	switch (message_severity)
	{
	default:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		SHMERROR(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		SHMWARN(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		SHMINFO(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		SHMTRACE(callback_data->pMessage);
		break;
	}

	return VK_FALSE;
}

