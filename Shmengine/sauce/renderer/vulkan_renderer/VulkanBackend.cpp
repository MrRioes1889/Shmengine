#include "VulkanBackend.hpp"

#include "VulkanTypes.hpp"
#include "VulkanDevice.hpp"
#include "VulkanPlatform.hpp"
#include "core/Logging.hpp"
#include "containers/Darray.hpp"
#include "utility/string/String.hpp"

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

	static VulkanContext context = {};

	bool32 vulkan_init(Backend* backend, const char* application_name, Platform::PlatformState* plat_state)
	{
		//TODO: Replace with own memory allocation callbacks!
		context.allocator_callbacks = 0;

		VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app_info.apiVersion = VK_API_VERSION_1_2;
		app_info.pApplicationName = application_name;
		app_info.applicationVersion = VK_MAKE_VERSION(0,0,1);
		app_info.pEngineName = "Shmengine";
		app_info.engineVersion = VK_MAKE_VERSION(0,0,1);

		VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		create_info.pApplicationInfo = &app_info;

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

		create_info.enabledExtensionCount = extension_count;
		create_info.ppEnabledExtensionNames = extension_names;

		create_info.enabledLayerCount = validation_layer_count;
		create_info.ppEnabledLayerNames = validation_layer_names;

		//NOTE: The second agument for this function is meant to contain callbacks for custom memory allocation.
		VK_CHECK(vkCreateInstance(&create_info, context.allocator_callbacks, &context.instance));

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

		SHMINFO("Vulkan instance initialized successfully!");
		return true;
	}

	void vulkan_shutdown(Backend* backend)
	{		
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