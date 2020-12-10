#pragma once
#include "maths.h"

struct Font;

struct Panel {
    char *name;
    Vector2 pos;
    float width;

    Vector2 item_pos;
};

namespace ui {
    Panel start_panel(char *name, Vector2 pos, float width);
    Panel start_panel(char *name, float x, float y, float width);
    void end_panel(Panel *panel);
    Vector4 get_panel_rect(Panel *panel);

    void end_frame();

    bool add_toggle(Panel *panel, char *label, bool *state);
    bool add_toggle(Panel *panel, char *label, int *state);
    bool add_slider(Panel *panel, char *label, float *pos, float min, float max);
    bool add_slider(Panel *panel, char *label, int *pos, int min, int max);
    bool add_combobox(Panel *panel, char *label, char **values, int value_count, int *selected_value, bool *expanded);
    bool add_function_plot(Panel *panel, char *label, float *x, float *y, int point_count, float *select_x, float select_y);
    bool add_textbox(Panel *panel, char *label, char *text, int buffer_size, int *cursor_position);

    // UI looks control functions.
    void set_background_opacity(float opacity);

    // These functions enable turning UI on/off as a listener for inputs
    void set_input_responsive(bool is_responsive);
    bool is_input_responsive();

    bool is_registering_input();
}

#ifdef CPPLIB_UI_IMPL
#include "ui.cpp"
#endif