#pragma once

#include "Defines.hpp"
#include "utility/Math.hpp"
#include "Subsystems.hpp"

namespace MouseButton
{
    enum
    {
        /** @brief The left mouse button */
        LMB,
        /** @brief The right mouse button */
        RMB,
        /** @brief The middle mouse button (typically the wheel) */
        MMB,
        BUTTON_MAX_BUTTONS
    };
    typedef uint8 Value;
}


namespace KeyCode
{
    enum
    {
        /** @brief The backspace key. */
        Backspace = 0x08,      
        /** @brief The tab key. */
        Tab = 0x09,
        /** @brief The enter key. */
        Enter = 0x0D,
        /** @brief The shift key. */
        Shift = 0x10,
        /** @brief The Control/Ctrl key. */
        Control = 0x11,
        /** @brief The Alt key. */
        Alt = 0x12,

        /** @brief The pause key. */
        Pause = 0x13,
        /** @brief The Caps Lock key. */
        Capital = 0x14,

        /** @brief The Escape key. */
        Escape = 0x1B,

        Convert = 0x1C,
        NonConvert = 0x1D,
        Accept = 0x1E,
        ModeChange = 0x1F,

        /** @brief The spacebar key. */
        Space = 0x20,
        /** @brief The page up key. */
        PageUp = 0x21,
        /** @brief The page down key. */
        PageDown = 0x22,
        /** @brief The end key. */
        End = 0x23,
        /** @brief The home key. */
        Home = 0x24,
        /** @brief The left arrow key. */
        Left = 0x25,
        /** @brief The up arrow key. */
        Up = 0x26,
        /** @brief The right arrow key. */
        Right = 0x27,
        /** @brief The down arrow key. */
        Down = 0x28,
        Select = 0x29,
        Print = 0x2A,
        Execute = 0x2B,
        /** @brief The Print Screen key. */
        PrintScreen = 0x2C,
        /** @brief The insert key. */
        Insert = 0x2D,
        /** @brief The delete key. */
        Delete = 0x2E,
        Help = 0x2F,

        /** @brief The 0 key */
        Num_0 = 0x30,
        /** @brief The 1 key */
        Num_1 = 0x31,
        /** @brief The 2 key */
        Num_2 = 0x32,
        /** @brief The 3 key */
        Num_3 = 0x33,
        /** @brief The 4 key */
        Num_4 = 0x34,
        /** @brief The 5 key */
        Num_5 = 0x35,
        /** @brief The 6 key */
        Num_6 = 0x36,
        /** @brief The 7 key */
        Num_7 = 0x37,
        /** @brief The 8 key */
        Num_8 = 0x38,
        /** @brief The 9 key */
        Num_9 = 0x39,

        /** @brief The A key. */
        A = 0x41,
        /** @brief The B key. */
        B = 0x42,
        /** @brief The C key. */
        C = 0x43,
        /** @brief The D key. */
        D = 0x44,
        /** @brief The E key. */
        E = 0x45,
        /** @brief The F key. */
        F = 0x46,
        /** @brief The G key. */
        G = 0x47,
        /** @brief The H key. */
        H = 0x48,
        /** @brief The I key. */
        I = 0x49,
        /** @brief The J key. */
        J = 0x4A,
        /** @brief The K key. */
        K = 0x4B,
        /** @brief The L key. */
        L = 0x4C,
        /** @brief The M key. */
        M = 0x4D,
        /** @brief The N key. */
        N = 0x4E,
        /** @brief The O key. */
        O = 0x4F,
        /** @brief The P key. */
        P = 0x50,
        /** @brief The Q key. */
        Q = 0x51,
        /** @brief The R key. */
        R = 0x52,
        /** @brief The S key. */
        S = 0x53,
        /** @brief The T key. */
        T = 0x54,
        /** @brief The U key. */
        U = 0x55,
        /** @brief The V key. */
        V = 0x56,
        /** @brief The W key. */
        W = 0x57,
        /** @brief The X key. */
        X = 0x58,
        /** @brief The Y key. */
        Y = 0x59,
        /** @brief The Z key. */
        Z = 0x5A,

        /** @brief The left Windows/Super key. */
        LSuper = 0x5B,
        /** @brief The right Windows/Super key. */
        RSuper = 0x5C,
        /** @brief The applicatons key. */
        Apps = 0x5D,

        /** @brief The sleep key. */
        Sleep = 0x5F,

        /** @brief The numberpad 0 key. */
        Numpad_0 = 0x60,
        /** @brief The numberpad 1 key. */
        Numpad_1 = 0x61,
        /** @brief The numberpad 2 key. */
        Numpad_2 = 0x62,
        /** @brief The numberpad 3 key. */
        Numpad_3 = 0x63,
        /** @brief The numberpad 4 key. */
        Numpad_4 = 0x64,
        /** @brief The numberpad 5 key. */
        Numpad_5 = 0x65,
        /** @brief The numberpad 6 key. */
        Numpad_6 = 0x66,
        /** @brief The numberpad 7 key. */
        Numpad_7 = 0x67,
        /** @brief The numberpad 8 key. */
        Numpad_8 = 0x68,
        /** @brief The numberpad 9 key. */
        Numpad_9 = 0x69,
        /** @brief The numberpad multiply key. */
        NumpadMult = 0x6A,
        /** @brief The numberpad add key. */
        NumpadAdd = 0x6B,
        /** @brief The numberpad separator key. */
        NumpadSeparator = 0x6C,
        /** @brief The numberpad subtract key. */
        NumpadSubtract = 0x6D,
        /** @brief The numberpad decimal key. */
        NumpadDecimal = 0x6E,
        /** @brief The numberpad divide key. */
		NumpadDivide = 0x6F,

		/** @brief The F1 key. */
		F1 = 0x70,
		/** @brief The F2 key. */
		F2 = 0x71,
		/** @brief The F3 key. */
		F3 = 0x72,
		/** @brief The F4 key. */
		F4 = 0x73,
		/** @brief The F5 key. */
		F5 = 0x74,
		/** @brief The F6 key. */
		F6 = 0x75,
		/** @brief The F7 key. */
		F7 = 0x76,
		/** @brief The F8 key. */
		F8 = 0x77,
		/** @brief The F9 key. */
		F9 = 0x78,
		/** @brief The F10 key. */
		F10 = 0x79,
		/** @brief The F11 key. */
		F11 = 0x7A,
		/** @brief The F12 key. */
		F12 = 0x7B,
		/** @brief The F13 key. */
		F13 = 0x7C,
		/** @brief The F14 key. */
		F14 = 0x7D,
		/** @brief The F15 key. */
		F15 = 0x7E,
		/** @brief The F16 key. */
		F16 = 0x7F,
		/** @brief The F17 key. */
		F17 = 0x80,
		/** @brief The F18 key. */
		F18 = 0x81,
		/** @brief The F19 key. */
		F19 = 0x82,
		/** @brief The F20 key. */
		F20 = 0x83,
		/** @brief The F21 key. */
		F21 = 0x84,
		/** @brief The F22 key. */
		F22 = 0x85,
		/** @brief The F23 key. */
		F23 = 0x86,
		/** @brief The F24 key. */
		F24 = 0x87,

		/** @brief The number lock key. */
		Numlock = 0x90,

		/** @brief The scroll lock key. */
		Scroll = 0x91,

		/** @brief The numberpad equal key. */
		NumpadEquals = 0x92,

		/** @brief The left shift key. */
		LShift = 0xA0,
		/** @brief The right shift key. */
		RShift = 0xA1,
		/** @brief The left control key. */
		LControl = 0xA2,
		/** @brief The right control key. */
		RControl = 0xA3,
		/** @brief The left alt key. */
		LAlt = 0xA4,
		/** @brief The right alt key. */
		RAlt = 0xA5,

        /** @brief The semicolon key. */
        Semicolon = 0xBA,
		/** @brief The equals key. */
		Equals = 0xBB,
		/** @brief The comma key. */
		Comma = 0xBC,
		/** @brief The minus key. */
		Minus = 0xBD,
		/** @brief The dot key. */
		Dot = 0xBE,
        /** @brief The forward Slash key. */
        Slash = 0xBF,

        /** @brief The Accent key. */
        Accent = 0xC0,

        /** @brief The opening bracket key. */
        BracketOpening = 0xDB,
        /** @brief The Backslash key. */
        Backslash = 0xDC,
        /** @brief The closing bracket key. */
        BracketClosing = 0xDD,
        /** @brief The Apostrophe key. */
        Apostrophe = 0xDE,
       
        MAX_KEYS
    };
    typedef uint8 Value;
}



struct FrameData;

namespace Input
{
    bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* confige);
    void system_shutdown(void* state);

    struct Keymap;
    SHMAPI void push_keymap(const Keymap* map);
    SHMAPI void pop_keymap();
    SHMAPI void clear_keymaps();

    void frame_start();
    void frame_end(const FrameData* frame_data);

    SHMAPI bool8 is_key_down(KeyCode::Value key);
    SHMAPI bool8 is_key_up(KeyCode::Value key);
    SHMAPI bool8 was_key_down(KeyCode::Value key);
    SHMAPI bool8 was_key_up(KeyCode::Value key);   

    SHMAPI bool8 is_mousebutton_down(MouseButton::Value button);
    SHMAPI bool8 is_mousebutton_up(MouseButton::Value button);
    SHMAPI bool8 was_mousebutton_down(MouseButton::Value button);
    SHMAPI bool8 was_mousebutton_up(MouseButton::Value button);
    SHMAPI Math::Vec2i get_mouse_position();
    SHMAPI Math::Vec2i get_previous_mouse_position();
    SHMAPI Math::Vec2i get_internal_mouse_offset();

    void process_key(KeyCode::Value key, bool8 pressed);
    void process_mousebutton(MouseButton::Value button, bool8 pressed);
    void process_mouse_move(int32 x, int32 y);
    void process_mouse_internal_move(int32 x_offset, int32 y_offset);
    void process_mouse_scroll(int32 delta);

    SHMAPI bool8 clip_cursor();
    SHMAPI bool8 is_cursor_clipped();

    SHMINLINE SHMAPI bool8 key_pressed(KeyCode::Value key)
    {
        return (is_key_down(key) && was_key_up(key));
    }
}
