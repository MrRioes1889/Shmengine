#include "platform/Platform.hpp"
#include "renderer/vulkan_renderer/VulkanPlatform.hpp"
#include "core/Event.hpp"
#include "core/Input.hpp"
#include "memory/LinearAllocator.hpp"

#if _WIN32

#include <windows.h>
#include <vulkan/vulkan_win32.h>

namespace Platform
{

    struct PlatformState {
        HINSTANCE h_instance;
        HWND hwnd;
        VkSurfaceKHR surface;
    };

    static PlatformState* plat_state;

    // Clock
    static float64 clock_frequency;
    static LARGE_INTEGER start_time;

    LRESULT CALLBACK win32_process_message(HWND hwnd, uint32 msg, WPARAM w_param, LPARAM l_param);

    bool32 startup(void* linear_allocator, void*& out_state, const char* application_name, int32 x, int32 y, int32 width, int32 height)
    {
        Memory::LinearAllocator* allocator = (Memory::LinearAllocator*)linear_allocator;
        out_state = Memory::linear_allocator_allocate(allocator, sizeof(PlatformState));
        plat_state = (PlatformState*)out_state;

        plat_state->h_instance = GetModuleHandleA(0);

        // Setup and register window class.
        HICON icon = LoadIcon(plat_state->h_instance, IDI_APPLICATION);
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_DBLCLKS;  // Get double-clicks
        wc.lpfnWndProc = win32_process_message;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = plat_state->h_instance;
        wc.hIcon = icon;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);  // NULL; // Manage the cursor manually
        wc.hbrBackground = NULL;                   // Transparent
        wc.lpszClassName = "shmengine_window_class";

        if (!RegisterClassA(&wc)) {
            MessageBoxA(0, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
            return FALSE;
        }

        // Create window
        uint32 client_x = x;
        uint32 client_y = y;
        uint32 client_width = width;
        uint32 client_height = height;

        uint32 window_x = client_x;
        uint32 window_y = client_y;
        uint32 window_width = client_width;
        uint32 window_height = client_height;

        uint32 window_style = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;
        uint32 window_ex_style = WS_EX_APPWINDOW;

        window_style |= WS_MAXIMIZEBOX;
        window_style |= WS_MINIMIZEBOX;
        window_style |= WS_THICKFRAME;

        // Obtain the size of the border.
        RECT border_rect = { 0, 0, 0, 0 };
        AdjustWindowRectEx(&border_rect, window_style, 0, window_ex_style);

        // In this case, the border rectangle is negative.
        window_x += border_rect.left;
        window_y += border_rect.top;

        // Grow by the size of the OS border.
        window_width += border_rect.right - border_rect.left;
        window_height += border_rect.bottom - border_rect.top;

        //console_write("Hallo :)", 3);

        HWND handle = CreateWindowExA(
            window_ex_style, "shmengine_window_class", application_name,
            window_style, window_x, window_y, window_width, window_height,
            0, 0, plat_state->h_instance, 0);

        if (handle == 0) {
            MessageBoxA(NULL, "Window creation failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);

            return FALSE;
        }
        else {
            plat_state->hwnd = handle;
        }

        // Show the window
        bool32 should_activate = 1;  // TODO: if the window should not accept input, this should be false.
        int32 show_window_command_flags = should_activate ? SW_SHOW : SW_SHOWNOACTIVATE;
        // If initially minimized, use SW_MINIMIZE : SW_SHOWMINNOACTIVE;
        // If initially maximized, use SW_SHOWMAXIMIZED : SW_MAXIMIZE
        ShowWindow(plat_state->hwnd, show_window_command_flags);

        // Clock setup
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        clock_frequency = 1.0 / (float64)frequency.QuadPart;
        QueryPerformanceCounter(&start_time);

        return TRUE;
    }

    void shutdown() {

        if (plat_state) {
            DestroyWindow(plat_state->hwnd);
            plat_state->hwnd = 0;
        }

        FreeConsole();

    }

    bool32 pump_messages()
    {
        MSG message;
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        return TRUE;
    }

    void* allocate(uint64 size, bool32 aligned)
    {
        return malloc(size);
    }

    void free_memory(void* block, bool32 aligned)
    {
        free(block);
    }

    void* zero_memory(void* block, uint64 size)
    {
        return memset(block, 0, size);
    }

    void* copy_memory(const void* source, void* dest, uint64 size)
    {
        return memcpy(dest, source, size);
    }

    void* set_memory(void* dest, int32 value, uint64 size)
    {
        return memset(dest, value, size);
    }

    void init_console()
    {
        AllocConsole();
        return;
    }

    void console_write(const char* message, uint8 color)
    {
        HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        //HANDLE console_handle = ((internal_state*)(state))->console_handle;
        // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
        static uint8 levels[6] = { 64, 4, 6, 2, 1, 8 };
        SetConsoleTextAttribute(console_handle, levels[color]);
        OutputDebugStringA(message);
        uint64 length = strlen(message);
        LPDWORD number_written = 0;
        WriteConsoleA(console_handle, message, (DWORD)length, number_written, 0);
    }

    void console_write_error(const char* message, uint8 color)
    {
        HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
        //HANDLE console_handle = ((internal_state*)(state))->console_handle;
        // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
        static uint8 levels[6] = { 64, 4, 6, 2, 1, 8 };
        SetConsoleTextAttribute(console_handle, levels[color]);
        OutputDebugStringA(message);
        uint64 length = strlen(message);
        LPDWORD number_written = 0;
        WriteConsoleA(console_handle, message, (DWORD)length, number_written, 0);
    }

    float64 get_absolute_time() {
        LARGE_INTEGER now_time;
        QueryPerformanceCounter(&now_time);
        return (float64)now_time.QuadPart * clock_frequency;
    }

    void sleep(uint32 ms) {
        Sleep(ms);
    }

    bool32 create_vulkan_surface(VulkanContext* context)
    {

        VkWin32SurfaceCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        create_info.hinstance = plat_state->h_instance;
        create_info.hwnd = plat_state->hwnd;

        if (vkCreateWin32SurfaceKHR(context->instance, &create_info, context->allocator_callbacks, &plat_state->surface) != VK_SUCCESS)
        {
            SHMFATAL("Failed to create vulkan surface");
            return false;
        }

        context->surface = plat_state->surface;
        return true;
    }

    LRESULT CALLBACK win32_process_message(HWND hwnd, uint32 msg, WPARAM w_param, LPARAM l_param) {
        switch (msg) {
        case WM_ERASEBKGND:
            // Notify the OS that erasing will be handled by the application to prevent flicker.
            return 1;
        case WM_CLOSE:
            // TODO: Fire an event for the application to quit.
            event_fire(EVENT_CODE_APPLICATION_QUIT, 0, {});
            return 1;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE: {
            // Get the updated size.
             RECT r;
             GetClientRect(hwnd, &r);
             uint32 width = r.right - r.left;
             uint32 height = r.bottom - r.top;

             EventData e = {};
             e.ui32[0] = width;
             e.ui32[1] = height;
             event_fire(EVENT_CODE_WINDOW_RESIZED, 0, e);
        } break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            // Key pressed/released
            bool32 pressed = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            Keys key = (Keys)w_param;

            Input::process_key(key, pressed);

        } break;
        case WM_MOUSEMOVE: {
            // Mouse move
            int32 x = LOWORD(l_param);
            int32 y = HIWORD(l_param);
            Input::process_mouse_move(x, y);
        } break;
        case WM_MOUSEWHEEL: {
             int32 delta = GET_WHEEL_DELTA_WPARAM(w_param);
             if (delta != 0) {
                 // Flatten the input to an OS-independent (-1, 1)
                 delta = (delta < 0) ? -1 : 1;
                 Input::process_mouse_scroll(delta);
             }
        } break;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP: {
            bool32 pressed = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
            Mousebuttons button = Mousebuttons::BUTTON_MAX_BUTTONS;
            switch (msg)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                button = Mousebuttons::LMB;
                break;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                button = Mousebuttons::RMB;
                break;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                button = Mousebuttons::MMB;
                break;
            }       

            Input::process_mousebutton(button, pressed);
        } break;
        }

        return DefWindowProcA(hwnd, msg, w_param, l_param);
    }

}

#endif