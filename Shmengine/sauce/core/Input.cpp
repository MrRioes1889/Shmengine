#include "Input.hpp"
#include "Event.hpp"
#include "Logging.hpp"
#include "memory/LinearAllocator.hpp"

struct KeyboardState
{
    bool32 keys[256];
};

struct MouseState
{
    Math::Vec2i pos;
    bool32 buttons[Mousebuttons::BUTTON_MAX_BUTTONS];
};

struct InputState
{
    KeyboardState keyboard_cur;
    KeyboardState keyboard_prev;

    MouseState mouse_cur;
    MouseState mouse_prev;

    bool32 initialized = false;
};

static InputState* state;

bool32 Input::system_init(void* linear_allocator, void*& out_state)
{

    Memory::LinearAllocator* allocator = (Memory::LinearAllocator*)linear_allocator;
    out_state = Memory::linear_allocator_allocate(allocator, sizeof(InputState));
    state = (InputState*)out_state;
    *state = {};

    state->initialized = true;
    SHMINFO("Input subsystem initialized!");
    return state->initialized;
}

void Input::system_shutdown()
{
    state->initialized = false;
}

void Input::system_update(float64 delta_time)
{
    if (!state->initialized)
        return;

    state->keyboard_prev = state->keyboard_cur;
    state->mouse_prev = state->mouse_cur;
}

void Input::process_key(Keys key, bool32 pressed)
{
    if (state->keyboard_cur.keys[key] != pressed)
    {
        state->keyboard_cur.keys[key] = pressed;

        EventData e;
        e.ui32[0] = key;
        event_fire(pressed ? (uint16)EVENT_CODE_KEY_PRESSED : (uint16)EVENT_CODE_KEY_RELEASED, 0, e);
    }
}

void Input::process_mousebutton(Mousebuttons button, bool32 pressed)
{
    if (state->mouse_cur.buttons[button] != pressed)
    {
        state->mouse_cur.buttons[button] = pressed;

        EventData e;
        e.ui32[0] = button;
        event_fire(pressed ? (uint16)EVENT_CODE_BUTTON_PRESSED : (uint16)EVENT_CODE_BUTTON_RELEASED, 0, e);
    }
}

void Input::process_mouse_move(int32 x, int32 y)
{
    if (state->mouse_cur.pos.x != x || state->mouse_cur.pos.y != y)
    {
        state->mouse_cur.pos.x = x;
        state->mouse_cur.pos.y = y;

        EventData e;
        e.i32[0] = x;
        e.i32[1] = y;
        event_fire(EVENT_CODE_MOUSE_MOVED, 0, e);
    }
}

void Input::process_mouse_scroll(int32 delta)
{
    if (delta)
    {
        EventData e;
        e.i32[0] = delta;
        event_fire(EVENT_CODE_MOUSE_SCROLL, 0, e);
    }
}

SHMAPI bool32 Input::is_key_down(Keys key)
{
    return state->keyboard_cur.keys[key];
}

SHMAPI bool32 Input::is_key_up(Keys key)
{
    return state->keyboard_cur.keys[key] == false;
}

SHMAPI bool32 Input::was_key_down(Keys key)
{
    return state->keyboard_prev.keys[key];
}

SHMAPI bool32 Input::was_key_up(Keys key)
{
    return state->keyboard_prev.keys[key] == false;
}

SHMAPI bool32 Input::is_mousebutton_down(Mousebuttons button)
{
    return state->mouse_cur.buttons[button];
}

SHMAPI bool32 Input::is_mousebutton_up(Mousebuttons button)
{
    return state->mouse_cur.buttons[button] == false;
}

SHMAPI bool32 Input::was_mousebutton_down(Mousebuttons button)
{
    return state->mouse_prev.buttons[button];
}

SHMAPI bool32 Input::was_mousebutton_up(Mousebuttons button)
{
    return state->mouse_cur.buttons[button] == false;
}

SHMAPI Math::Vec2i Input::get_mouse_position()
{
    return state->mouse_cur.pos;
}

SHMAPI Math::Vec2i Input::get_previous_mouse_position()
{
    return state->mouse_prev.pos;
}
