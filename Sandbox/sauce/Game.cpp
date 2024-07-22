#include "Game.hpp"
#include "Defines.hpp"
#include "containers/Darray.hpp"

#include <core/Logging.hpp>
#include <core/Input.hpp>
#include <core/Event.hpp>
#include <utility/Utility.hpp>
#include <utility/String.hpp>
#include <renderer/RendererTypes.hpp>
#include <utility/Sort.hpp>

bool32 game_init(Game* game_inst) 
{
    GameState* state = (GameState*)game_inst->state;

    state->world_camera = CameraSystem::get_default_camera();
    state->world_camera->set_position({ 10.5f, 5.0f, 9.5f });

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

    if (Input::key_pressed(Keys::KEY_L))
    {
        Event::event_fire(SystemEventCode::DEBUG1, 0, {});
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
            state->world_camera->yaw(1.0f * delta_time);
        }
        if (Input::is_key_down(Keys::KEY_RIGHT))
        {
            state->world_camera->yaw(-1.0f * delta_time);
        }
        if (Input::is_key_down(Keys::KEY_UP))
        {
            state->world_camera->pitch(1.0f * delta_time);
        }
        if (Input::is_key_down(Keys::KEY_DOWN))
        {
            state->world_camera->pitch(-1.0f * delta_time);
        }
    }
    else
    {
        Math::Vec2i mouse_offset = Input::get_internal_mouse_offset();
        float32 mouse_sensitivity = 3.0f;
        state->world_camera->yaw(-mouse_offset.x * delta_time * mouse_sensitivity);
        state->world_camera->pitch(-mouse_offset.y * delta_time * mouse_sensitivity);
    }
    
    float32 temp_move_speed = 50.0f;

    if (Input::is_key_down(Keys::KEY_W))
    {
        state->world_camera->move_forward(temp_move_speed * delta_time);
    }
    if (Input::is_key_down(Keys::KEY_S))
    {
        state->world_camera->move_backward(temp_move_speed * delta_time);
    }
    if (Input::is_key_down(Keys::KEY_D))
    {
        state->world_camera->move_right(temp_move_speed * delta_time);
    }
    if (Input::is_key_down(Keys::KEY_A))
    {
        state->world_camera->move_left(temp_move_speed * delta_time);
    }
    if (Input::is_key_down(Keys::KEY_SPACE))
    {
        state->world_camera->move_up(temp_move_speed * delta_time);
    }
    if (Input::is_key_down(Keys::KEY_SHIFT))
    {
        state->world_camera->move_down(temp_move_speed * delta_time);
    }

    return true;
}

bool32 game_render(Game* game_inst, float32 delta_time) 
{
    return true;
}

void game_on_resize(Game* game_inst, uint32 width, uint32 height) 
{
}