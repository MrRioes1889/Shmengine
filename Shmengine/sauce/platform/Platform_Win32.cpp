#include "platform/Platform.hpp"
#include "core/Event.hpp"
#include "core/Input.hpp"
#include "core/Logging.hpp"
#include "memory/LinearAllocator.hpp"

#include "optick.h"


#if _WIN32

#include <windows.h>
#include <hidusage.h>

namespace Platform
{

    struct FileWatch
    {
        String file_path;
        FILETIME last_write_timestamp;
    };

    struct PlatformState {
        HINSTANCE h_instance;
        HWND hwnd;
        bool32 cursor_clipped;
        uint32 client_x;
        uint32 client_y;
        uint32 client_width;
        uint32 client_height;        

        Darray<FileWatch> file_watches;
    };

    static PlatformState* plat_state = 0;

    // Clock
    static float64 clock_frequency;
    static LARGE_INTEGER start_time;

    static bool32 win32_process_message_fast(uint32 msg, WPARAM w_param, LPARAM l_param);
    LRESULT CALLBACK win32_process_message(HWND hwnd, uint32 msg, WPARAM w_param, LPARAM l_param);

    bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
    {

        SystemConfig* sys_config = (SystemConfig*)config;
        plat_state = (PlatformState*)allocator_callback(allocator, sizeof(PlatformState));

        plat_state->h_instance = GetModuleHandleA(0);

        plat_state->cursor_clipped = false;

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
        plat_state->client_x = sys_config->x;
        plat_state->client_y = sys_config->y;
        plat_state->client_width = sys_config->width;
        plat_state->client_height = sys_config->height;

        uint32 window_x = plat_state->client_x;
        uint32 window_y = plat_state->client_y;
        uint32 window_width = plat_state->client_width;
        uint32 window_height = plat_state->client_height;

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
            window_ex_style, "shmengine_window_class", sys_config->application_name,
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

        // Setup mouse for low-level offset input messages
        RAWINPUTDEVICE Rid[1];
        Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        Rid[0].dwFlags = RIDEV_INPUTSINK;
        Rid[0].hwndTarget = plat_state->hwnd;
        RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

        // Clock setup
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        clock_frequency = 1.0 / (float64)frequency.QuadPart;
        QueryPerformanceCounter(&start_time);

        timeBeginPeriod(1);

        plat_state->file_watches.init(8, 0);

        return TRUE;
    }

    void system_shutdown(void* state) {

        if (plat_state) {
            DestroyWindow(plat_state->hwnd);
            plat_state->hwnd = 0;
        }

        FreeConsole();

    }

    ReturnCode get_last_error()
    {
        DWORD err = GetLastError();
        switch (err)
        {
        case ERROR_FILE_NOT_FOUND:
            return Platform::ReturnCode::FILE_NOT_FOUND;
        case ERROR_SHARING_VIOLATION:
            return Platform::ReturnCode::FILE_LOCKED;
        case ERROR_FILE_EXISTS:
            return Platform::ReturnCode::FILE_ALREADY_EXISTS;
        default:
            return Platform::ReturnCode::UNKNOWN;
        }
    }

    bool32 pump_messages()
    {
        MSG message;
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) 
        {
            if (!win32_process_message_fast(message.message, message.wParam, message.lParam))
            {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            }
        }

        return TRUE;
    }

    const char* get_root_dir()
    {

        static char executable_path[256] = {};

        if (!executable_path[0])
        {
            char* buffer = executable_path;
            uint32 buffer_length = sizeof(executable_path);

            GetModuleFileNameA(0, buffer, (DWORD)buffer_length);
            CString::replace(buffer, '\\', '/');
            CString::left_of_last(buffer, buffer_length, '/');
            CString::append(buffer, buffer_length, '/');            
        }

        return executable_path;   
    }

#define DEV_SYSTEM 1

    void* allocate(uint64 size, uint16 alignment)
    {
        if (alignment > 1)
        {
#if DEV_SYSTEM
            static uint64 start_adress = Gibibytes(1) * 1024 * 4;
            void* ret = VirtualAlloc((void*)start_adress, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            start_adress += Gibibytes(16);
            return ret;
#else
            return _aligned_malloc(size, alignment);
#endif
        }      
        else
            return malloc(size);
    }

    void free_memory(void* block, bool32 aligned)
    {
        if (aligned)
#if DEV_SYSTEM
        {
            VirtualFree(block, 0, MEM_RELEASE);
        }
#else
        {
            return _aligned_free(block);
        }
#endif
        else
            return free(block);
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

    Platform::ReturnCode register_file_watch(const char* path, uint32* out_watch_id)
    {

        *out_watch_id = INVALID_ID;

        for (uint32 i = 0; i < plat_state->file_watches.count; i++)
        {
            if (CString::equal_i(path, plat_state->file_watches[i].file_path.c_str()))
            {
                *out_watch_id = i;
                return ReturnCode::SUCCESS;
            }
        }

        WIN32_FIND_DATAA find_data;
        HANDLE file_handle = FindFirstFileA(path, &find_data);
        if (file_handle == INVALID_HANDLE_VALUE)
            return get_last_error();
        if (!FindClose(file_handle))
            return get_last_error();

        FileWatch* watch = &plat_state->file_watches[plat_state->file_watches.emplace()];
        watch->file_path = path;
        watch->last_write_timestamp = find_data.ftLastWriteTime;

        return ReturnCode::SUCCESS;
    }

    bool32 unregister_file_watch(uint32 watch_id)
    {
        if (watch_id > plat_state->file_watches.count - 1)
            return false;

        plat_state->file_watches[watch_id].file_path.free_data();
        plat_state->file_watches.remove_at(watch_id);

        return true;
    }

    void update_file_watches()
    {

        for (uint32 i = 0; i < plat_state->file_watches.count; i++)
        {
            FileWatch* watch = &plat_state->file_watches[i];

            WIN32_FIND_DATAA find_data;
            HANDLE file_handle = FindFirstFileA(watch->file_path.c_str(), &find_data);
            if (file_handle == INVALID_HANDLE_VALUE)
            {
                EventData e_data = {};
                e_data.ui32[0] = i;
                Event::event_fire(SystemEventCode::WATCHED_FILE_DELETED, 0, e_data);
                SHMINFOV("Watched file %s has been deleted.", watch->file_path.c_str());
                unregister_file_watch(i);
                continue;
            }
            if (!FindClose(file_handle))
                continue;

            if (CompareFileTime(&find_data.ftLastWriteTime, &watch->last_write_timestamp) > 0)
            {
                watch->last_write_timestamp = find_data.ftLastWriteTime;
                EventData e_data = {};
                e_data.ui32[0] = i;
                Event::event_fire(SystemEventCode::WATCHED_FILE_WRITTEN, 0, e_data);
            }
        }

    }

    void init_console()
    {
        AllocConsole();
        return;
    }

    void console_write(const char* message, uint8 color)
    {
        HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        static uint8 levels[6] = { 64, 4, 6, 2, 1, 8 };
        SetConsoleTextAttribute(console_handle, levels[color]);
        OutputDebugStringA(message);
        uint64 length = strlen(message);
        DWORD number_written = 0;
        WriteConsoleA(console_handle, message, (DWORD)length, &number_written, 0);
    }

    void console_write_error(const char* message, uint8 color)
    {
        HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
        static uint8 levels[6] = { 64, 4, 6, 2, 1, 8 };
        SetConsoleTextAttribute(console_handle, levels[color]);
        OutputDebugStringA(message);
        uint64 length = strlen(message);
        DWORD number_written = 0;
        WriteConsoleA(console_handle, message, (DWORD)length, &number_written, 0);
    }

    float64 get_absolute_time() {
        LARGE_INTEGER now_time;
        QueryPerformanceCounter(&now_time);
        return (float64)now_time.QuadPart * clock_frequency;
    }

    void sleep(uint32 ms) {
        Sleep(ms);
    }

    Math::Vec2i get_cursor_pos()
    {
        POINT cursor_pos;
        GetCursorPos(&cursor_pos);
        return { cursor_pos.x, cursor_pos.y };
    }

    void set_cursor_pos(int32 x, int32 y)
    {
        SetCursorPos(x, y);
    }

    bool32 clip_cursor(bool32 clip)
    {
        ShowCursor(!clip);

        Math::Vec2i client_center = { (int32)plat_state->client_width / 2, (int32)plat_state->client_height / 2 };

        if (clip)
        {           
            RECT window_rect = {};
            GetWindowRect(plat_state->hwnd, &window_rect);
            if (!ClipCursor(&window_rect))
            {
                SHMDEBUG("ClipCursor failed!");
                return false;
            }                    
        }
        else
        {
            ClipCursor(0);
        }

        set_cursor_pos(client_center.x + (int32)plat_state->client_x, client_center.y + (int32)plat_state->client_y);
        Input::process_mouse_move(client_center.x, client_center.y);

        return true;
    }

    WindowHandle get_window_handle()
    {
        WindowHandle ret;
        ret.h_instance = plat_state->h_instance;
        ret.h_wnd = plat_state->hwnd;
        return ret;
    }

    bool32 load_dynamic_library(const char* name, const char* filename, DynamicLibrary* out_lib)
    {

        HMODULE lib = LoadLibraryA(filename);
        if (!lib)
            return false;

        CString::copy(name, out_lib->name, sizeof(out_lib->name));
        CString::copy(name, out_lib->filename, MAX_FILEPATH_LENGTH);
        out_lib->handle = lib;

        SHMINFOV("Loaded dynamic library '%s'", name);

        return true;

    }

    bool32 unload_dynamic_library(DynamicLibrary* lib)
    {
      
        if (!FreeLibrary((HMODULE)lib->handle))
            return false;

        SHMINFOV("Unloaded dynamic library '%s'", lib->name); 

        return true;

    }

    bool32 load_dynamic_library_function(DynamicLibrary* lib, const char* name, void** out_function)
    {
        FARPROC fp = GetProcAddress((HMODULE)lib->handle, name);
        if (!fp)
            return false;

        *out_function = (void*)fp;

        return true;
    }

    void message_box(const char* prompt, const char* message)
    {
        MessageBoxA(plat_state->hwnd, message, prompt, MB_OK);
    }

    void set_window_text(const char* s)
    {
        SetWindowTextA(plat_state->hwnd, s);
    }

    static bool32 win32_process_message_fast(uint32 msg, WPARAM w_param, LPARAM l_param)
    {
        switch (msg) {
        case WM_MOVE:
        {
            plat_state->client_x = LOWORD(l_param);
            plat_state->client_y = HIWORD(l_param);
            break;
        }
        case WM_INPUT:
        {

            static RAWINPUT input_buffer[1];
            UINT input_size = sizeof(input_buffer);

            GetRawInputData((HRAWINPUT)l_param, RID_INPUT, input_buffer, &input_size, sizeof(RAWINPUTHEADER));

            if (input_buffer[0].header.dwType == RIM_TYPEMOUSE)
            {
                int32 x = input_buffer[0].data.mouse.lLastX;
                int32 y = input_buffer[0].data.mouse.lLastY;
                Input::process_mouse_internal_move(x, y);
            }
            break;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            // Key pressed/released
            bool8 pressed = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            KeyCode::Value key = (KeyCode::Value)w_param;

            Input::process_key(key, pressed);
            break;
        }
        case WM_MOUSEMOVE:
        {
            // Mouse move
            int32 x = LOWORD(l_param);
            int32 y = HIWORD(l_param);
            if (!Input::is_cursor_clipped())
                Input::process_mouse_move(x, y);
            break;
        }
        case WM_MOUSEWHEEL:
        {
            int32 delta = GET_WHEEL_DELTA_WPARAM(w_param);
            if (delta != 0) {
                // Flatten the input to an OS-independent (-1, 1)
                delta = (delta < 0) ? -1 : 1;
                Input::process_mouse_scroll(delta);
            }
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        {
            bool8 pressed = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
            MouseButton::Value button = MouseButton::BUTTON_MAX_BUTTONS;
            switch (msg)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                button = MouseButton::LMB;
                break;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                button = MouseButton::RMB;
                break;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                button = MouseButton::MMB;
                break;
            }

            Input::process_mousebutton(button, pressed);
            break;
        }
        case WM_NCMOUSEMOVE:
        case WM_NCMOUSELEAVE:
        case WM_MOUSELEAVE:
        {
            return true;
        }
        default:
        {
            return false;
        }
        }

        return true;
    }

    LRESULT CALLBACK win32_process_message(HWND hwnd, uint32 msg, WPARAM w_param, LPARAM l_param) 
    {

        switch (msg) {
        case WM_ERASEBKGND:
        {
            // Notify the OS that erasing will be handled by the application to prevent flicker.
            return 1;
        }       
        case WM_CLOSE:
        {
            Event::event_fire(SystemEventCode::APPLICATION_QUIT, 0, {});
            return 1;
        }        
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }  
        case WM_SIZE:
        {
            // Get the updated size.
            RECT r;
            GetClientRect(plat_state->hwnd, &r);
            plat_state->client_width = r.right - r.left;
            plat_state->client_height = r.bottom - r.top;

            EventData e = {};
            e.ui32[0] = plat_state->client_width;
            e.ui32[1] = plat_state->client_height;
            Event::event_fire(SystemEventCode::WINDOW_RESIZED, 0, e);
            break;
        }
        case WM_NCHITTEST:
        {
            return DefWindowProcA(hwnd, msg, w_param, l_param);
        }
        case WM_SETTEXT:
        {
            return DefWindowProcA(hwnd, msg, w_param, l_param);
        }
        case WM_SETCURSOR:
        {
            return DefWindowProcA(hwnd, msg, w_param, l_param);
        }
        case 174: //???
        {
            return DefWindowProcA(hwnd, msg, w_param, l_param);
        }
        case WM_GETICON:
        {
            return DefWindowProcA(hwnd, msg, w_param, l_param);
        }
        default:
        {
            return DefWindowProcA(hwnd, msg, w_param, l_param);
        }
        }

        return 0;
    }

}

#endif