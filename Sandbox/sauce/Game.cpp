#include "Game.hpp"
#include "Defines.hpp"
#include "containers/Darray.hpp"

#include <core/Logging.hpp>
#include <core/Input.hpp>
#include <core/Event.hpp>
#include <utility/Utility.hpp>
#include <utility/String.hpp>
#include <renderer/RendererFrontend.hpp>

static void recalculate_view_matrix(GameState* state)
{

    Math::Mat4 rotation = Math::mat_euler_xyz(state->camera_euler.x, state->camera_euler.y, state->camera_euler.z);
    Math::Mat4 translation = Math::mat_translation(state->camera_position);

    state->view = Math::mat_inverse(Math::mat_mul(rotation, translation));
    state->camera_view_dirty = false;

}

SHMINLINE void camera_pitch(GameState* state, float32 change)
{
    state->camera_euler.pitch += change;

    float32 limit = Math::deg_to_rad(89.0f);
    change = clamp(change, -limit, limit);
    state->camera_view_dirty = true;
}

SHMINLINE void camera_yaw(GameState* state, float32 change)
{
    state->camera_euler.yaw += change;
    state->camera_view_dirty = true;
}

SHMINLINE void camera_roll(GameState* state, float32 change)
{
    state->camera_euler.roll += change;
    state->camera_view_dirty = true;
}

bool32 game_init(Game* game_inst) 
{
    GameState* state = (GameState*)game_inst->state;

    state->camera_position = { 0.0f, 0.0f, 30.0f };
    state->camera_euler = VEC3_ZERO;
    state->camera_view_dirty = true;

    state->view = Math::mat_translation(state->camera_position);
    state->view = Math::mat_inverse(state->view);

    return true;
}

bool32 game_update(Game* game_inst, float32 delta_time) 
{
    GameState* state = (GameState*)game_inst->state;

    if (Input::key_pressed(Keys::KEY_C))
    {
        SHMDEBUG("Clipping/Unclipping cursor!");
        Input::clip_cursor();
    }

    if (Input::key_pressed(Keys::KEY_T))
    {
        SHMDEBUG("Swapping Texture!");
        Event::event_fire(SystemEventCode::DEBUG0, 0, {});
    }    

    if (Input::key_pressed(Keys::KEY_1)) {
        EventData data = {};
        data.i32[0] = Renderer::ViewMode::DEFAULT;
        Event::event_fire(SystemEventCode::SET_RENDER_MODE, game_inst, data);
    }

    if (Input::key_pressed(Keys::KEY_2)) {
        EventData data = {};
        data.i32[0] = Renderer::ViewMode::LIGHTING;
        Event::event_fire(SystemEventCode::SET_RENDER_MODE, game_inst, data);
    }

    if (Input::key_pressed(Keys::KEY_3)) {
        EventData data = {};
        data.i32[0] = Renderer::ViewMode::NORMALS;
        Event::event_fire(SystemEventCode::SET_RENDER_MODE, game_inst, data);
    } 

    if (!Input::is_cursor_clipped())
    {
        if (Input::is_key_down(Keys::KEY_LEFT))
        {
            camera_yaw(state, 1.0f * delta_time);
        }
        if (Input::is_key_down(Keys::KEY_RIGHT))
        {
            camera_yaw(state, -1.0f * delta_time);
        }
        if (Input::is_key_down(Keys::KEY_UP))
        {
            camera_pitch(state, 1.0f * delta_time);
        }
        if (Input::is_key_down(Keys::KEY_DOWN))
        {
            camera_pitch(state, -1.0f * delta_time);
        }
    }
    else
    {
        Math::Vec2i mouse_offset = Input::get_internal_mouse_offset();
        float32 mouse_sensitivity = 3.0f;
        camera_yaw(state, -mouse_offset.x * delta_time * mouse_sensitivity);
        camera_pitch(state, -mouse_offset.y * delta_time * mouse_sensitivity);
    }
    

    float32 temp_move_speed = 50.0f;
    Math::Vec3f velocity = VEC3_ZERO;
    Math::Vec3f forward = Math::mat_forward(state->view);
    Math::Vec3f right = Math::mat_right(state->view);
    Math::Vec3f up = Math::mat_up(state->view);

    if (Input::is_key_down(Keys::KEY_W))
    {
        velocity.z = temp_move_speed * delta_time;
    }
    if (Input::is_key_down(Keys::KEY_S))
    {
        velocity.z = -temp_move_speed * delta_time;
    }

    if (Input::is_key_down(Keys::KEY_D))
    {
        velocity.x = temp_move_speed * delta_time;
    }
    if (Input::is_key_down(Keys::KEY_A))
    {
        velocity.x = -temp_move_speed * delta_time;
    }

    if (Input::is_key_down(Keys::KEY_SPACE))
    {
        velocity.y = temp_move_speed * delta_time;
    }
    if (Input::is_key_down(Keys::KEY_SHIFT))
    {
        velocity.y = -temp_move_speed * delta_time;
    }

    if (velocity.x || velocity.y || velocity.z)
    {
        forward = forward * velocity.z;
        right = right * velocity.x;
        up = up * velocity.y;
        state->camera_position = state->camera_position + forward + right + up;
        state->camera_view_dirty = true;
    }
    

    if (state->camera_view_dirty)
        recalculate_view_matrix(state);

    Renderer::set_view(state->view, state->camera_position);
    return true;
}

bool32 game_render(Game* game_inst, float32 delta_time) 
{
    return true;
}

void game_on_resize(Game* game_inst, uint32 width, uint32 height) 
{
}