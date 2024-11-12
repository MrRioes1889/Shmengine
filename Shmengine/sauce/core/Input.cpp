#include "Input.hpp"
#include "Event.hpp"
#include "Logging.hpp"
#include "Keymap.hpp"
#include "platform/Platform.hpp"
#include "memory/LinearAllocator.hpp"
#include "core/FrameData.hpp"
#include "containers/Stack.hpp"

namespace Input
{

    struct KeyboardState
    {
        bool8 keys[256];
    };

    struct MouseButtonsState
    {
        bool8 buttons[MouseButton::BUTTON_MAX_BUTTONS];
    };

    struct InputState
    {
        Stack<Keymap> keymap_stack;

        KeyboardState keyboard_cur;
        KeyboardState keyboard_prev;

        MouseButtonsState mouse_cur;
        MouseButtonsState mouse_prev;

        Math::Vec2i mouse_pos;
        Math::Vec2i prev_mouse_pos;

        Math::Vec2i mouse_internal_offset;

        bool8 cursor_clipped;
        bool8 initialized;
    };

    static InputState* system_state;

    bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
    {

        system_state = (InputState*)allocator_callback(allocator, sizeof(InputState));

        system_state->keymap_stack.init(2, 0);

        system_state->cursor_clipped = false;
        system_state->mouse_internal_offset = {};

        system_state->initialized = true;
        SHMINFO("Input subsystem initialized!");
        return system_state->initialized;

    }

    void system_shutdown(void* state)
    {
        system_state->initialized = false;
    }

    void push_keymap(const Keymap* map)
    {
        system_state->keymap_stack.push(*map);
    }

    void pop_keymap()
    {
        system_state->keymap_stack.pop();
    }

    void clear_keymaps()
    {
        system_state->keymap_stack.clear();
    }

    static bool32 check_modifiers(KeymapModifierFlags::Value modifiers)
    {
        if (modifiers & KeymapModifierFlags::SHIFT)
        {
            if (!is_key_down(KeyCode::SHIFT) && !is_key_down(KeyCode::LSHIFT) && !is_key_down(KeyCode::RSHIFT))
                return false;
        }

        if (modifiers & KeymapModifierFlags::CONTROL)
        {
            if (!is_key_down(KeyCode::CONTROL) && !is_key_down(KeyCode::LCONTROL) && !is_key_down(KeyCode::RCONTROL))
                return false;
        }

        if (modifiers & KeymapModifierFlags::ALT)
        {
            if (!is_key_down(KeyCode::LALT))
                return false;
        }

        if (modifiers & KeymapModifierFlags::ALT_GR)
        {
            if (!is_key_down(KeyCode::RALT))
                return false;
        }

        return true;
    }

    void frame_start()
    {
        if (!system_state->initialized)
            return;

        //system_state->mouse_pos = Platform::get_cursor_pos();
    }

    void frame_end(const FrameData* frame_data)
    {
        if (!system_state->initialized)
            return;

        for (KeyCode::Value key = 0; key < KeyCode::MAX_KEYS; key++)
        {
            if (!(is_key_down(key) && was_key_down(key)))
                continue;

            for (int32 m = (int32)system_state->keymap_stack.count - 1; m >= 0; m--)
            {
                Keymap* map = &system_state->keymap_stack[m];
                KeymapBinding* binding = &map->entries[key].bindings[0];
                bool32 unset = false;

                while (binding)
                {
                    if (binding->type == KeymapBindingType::UNSET)
                    {
                        unset = true;
                        break;
                    }
                    else if (binding->type == KeymapBindingType::HOLD)
                    {
                        if (binding->callback && check_modifiers(binding->modifiers))
                            binding->callback(key, binding->type, binding->modifiers, binding->user_data);
                    }

                    binding = binding->next;
                }

                if (unset || map->overrides_all)
                    break;
            }
        }

        system_state->keyboard_prev = system_state->keyboard_cur;
        system_state->mouse_prev = system_state->mouse_cur;

        system_state->prev_mouse_pos = system_state->mouse_pos;
        system_state->mouse_internal_offset = { 0, 0 };
    }

    void process_key(KeyCode::Value key, bool8 pressed)
    {
        if (system_state->keyboard_cur.keys[key] != pressed)
        {
            system_state->keyboard_cur.keys[key] = pressed;

            for (int32 m = (int32)system_state->keymap_stack.count - 1; m >= 0; m--)
            {
                Keymap* map = &system_state->keymap_stack[m];
                KeymapBinding* binding = &map->entries[key].bindings[0];
                bool32 unset = false;

                while (binding)
                {
                    if (binding->type == KeymapBindingType::UNSET)
                    {
                        unset = true;
                        break;
                    }
                    else if (pressed && binding->type == KeymapBindingType::PRESS)
                    {
                        if (binding->callback && check_modifiers(binding->modifiers))
                            binding->callback(key, binding->type, binding->modifiers, binding->user_data);
                    }
                    else if (!pressed && binding->type == KeymapBindingType::RELEASE)
                    {
                        if (binding->callback && check_modifiers(binding->modifiers))
                            binding->callback(key, binding->type, binding->modifiers, binding->user_data);
                    }

                    binding = binding->next;
                }

                if (unset || map->overrides_all)
                    break;
            }

            EventData e;
            e.ui32[0] = key;
            Event::event_fire(pressed ? (uint16)SystemEventCode::KEY_PRESSED : (uint16)SystemEventCode::KEY_RELEASED, 0, e);
        }
    }

    void process_mousebutton(MouseButton::Value button, bool8 pressed)
    {
        if (system_state->mouse_cur.buttons[button] != pressed)
        {
            system_state->mouse_cur.buttons[button] = pressed;

            EventData e;
            e.ui32[0] = button;
            Event::event_fire(pressed ? (uint16)SystemEventCode::BUTTON_PRESSED : (uint16)SystemEventCode::BUTTON_RELEASED, 0, e);
        }
    }

    void process_mouse_move(int32 x, int32 y)
    {
        system_state->mouse_pos.x = x;
        system_state->mouse_pos.y = y;

        if (!system_state->cursor_clipped && (system_state->mouse_pos.x != x || system_state->mouse_pos.y != y))
        {
            EventData e;
            e.i32[0] = x;
            e.i32[1] = y;
            Event::event_fire(SystemEventCode::MOUSE_MOVED_, 0, e);
        }
    }

    void process_mouse_internal_move(int32 x_offset, int32 y_offset)
    {
        system_state->mouse_internal_offset = { x_offset, y_offset };

        EventData e;
        e.i32[0] = x_offset;
        e.i32[1] = y_offset;
        Event::event_fire(SystemEventCode::MOUSE_INTERNAL_MOVED, 0, e);
    }

    void process_mouse_scroll(int32 delta)
    {
        if (delta)
        {
            EventData e;
            e.i32[0] = delta;
            Event::event_fire(SystemEventCode::MOUSE_SCROLL, 0, e);
        }
    }

    bool32 clip_cursor()
    {
        system_state->cursor_clipped = (!system_state->cursor_clipped);
        return Platform::clip_cursor(system_state->cursor_clipped);
    }

    bool32 is_key_down(KeyCode::Value key)
    {
        return system_state->keyboard_cur.keys[key];
    }

    bool32 is_key_up(KeyCode::Value key)
    {
        return system_state->keyboard_cur.keys[key] == false;
    }

    bool32 was_key_down(KeyCode::Value key)
    {
        return system_state->keyboard_prev.keys[key];
    }

    bool32 was_key_up(KeyCode::Value key)
    {
        return system_state->keyboard_prev.keys[key] == false;
    }

    bool32 is_mousebutton_down(MouseButton::Value button)
    {
        return system_state->mouse_cur.buttons[button];
    }

    bool32 is_mousebutton_up(MouseButton::Value button)
    {
        return system_state->mouse_cur.buttons[button] == false;
    }

    bool32 was_mousebutton_down(MouseButton::Value button)
    {
        return system_state->mouse_prev.buttons[button];
    }

    bool32 was_mousebutton_up(MouseButton::Value button)
    {
        return system_state->mouse_cur.buttons[button] == false;
    }

    Math::Vec2i get_mouse_position()
    {
        return system_state->mouse_pos;
    }

    Math::Vec2i get_previous_mouse_position()
    {
        return system_state->prev_mouse_pos;
    }

    Math::Vec2i get_internal_mouse_offset()
    {
        return system_state->mouse_internal_offset;
    }

    bool32 is_cursor_clipped()
    {
        return system_state->cursor_clipped;
    }

}
