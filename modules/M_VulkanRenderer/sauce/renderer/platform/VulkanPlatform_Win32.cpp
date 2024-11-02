#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "VulkanPlatform.hpp"
#include <platform/Platform.hpp>
#include <core/Logging.hpp>

namespace Renderer::Vulkan
{
	extern Renderer::Vulkan::VulkanContext context;
   
    bool32 create_vulkan_surface()
    {
	    Platform::WindowHandle handle = Platform::get_window_handle();

        VkWin32SurfaceCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        create_info.hinstance = (HINSTANCE)handle.h_instance;
        create_info.hwnd = (HWND)handle.h_wnd;
        VkResult result = vkCreateWin32SurfaceKHR(context.instance, &create_info, context.allocator_callbacks, &context.surface);
        if (result != VK_SUCCESS) {
            SHMFATAL("Vulkan surface creation failed.");
            return false;
        }
        return true;
    }

}
