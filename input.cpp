#include "input.h"

// Input state variables
bool mouse_lbutton_pressed   = false;
bool mouse_lbutton_down      = false;
Vector2 mouse_position_       = Vector2(-1,-1);
Vector2 mouse_delta_position_ = Vector2(0.0f,0.0f);

float mouse_scroll_delta_ = 0.0f;

static bool key_down_[100];
static bool key_pressed_[100];

static int characters_entered_count = 0;
static char character_buffer[100];

bool ui_active_ = false;

////////////////////////////
/// PUBLIC API
////////////////////////////

void input::reset()
{
    mouse_lbutton_pressed = false;
    mouse_delta_position_ = Vector2(0.0f, 0.0f);
    mouse_scroll_delta_ = 0.0f;
    memset(key_pressed_, 0, sizeof(bool) * 100);
    characters_entered_count = 0;
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

float input::mouse_scroll_delta()
{
    return mouse_scroll_delta_;
}

bool input::key_pressed(KeyCode code)
{
    return key_pressed_[code];
}

bool input::key_down(KeyCode code)
{
    return key_down_[code];
}

int input::characters_entered(char *buffer) {
    if(buffer) {
        memcpy(buffer, character_buffer, characters_entered_count);
    }
    return characters_entered_count;
}

void input::register_event(Event *event)
{
    switch(event->type)
    {
        case MOUSE_MOVE:
        {
            // Set mouse position.
            MouseMoveData *data = (MouseMoveData *)event->data;
            // Don't update delta on the first frame (initial position (-1, -1))
            Vector2 new_mouse_position = Vector2(data->x, data->y);
            if(mouse_position_.x > 0.0f && mouse_position_.y > 0.0f)
            {
                mouse_delta_position_ = new_mouse_position - mouse_position_;
            }
            mouse_position_ = new_mouse_position;
        }
        break;
        case MOUSE_LBUTTON_DOWN:
        {
            // Set left mouse button down.
            if(!mouse_lbutton_down) mouse_lbutton_pressed = true;
            mouse_lbutton_down = true;
        }
        break;
        case MOUSE_LBUTTON_UP:
        {
            // Set left mouse button up.
            mouse_lbutton_down = false;
        }
        break;
        case MOUSE_WHEEL:
        {
            // Set mouse scroll delta.
		    MouseWheelData *data = (MouseWheelData *)event->data;
            mouse_scroll_delta_ = data->delta;
        }
        break;
        case KEY_DOWN:
        {
            // Set key down.
            KeyPressedData *key_data = (KeyPressedData *)event->data;
            if(!key_down_[key_data->code]) key_pressed_[key_data->code] = true;
            key_down_[key_data->code] = true;
        }
        break;
        case KEY_UP:
        {
            // Set key up.
            KeyPressedData *key_data = (KeyPressedData *)event->data;
            key_down_[key_data->code] = false;
        }
        break;
        case CHAR_ENTERED:
        {
            // TODO: Maybe provide API for this
            character_buffer[characters_entered_count++] = event->data[0];
        }
    }
    
}