#include "input.h"

bool mouse_lbutton_pressed   = false;
bool mouse_lbutton_down      = false;
Vector2 mouse_position_       = Vector2(-1,-1);
Vector2 mouse_delta_position_ = Vector2(0.0f,0.0f);

static bool key_down_[100];
static bool key_pressed_[100];

bool ui_active_ = false;

void input::set_ui_active()
{
    ui_active_ = true;
}

void input::set_ui_inactive()
{
    ui_active_ = false;
}

bool input::ui_active()
{
    return ui_active_;
}

void input::reset()
{
    mouse_lbutton_pressed = false;
    mouse_delta_position_ = Vector2(0.0f, 0.0f);
    memset(key_pressed_, 0, sizeof(bool) * 100);
}   

bool input::mouse_left_button_pressed()
{
    return mouse_lbutton_pressed;
}

bool input::mouse_left_button_down()
{
    return mouse_lbutton_down;
}

Vector2 input::mouse_position()
{
    return mouse_position_;
}

Vector2 input::mouse_delta_position()
{
    return mouse_delta_position_;
}

void input::set_mouse_left_button_down()
{
    if(!mouse_lbutton_down) mouse_lbutton_pressed = true;
    mouse_lbutton_down = true;
}

void input::set_mouse_left_button_up()
{
    mouse_lbutton_down = false;
}

void input::set_mouse_position(Vector2 position)
{
    if(mouse_position_.x > 0.0f && mouse_position_.y > 0.0f)
    {
        mouse_delta_position_ = position - mouse_position_;
    }
    mouse_position_ = position;
}

void input::set_key_down(KeyCode code)
{
    if(!key_down_[code]) key_pressed_[code] = true;
    key_down_[code] = true;
}

void input::set_key_up(KeyCode code)
{
    key_down_[code] = false;
}

bool input::key_pressed(KeyCode code)
{
    return key_pressed_[code];
}