#include "Input.hpp"
#include "Event.hpp"
#include "Logging.hpp"
#include "platform/Platform.hpp"
#include "memory/LinearAllocator.hpp"

struct KeyboardState
{
    bool8 keys[256];
};

struct MouseButtonsState
{  
    bool8 buttons[Mousebuttons::BUTTON_MAX_BUTTONS];
};

struct InputState
{
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

bool32 Input::system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state)
{

    out_state = allocator_callback(sizeof(InputState));
    system_state = (InputState*)out_state;

    system_state->cursor_clipped = false;
    system_state->mouse_internal_offset = {};

    system_state->initialized = true;
    SHMINFO("Input subsystem initialized!");
    return system_state->initialized;

}

void Input::system_shutdown()
{
    system_state->initialized = false;
}

void Input::frame_start()
{
    if (!system_state->initialized)
        return;

    //system_state->mouse_pos = Platform::get_cursor_pos();
}

void Input::frame_end(float64 delta_time)
{
    if (!system_state->initialized)
        return;

    system_state->keyboard_prev = system_state->keyboard_cur;
    system_state->mouse_prev = system_state->mouse_cur;

    system_state->prev_mouse_pos = system_state->mouse_pos;
    system_state->mouse_internal_offset = { 0, 0 };
}

void Input::process_key(Keys key, bool8 pressed)
{
    if (system_state->keyboard_cur.keys[key] != pressed)
    {
        system_state->keyboard_cur.keys[key] = pressed;

        EventData e;
        e.ui32[0] = key;
        Event::event_fire(pressed ? (uint16)SystemEventCode::KEY_PRESSED : (uint16)SystemEventCode::KEY_RELEASED, 0, e);
    }
}

void Input::process_mousebutton(Mousebuttons button, bool8 pressed)
{
    if (system_state->mouse_cur.buttons[button] != pressed)
    {
        system_state->mouse_cur.buttons[button] = pressed;

        EventData e;
        e.ui32[0] = button;
        Event::event_fire(pressed ? (uint16)SystemEventCode::BUTTON_PRESSED : (uint16)SystemEventCode::BUTTON_RELEASED, 0, e);
    }
}

void Input::process_mouse_move(int32 x, int32 y)
{
    system_state->mouse_pos.x = x;
    system_state->mouse_pos.y = y;

    if (!system_state->cursor_clipped && (system_state->mouse_pos.x != x || system_state->mouse_pos.y != y))
    {       
        EventData e;
        e.i32[0] = x;
        e.i32[1] = y;
        Event::event_fire(SystemEventCode::MOUSE_MOVED, 0, e);
    }
}

void Input::process_mouse_internal_move(int32 x_offset, int32 y_offset)
{
    system_state->mouse_internal_offset = { x_offset, y_offset };

    EventData e;
    e.i32[0] = x_offset;
    e.i32[1] = y_offset;
    Event::event_fire(SystemEventCode::MOUSE_INTERNAL_MOVED, 0, e);
}

void Input::process_mouse_scroll(int32 delta)
{
    if (delta)
    {
        EventData e;
        e.i32[0] = delta;
        Event::event_fire(SystemEventCode::MOUSE_SCROLL, 0, e);
    }
}

bool32 Input::clip_cursor()
{    
    system_state->cursor_clipped = (!system_state->cursor_clipped);
    return Platform::clip_cursor(system_state->cursor_clipped);
}

bool32 Input::is_key_down(Keys key)
{
    return system_state->keyboard_cur.keys[key];
}

bool32 Input::is_key_up(Keys key)
{
    return system_state->keyboard_cur.keys[key] == false;
}

bool32 Input::was_key_down(Keys key)
{
    return system_state->keyboard_prev.keys[key];
}

bool32 Input::was_key_up(Keys key)
{
    return system_state->keyboard_prev.keys[key] == false;
}

bool32 Input::is_mousebutton_down(Mousebuttons button)
{
    return system_state->mouse_cur.buttons[button];
}

bool32 Input::is_mousebutton_up(Mousebuttons button)
{
    return system_state->mouse_cur.buttons[button] == false;
}

bool32 Input::was_mousebutton_down(Mousebuttons button)
{
    return system_state->mouse_prev.buttons[button];
}

bool32 Input::was_mousebutton_up(Mousebuttons button)
{
    return system_state->mouse_cur.buttons[button] == false;
}

Math::Vec2i Input::get_mouse_position()
{
    return system_state->mouse_pos;
}

Math::Vec2i Input::get_previous_mouse_position()
{
    return system_state->prev_mouse_pos;
}

Math::Vec2i Input::get_internal_mouse_offset()
{
    return system_state->mouse_internal_offset;
}

bool32 Input::is_cursor_clipped()
{
    return system_state->cursor_clipped;
}
