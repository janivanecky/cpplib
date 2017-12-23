#pragma once
#include "math2.h"
#include "platform.h"

namespace input
{
    void reset();
    void set_ui_active();
    void set_ui_inactive();
    bool ui_active();

    bool mouse_left_button_pressed();
    bool mouse_left_button_down();
    Vector2 mouse_position();
    Vector2 mouse_delta_position();

    void set_mouse_left_button_down();
    void set_mouse_left_button_up();
    void set_mouse_position(Vector2 position);

    void set_key_down(KeyCode code);
    void set_key_up(KeyCode code);
    bool key_pressed(KeyCode code);
}