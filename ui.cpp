#include <cassert>
#include "ui.h"
#include "ui_draw.h"
#include "font.h"
#include "graphics.h"
#include "array.h"
#include "input.h"

#undef min
#undef max

/*

This section defines global UI parameters.

*/

// Width of individual items, does not include the item label.
const float ITEMS_WIDTH = 225.0f * 1.25f;
// Distance from start of the panel position to start of the items' position.
const float OUTER_PADDING = 15.0f;
// Vertical padding between items.
const float INNER_PADDING = 5.0f;
// Horizontal padding between the item and its label.
const float LABEL_PADDING = INNER_PADDING + 2.0f;
// All lines in the UI have this consistent width.
const float LINES_WIDTH = 2.0f;
// Vertical padding between start of the function plot item and the plot itself.
const float PLOT_BOX_VERTICAL_PADDING = 5.0f;
// Maximum length of any text.
const int MAX_TEXT_LENGTH = 100;

#undef COLOR_BACKGROUND // For some reason this is defined in WinUser.h
static Vector4 COLOR_BACKGROUND = Vector4(0.01f, 0.01f, 0.01f, .9f);
static Vector4 COLOR_LABEL      = Vector4(1.0f, 1.0f, 1.0f, 0.9f);

// Items that are not hot/active are faded.
const float INACTIVE_COLOR_MODIFIER = 0.8f;
const float ACTIVE_COLOR_MODIFIER   = 1.0f;

// API to change the background color opacity. 
void ui::set_background_opacity(float opacity) {
    COLOR_BACKGROUND.w = opacity;
}

// Invert the UI colors.
void ui::invert_colors() {
    COLOR_BACKGROUND.x = 1.0f - COLOR_BACKGROUND.x;
    COLOR_BACKGROUND.y = 1.0f - COLOR_BACKGROUND.y;
    COLOR_BACKGROUND.z = 1.0f - COLOR_BACKGROUND.z;

    COLOR_LABEL.x = 1.0f - COLOR_LABEL.x;
    COLOR_LABEL.y = 1.0f - COLOR_LABEL.y;
    COLOR_LABEL.z = 1.0f - COLOR_LABEL.z;
}

/*

This section handles communication of input state between UI and application.

-   If UI is "input_responsive" (can be checked with `ui::is_input_responsive` call),
    the UI can react to user input. Application using UI can set this responsivness
    (using `ui::set_input_responsive` call) in case application should have input priority.
-   If UI is "registering_input" (can be checekd with `ui::is_registering_input` call),
    this means that UI state is being changed by user at the moment. This can be used
    by application to determine that some of its inputs should be ignored.

*/

bool is_input_responsive_ = true;
bool is_registering_input_ = false;

void ui::set_input_responsive(bool is_responsive) {
    is_input_responsive_ = is_responsive;
}

bool ui::is_input_responsive() {
    return is_input_responsive_;
}

bool ui::is_registering_input() {
    return is_registering_input_;
}

/*

This section defines allocator for per-frame temporary data.

*/

const int INITIAL_POOL_SIZE = 1024 * 1024;
int pool_size = INITIAL_POOL_SIZE;
int pool_top = 0;
uint8_t *pool = (uint8_t *)malloc(pool_size);

void *alloc_temp(int bytes) {
    // Reallocate if pool too small.
    if(pool_top + bytes > pool_size) {
        pool_size = pool_size * 2;
        pool = (uint8_t *)realloc(pool, pool_size);
    }

    // Get return pointer.
    void *allocated_data_ptr = pool + pool_top;

    // "Allocate" the memory.
    pool_top += bytes;

    return allocated_data_ptr;
}

/*

This section defines a system for storing items (text/rects) to render.

`ui::end` is the only user-facing function which submits UI items for drawing.

*/

// All the info needed to render a single piece of text.
struct TextItem {
    Vector4 color;
    Vector2 pos;
    Vector2 origin = Vector2(0,0);
    char text[MAX_TEXT_LENGTH];
};

// All the info needed to render a single rectangle.
struct RectItem {
    Vector4 color;
    Vector2 pos;
    Vector2 size;
};

// All the info needed to render a single triangle.
struct TriangleItem {
    Vector4 color;
    Vector2 v1;
    Vector2 v2;
    Vector2 v3;
};

// All the info needed to render a single line.
struct LineItem {
    Vector4 color;
    Vector2 *points;
    int point_count;
    float width;
};

// Arrays for storing per-frame items.
const int INITIAL_ITEM_COUNT = 100;
Array<TextItem> text_items = array::get<TextItem>(INITIAL_ITEM_COUNT);
Array<RectItem> rect_items = array::get<RectItem>(INITIAL_ITEM_COUNT);
Array<RectItem> rect_items_bg = array::get<RectItem>(INITIAL_ITEM_COUNT);
Array<TriangleItem> triangle_items = array::get<TriangleItem>(INITIAL_ITEM_COUNT);
Array<LineItem> line_items = array::get<LineItem>(INITIAL_ITEM_COUNT);

// Helper function to store a rectangle for current frame.
void add_rect(Vector2 pos, Vector2 size, Vector4 color) {
    RectItem item = {color, pos, size};
    array::add(&rect_items, item);
}

void add_rect_bg(Vector2 pos, Vector2 size, Vector4 color) {
    RectItem item = {color, pos, size};
    array::add(&rect_items_bg, item);
}

void add_text(Vector2 pos, Vector4 color, Vector2 origin, char *fmt_string, ...) {
    TextItem item = {color, pos, origin};

    // Print formatted string into buffer.
    va_list args;
    va_start(args, fmt_string);
    vsnprintf(item.text, MAX_TEXT_LENGTH, fmt_string, args);
    va_end(args);

    array::add(&text_items, item);
}

// Helper functions to store a piece of text for current frame.
void add_text(Vector2 pos, char *text, int text_length, Vector4 color, Vector2 origin=Vector2(0, 0)) {
    TextItem item = {color, pos, origin};

    // Copy text string.
    if(text_length > MAX_TEXT_LENGTH) {
        text_length = MAX_TEXT_LENGTH;
    }
    memcpy(item.text, text, text_length);
    item.text[text_length] = 0;

    array::add(&text_items, item);
}

void add_text(Vector2 pos, char *text, Vector4 color, Vector2 origin=Vector2(0, 0)) {
    add_text(pos, text, int(strlen(text)), color, origin);
}

// Helper function to store a triangle for current frame.
void add_triangle(Vector2 v1, Vector2 v2, Vector2 v3, Vector4 color) {
    TriangleItem item = {color, v1, v2, v3};
    array::add(&triangle_items, item);
}

// Helper function to store a line for current frame.
void add_line(Vector2 *points, int point_count, float width, Vector4 color) {
    LineItem item = {color, points, point_count, width};
    array::add(&line_items, item);
}

// Submits items for rendering.
void ui::end_frame() {
    for(uint32_t i = 0; i < rect_items_bg.count; ++i) {
        RectItem *item = &rect_items_bg.data[i];
        ui_draw::draw_rect(item->pos, item->size.x, item->size.y, item->color);
    }

    for(uint32_t i = 0; i < rect_items.count; ++i) {
        RectItem *item = &rect_items.data[i];
        ui_draw::draw_rect(item->pos, item->size.x, item->size.y, item->color);
    }

    for(uint32_t i = 0; i < triangle_items.count; ++i) {
        TriangleItem *item = &triangle_items.data[i];
        ui_draw::draw_triangle(item->v1, item->v2, item->v3, item->color);
    }

    for(uint32_t i = 0; i < line_items.count; ++i) {
        LineItem *item = &line_items.data[i];
        ui_draw::draw_line(item->points, item->point_count, item->width, item->color);
    }

    for(uint32_t i = 0; i < text_items.count; ++i) {
        TextItem *item = &text_items.data[i];
        ui_draw::draw_text(item->text, item->pos, item->color, item->origin);
    }

    // Clear frame.
    array::reset(&rect_items_bg);
    array::reset(&rect_items);
    array::reset(&triangle_items);
    array::reset(&text_items);
    array::reset(&line_items);
    pool_top = 0;
}

/*

This section defines convenience functions used frequently for handling UI item state update/rendering.

*/

int32_t hash_string(char *string) {
    int32_t hash_value = 5381;
    while(*string) {
        hash_value = ((hash_value << 5) + hash_value) + *string;
        string++;
    }
    return hash_value + 1;
}

bool is_in_rect(Vector2 position, Vector2 rect_position, Vector2 rect_size) {
    return (
        position.x >= rect_position.x &&
        position.x <= rect_position.x + rect_size.x &&
        position.y >= rect_position.y &&
        position.y <= rect_position.y + rect_size.y
    );
}

float get_item_height() {
    return font::get_row_height(ui_draw::get_font());
}

void add_min_max_markers(Vector2 pos, float width, float height, Vector4 color) {
    Vector2 marker_size = Vector2(width, height);
    Vector2 min_marker_pos = pos + Vector2(-width, 0.0f);
    Vector2 max_marker_pos = pos + Vector2(ITEMS_WIDTH, 0.0f);
    add_rect(min_marker_pos, marker_size, color);
    add_rect(max_marker_pos, marker_size, color);
}

void delete_character(char *text, int text_length, int character_to_delete) {
    char *dst_ptr = text + character_to_delete - 1;
    char *src_ptr = dst_ptr + 1;
    int text_length_after_deleted_char = text_length - character_to_delete - 1;
    memcpy(dst_ptr, src_ptr, text_length_after_deleted_char);
    text[text_length - 1] = 0; // NULL terminating character.
}

float compute_item_width(float item_width, char *label) {
    float total_width = OUTER_PADDING * 2.0f; // Left and right padding.
    total_width += item_width;
    total_width += LABEL_PADDING;
    total_width += font::get_string_width(label, ui_draw::get_font());
    return total_width;
}

/*

This section handles UI panel manipulation (initialization etc.).

*/

Panel ui::start_panel(char *name, Vector2 pos) {
    Panel panel = {};

    panel.pos = pos;
    panel.width = OUTER_PADDING * 2.0f;
    panel.item_pos.x = OUTER_PADDING;
    panel.item_pos.y = get_item_height() + INNER_PADDING + OUTER_PADDING;
    panel.name = name;

    return panel;
}

Panel ui::start_panel(char *name, float x, float y) {
    return ui::start_panel(name, Vector2(x,y));
}

void ui::end_panel(Panel *panel) {
    Vector4 panel_rect = get_panel_rect(panel);
    add_rect_bg(
        Vector2(panel_rect.x, panel_rect.y),
        Vector2(panel_rect.z, panel_rect.w),
        COLOR_BACKGROUND
    );

    Vector2 title_pos = panel->pos + Vector2(OUTER_PADDING, OUTER_PADDING);
    add_text(title_pos, panel->name, COLOR_LABEL);
}

Vector4 ui::get_panel_rect(Panel *panel) {
    Vector4 result = Vector4(
        panel->pos.x, panel->pos.y,
        panel->width, panel->item_pos.y + OUTER_PADDING - INNER_PADDING
    );
    return result;
}

/*

This section manages hot/active items' states.

Hot item is one that user is potentially going to interact with (mouse is over).
Active item is one that user is interacting with.

*/

int32_t active_id = -1;
int32_t hot_id = -1;

bool is_hot(int32_t item_id) {
    return item_id == hot_id;
}

bool is_active(int32_t item_id) {
    return item_id == active_id;
}

void set_hot(int32_t item_id) {
    if(hot_id == -1) {
        hot_id = item_id;
    }
}

void set_active(int32_t item_id) {
    active_id = item_id;
    is_registering_input_ = true;
}

void unset_hot(int32_t item_id) {
    if(is_hot(item_id)) {
        hot_id = -1;
    }
}

void unset_active(int32_t item_id) {
    if(is_active(item_id)) {
        active_id = -1;
        is_registering_input_ = false;
    }
}

enum ActiveBehaviorType {
    BUTTON,         // Button loses active state the next frame after becoming active.
    PRESS_AND_HOLD, // Press and hold keeps active state while the mouse button is held down.
    STICKY          // Sticky loses active state only after another mouse press event outside of the item.
};

// NOTE: Sticky item is one which keeps being active even after mouse is released.
void update_hot_and_active(
    int32_t item_id, Vector2 item_pos, Vector2 item_size, ActiveBehaviorType active_behavior_type=BUTTON
) {
    // UI can be made unresponsive by the application in which case we don't want to update
    // the hot/active states based on inputs, just unset everything as hot/active.
    if(ui::is_input_responsive()) {
        Vector2 mouse_position = Vector2(input::mouse_position_x(), input::mouse_position_y());
        
        // Set hot state.
        if(is_in_rect(mouse_position, item_pos, item_size)) {
            set_hot(item_id);
        } else {
            unset_hot(item_id);
        }

        // Set active state.
        if(is_hot(item_id) && !is_active(item_id) && input::mouse_left_button_pressed()) {
            set_active(item_id);
        } else {
            // The condition for losing active state differs based on `active_beahvior_type`.
            bool lose_active_state = false;
            switch (active_behavior_type) {
                case BUTTON:
                lose_active_state = is_active(item_id);
                break;
                case PRESS_AND_HOLD:
                lose_active_state = !input::mouse_left_button_down();
                break;
                case STICKY:
                lose_active_state = !is_hot(item_id) && input::mouse_left_button_pressed();
                break;
            }
            if(lose_active_state){
                unset_active(item_id);
            }
        }
    } else {
        unset_hot(item_id);
        unset_active(item_id);
    }
}

/*

Section handling updating and rendering UI items.

Each item is added using ui::add_ITEM function which is the public API.

Internally, each item consists of `update_ITEM_state` and `render_ITEM_state` functions
which nicely separate item state from its visual representation.

- update_ITEM_state needs to return boolean expressing if the state has changed.
- render_ITEM_state doesn't return any value

*/

// Toggle

bool update_toggle_state(int32_t toggle_id, Vector2 pos, Vector2 size, bool *active) {
    bool changed = is_active(toggle_id);
    if(changed) *active = !(*active);
    return changed;
}

void render_toggle_state(int32_t toggle_id, Vector2 pos, Vector2 size, bool active, char *label) {
    // Set up colors.
    float color_modifier = is_hot(toggle_id) ? ACTIVE_COLOR_MODIFIER : INACTIVE_COLOR_MODIFIER;
    // Slightly toned-down so the color is less intensive.
    Vector4 box_color = COLOR_LABEL * color_modifier * 0.9f;
    Vector4 label_color = COLOR_LABEL * color_modifier;

    // Draw bg rectangle box.
    {
        // Top
        Vector2 box_size = Vector2(size.x, LINES_WIDTH);
        Vector2 box_pos = pos;
        add_rect(box_pos, box_size, box_color);

        // Bottom
        box_pos.y += size.y - LINES_WIDTH;
        add_rect(box_pos, box_size, box_color);

        // Left
        box_size = Vector2(LINES_WIDTH, size.y - LINES_WIDTH * 2);
        box_pos = pos + Vector2(0, LINES_WIDTH);
        add_rect(box_pos, box_size, box_color);

        // Right
        box_pos.x += size.x - LINES_WIDTH;
        add_rect(box_pos, box_size, box_color);
    }

    // Draw the active center of the toggle box.
    if (active) {
        float active_box_size = get_item_height() - LINES_WIDTH * 4.0f;
        Vector2 box_fg_size = Vector2(active_box_size, active_box_size);
        Vector2 box_fg_pos = pos + (size - box_fg_size) / 2.0f;
        add_rect(box_fg_pos, box_fg_size, box_color);
    }

    // Draw toggle label.
    Vector2 text_pos = pos + Vector2(size.x + LABEL_PADDING, 0);
    add_text(text_pos, label, label_color);
}

bool ui::add_toggle(Panel *panel, char *label, int *active) {
    bool a = *active;
    bool changed = add_toggle(panel, label, &a);
    *active = a;
    return changed;
}

bool ui::add_toggle(Panel *panel, char *label, bool *active) {
    int32_t toggle_id = hash_string(label);

    // Set up toggle button size and position.
    float item_height = get_item_height();
    Vector2 toggle_size = Vector2(item_height, item_height);
    Vector2 toggle_pos = panel->pos + panel->item_pos;

    // Update state.
    update_hot_and_active(toggle_id, toggle_pos, toggle_size, BUTTON);
    bool changed = update_toggle_state(toggle_id, toggle_pos, toggle_size, active);

    // Render state.
    render_toggle_state(toggle_id, toggle_pos, toggle_size, *active, label);

    // Move current panel's item position.
    panel->item_pos.y += toggle_size.y + INNER_PADDING;
    
    // Update panel's width.
    panel->width = math::max(panel->width, compute_item_width(toggle_size.x, label));

    return changed;
}

// Slider

bool update_slider_state(int32_t slider_id, Vector2 pos, Vector2 size, float *value, float min, float max) {
    // Inactive slider cannot change.
    if (!is_active(slider_id)) {
        return false;
    }

    // Get mouse position relative to the slider.
    float mouse_x = input::mouse_position_x();
    float mouse_x_rel = (mouse_x - pos.x) / (size.x);
    mouse_x_rel = math::clamp(mouse_x_rel, 0.0f, 1.0f);

    // Set output value.
    *value = mouse_x_rel * (max - min) + min;

    return true;
}

void render_slider_state(
    int32_t slider_id, Vector2 pos, Vector2 size, float value, float min, float max, char *label
) {
    // Update visual parameters.
    bool slider_active = is_hot(slider_id) || is_active(slider_id);
    float bounds_width = slider_active ? LINES_WIDTH * 2.0f : LINES_WIDTH;
    float color_modifier = slider_active ? ACTIVE_COLOR_MODIFIER : INACTIVE_COLOR_MODIFIER;
    
    // Slightly toned down so there's contrast between the current value and slider box.
    Vector4 slider_color = COLOR_LABEL * color_modifier * 0.7f;
    Vector4 bounds_color = COLOR_LABEL * color_modifier;
    Vector4 label_color = COLOR_LABEL * color_modifier;
    
    // Add min/max bound markers.
    float height = get_item_height();
    add_min_max_markers(pos, bounds_width, height, bounds_color);

    // Add current value label.
    Vector2 current_pos = Vector2(pos.x + size.x / 2.0f, pos.y);
    add_text(current_pos, label_color, Vector2(0.5f, 0.0f), "%.2f", value);

    // Add slider.
    Vector2 slider_size = Vector2((value - min) / (max - min) * ITEMS_WIDTH, height);
    add_rect(pos, slider_size, slider_color);

    // Slider label.
    Vector2 text_pos = Vector2(size.x + pos.x + LABEL_PADDING, pos.y);
    add_text(text_pos, label, label_color);
}

bool ui::add_slider(Panel *panel, char *label, int *pos, int min, int max) {
    float pos_f = float(*pos);
    bool changed = add_slider(panel, label, &pos_f, float(min), float(max));
    *pos = int(pos_f);
    return changed;
}

bool ui::add_slider(Panel *panel, char *label, float *pos, float min, float max) {
    int32_t slider_id = hash_string(label);

    // Slider bar
    float height = get_item_height();
    Vector2 slider_pos = panel->pos + panel->item_pos;
    Vector2 slider_size = Vector2(ITEMS_WIDTH, height);

    // Update state.
    update_hot_and_active(slider_id, slider_pos, slider_size, PRESS_AND_HOLD);
    bool changed = update_slider_state(slider_id, slider_pos, slider_size, pos, min, max);

    // Render state.
    render_slider_state(slider_id, slider_pos, slider_size, *pos, min, max, label);

    // Move current panel's item position.
    panel->item_pos.y += slider_size.y + INNER_PADDING;

    // Update panel's width.
    panel->width = math::max(panel->width, compute_item_width(slider_size.x, label));

    return changed;
}

// Combobox

bool update_combobox_state(
    int32_t combobox_id,
    Vector2 pos, Vector2 size,
    int value_count, int *selected_value,
    bool *expanded,
    int *hovered_value
) {
    // Update expanded state.
    if(is_active(combobox_id)) {
        *expanded = !*expanded;
    }

    // If combobox is not expanded, the selected value couldn't have changed.
    if(!*expanded) {
        return false;
    }

    // Initialize output values.
    int new_selected_value = *selected_value;
    *hovered_value = -1;

    // Move position to combobox values.
    float height = get_item_height();
    pos.y += height + INNER_PADDING;

    // Update state of individual values.
    for (int i = 0; i < value_count; ++i) {
        // Check for mouse over.
        Vector2 mouse_position = Vector2(input::mouse_position_x(), input::mouse_position_y());
        bool mouse_over = is_in_rect(mouse_position, pos, size);

        // NOTE: This could maybe be implemented by using normal hot/active logic and creating
        // special IDs for each value.
        // Update current value's state.
        if(mouse_over) {
            *hovered_value = i;

            // Select current value.
            if(input::mouse_left_button_pressed()) {
                set_active(combobox_id);
                new_selected_value = i;
            }
        }

        // Move position to next value.
        pos.y += height + INNER_PADDING;
    }

    // Set output values.
    bool selected_value_changed = new_selected_value != *selected_value;
    *selected_value = new_selected_value;
    if(selected_value_changed) {
        *expanded = false;
    }

    return selected_value_changed;
}

void render_combobox_state(
    int32_t combobox_id,
    Vector2 pos, Vector2 size,
    char **values, int value_count,
    int selected_value,
    bool expanded,
    int hovered_value,
    char *label
) {
    // Set colors based on combobox activity.
    bool combobox_active = is_hot(combobox_id) || is_active(combobox_id);
    float bounds_width = combobox_active ? LINES_WIDTH * 2.0f : LINES_WIDTH;
    float color_modifier = combobox_active ? ACTIVE_COLOR_MODIFIER : INACTIVE_COLOR_MODIFIER;
    Vector4 color = COLOR_LABEL * color_modifier;

    // Add selected value text.
    Vector2 value_pos = pos + Vector2(INNER_PADDING, 0);
    add_text(value_pos, values[selected_value], color);

    // Add label
    Vector2 label_pos = pos + Vector2(ITEMS_WIDTH + LABEL_PADDING, 0);
    add_text(label_pos, label, color);

    // Draw the drop-down arrow.
    float height = get_item_height();
    Vector2 arrow_center = Vector2(label_pos.x - INNER_PADDING - 20, label_pos.y + height / 2.0f);
    float arrow_height = height * 0.5f;
    float arrow_width = LINES_WIDTH * arrow_height / math::sqrt(3.0f);
    if(expanded) {
        // Upward pointing arrow.
        Vector2 v1 = arrow_center + Vector2(-arrow_width / 2.0f, arrow_height / 3.0f);
        Vector2 v2 = arrow_center + Vector2(arrow_width / 2.0f, arrow_height / 3.0f);
        Vector2 v3 = arrow_center + Vector2(0, -arrow_height * 2.0f / 3.0f);

        add_triangle(v1, v2, v3, color);
    } else {
        // Downward pointing arrow.
        Vector2 v1 = arrow_center + Vector2(-arrow_width / 2.0f, -arrow_height / 3.0f);
        Vector2 v2 = arrow_center + Vector2(arrow_width / 2.0f, -arrow_height / 3.0f);
        Vector2 v3 = arrow_center + Vector2(0, arrow_height * 2.0f / 3.0f);

        add_triangle(v1, v2, v3, color);
    }

    // Draw the minmax markers
    add_min_max_markers(pos, bounds_width, height, color);

    // Move position to combobox values.
    pos.y += height + INNER_PADDING;

    if(expanded) {
        for (int i = 0; i < value_count; ++i) {
            // Visual parameters for current value.
            float bounds_width = hovered_value == i ? LINES_WIDTH * 2.0f : LINES_WIDTH;
            float color_modifier = hovered_value == i ? ACTIVE_COLOR_MODIFIER : INACTIVE_COLOR_MODIFIER;
            Vector4 color = COLOR_LABEL * color_modifier;
            Vector2 value_pos = pos + Vector2(INNER_PADDING, 0);

            // Add minmax markers for active/hovered value.
            if(selected_value == i || hovered_value == i) {
                add_min_max_markers(pos, bounds_width, height, color);
            }

            // Draw the value.
            add_text(value_pos, values[i], color);

            // Move current panel item position.
            pos.y += height + INNER_PADDING;
        }
    }
}

// TODO: This can be refactored so the per-item logic is handled in add_combobox.
bool ui::add_combobox(
    Panel *panel, char *label, char **values, int value_count, int *selected_value, bool *expanded
) {
    int32_t combobox_id = hash_string(label);
    
    // Set up combobox position and each item's size.
    float height = get_item_height();
    Vector2 selected_value_pos = panel->pos + panel->item_pos;
    Vector2 combobox_value_size = Vector2(ITEMS_WIDTH, height);

    // Update states.
    update_hot_and_active(combobox_id, selected_value_pos, combobox_value_size, BUTTON);
    int hovered_value = -1;
    bool changed = update_combobox_state(
        combobox_id,
        selected_value_pos,
        combobox_value_size,
        value_count,
        selected_value,
        expanded,
        &hovered_value
    );

    // Render state.
    render_combobox_state(
        combobox_id,
        selected_value_pos,
        combobox_value_size,
        values,
        value_count,
        *selected_value,
        *expanded,
        hovered_value,
        label
    );

    // Move current panel's item position.
    int total_item_count = *expanded ? value_count + 1 : 1; 
    panel->item_pos.y += (combobox_value_size.y + INNER_PADDING) * total_item_count;

    // Update panel's width.
    panel->width = math::max(panel->width, compute_item_width(combobox_value_size.x, label));

    return changed;
}

// Function plot

bool update_function_plot_state(
    int32_t plot_id, Vector2 pos, Vector2 size, float *selected_value, float min_x, float max_x
) {
    // Inactive plot cannot change.
    if(!is_active(plot_id)) {
        return false;
    }
    
    // Get mouse position relative to the plot.
    float mouse_position_x = input::mouse_position_x();
    float pos_relative_to_plot_x = mouse_position_x - pos.x;
    float pos_relative_in_plot_x = pos_relative_to_plot_x / size.x;

    // Set output value.
    *selected_value = math::clamp(pos_relative_in_plot_x, 0.0f, 1.0f) * (max_x - min_x) + min_x;

    return true;
}

void render_function_plot_state(
    int32_t plot_id,
    Vector2 pos, Vector2 size,
    float *x, float *y, int point_count,
    float min_x, float max_x,
    float min_y, float max_y,
    float select_x, float select_y,
    char *label
) {
    // Update visual parameteres.
    bool plot_active = is_hot(plot_id) || is_active(plot_id);
    float bounds_width = plot_active ? LINES_WIDTH * 2.0f : LINES_WIDTH;
    float color_modifier = plot_active ? ACTIVE_COLOR_MODIFIER : INACTIVE_COLOR_MODIFIER;
    Vector4 color = COLOR_LABEL * color_modifier;

    // There's a vertical space between item start and the actual plot start.
    Vector2 plot_pos = pos + Vector2(0, PLOT_BOX_VERTICAL_PADDING);
    Vector2 plot_size = size - Vector2(0, PLOT_BOX_VERTICAL_PADDING * 2.0f);

    // Add minmax markers.
    add_min_max_markers(pos, bounds_width, size.y, color);

    // Draw the grid.
    const int GRID_X_COUNT = 8, GRID_Y_COUNT = 5;
    Vector4 grid_color = color * 0.5f;
    // Grid overlaps with min_max_markers, so it starts and ends outside of the item.
    Vector2 grid_pos = plot_pos + Vector2(-LINES_WIDTH, 0.0f);
    Vector2 grid_size = plot_size + Vector2(LINES_WIDTH * 2.0f, 0.0f);
    for(int i = 0; i < GRID_X_COUNT; ++i) {
        Vector2 grid_line_pos = grid_pos + Vector2((grid_size.x - LINES_WIDTH) / float(GRID_X_COUNT - 1) * i, 0);
        Vector2 grid_line_size = Vector2(LINES_WIDTH, plot_size.y);
        add_rect(grid_line_pos, grid_line_size, grid_color);
    }
    for(int i = 0; i < GRID_Y_COUNT; ++i) {
        Vector2 grid_line_pos = grid_pos + Vector2(0, grid_size.y / float(GRID_Y_COUNT - 1) * i);
        Vector2 grid_line_size = Vector2(grid_size.x, LINES_WIDTH);
        add_rect(grid_line_pos, grid_line_size, grid_color);
    }

    // Draw the function plot line.
    Vector2 *points = (Vector2 *)alloc_temp(point_count * sizeof(Vector2));
    for(int i = 0; i < point_count; ++i) {
        float x_plot = (x[i] - min_x) / (max_x - min_x);
        float y_plot = (y[i] - min_y) / (max_y - min_y);
        points[i].x = plot_pos.x + x_plot * plot_size.x;
        points[i].y = plot_pos.y + (1.0f - y_plot) * plot_size.y;
    }
    Vector4 line_color = color * 0.75f;
    add_line(points, point_count, LINES_WIDTH, line_color);

    // Draw the selected x-value box/line.
    const float select_box_width = 3.0f;
    Vector4 select_box_color = color;
    Vector2 select_box_size = Vector2(select_box_width, plot_size.y);
    float relative_select_box_pos_x = math::clamp((select_x - min_x) / (max_x - min_x), 0.0f, 1.0f);
    Vector2 select_box_pos = Vector2(
        plot_pos.x + relative_select_box_pos_x * plot_size.x - select_box_width * 0.5f,
        plot_pos.y
    );
    add_rect(select_box_pos, select_box_size, select_box_color);

    // Add item label.
    Vector2 text_pos = pos + Vector2(ITEMS_WIDTH + LABEL_PADDING, 0);
    add_text(text_pos, label, color);

    // Add text for currently selected values.
    float height = get_item_height();
    Vector2 select_text_pos = pos + Vector2(ITEMS_WIDTH + LABEL_PADDING, height);
    add_text(select_text_pos, color, Vector2(0.0f, 0.0f), "[%.2f, %.2f]", select_x, select_y);
}

bool ui::add_function_plot(
    Panel *panel, char *label, float *x, float *y, int point_count, float *select_x, float select_y
) {
    int32_t plot_id = hash_string(label);

    // Set up function plot size and position.
    const float plot_size_height_to_item_height = 4.0f;
    float height = get_item_height();
    Vector2 plot_box_pos = panel->pos + panel->item_pos;
    Vector2 plot_box_size = Vector2(ITEMS_WIDTH, height * plot_size_height_to_item_height);

    // Update the function plot state.
    float min_x = 1000.0f, max_x = -1000.0f;
    float min_y = 1000.0f, max_y = -1000.0f;
    {
        for(int i = 0; i < point_count; ++i) {
            min_x = math::min(min_x, x[i]);
            max_x = math::max(max_x, x[i]);
            min_y = math::min(min_y, y[i]);
            max_y = math::max(max_y, y[i]);
        }
    }
    update_hot_and_active(plot_id, plot_box_pos, plot_box_size, PRESS_AND_HOLD);
    bool changed = update_function_plot_state(plot_id, plot_box_pos, plot_box_size, select_x, min_x, max_x);

    // Render the function plot state.
    render_function_plot_state(
        plot_id,
        plot_box_pos, plot_box_size,
        x, y, point_count,
        min_x, max_x,
        min_y, max_y,
        *select_x, select_y,
        label
    );

    // Move current panel's item position.
    panel->item_pos.y += height * plot_size_height_to_item_height + INNER_PADDING;

    // Update panel's width.
    panel->width = math::max(panel->width, compute_item_width(plot_box_size.x, label));

    return changed;
}

// Textbox

// TODO:
// - Shift selection handling
// - Ctrl handling
bool update_textbox_state(
    int32_t textbox_id, Vector2 pos, Vector2 size, int *cursor_position, char *text, int buffer_size
) {
    // Inactive textbox cannot change.
    if(!is_active(textbox_id)) {
        return false;
    }

    // Prepare variables used all over the function.
    int new_cursor_position = *cursor_position;
    bool text_changed = false;
    int text_length = int(strlen(text));

    // Handle left-right arrow keys.
    if(input::key_pressed(KeyCode::LEFT)) {
        new_cursor_position = math::max(new_cursor_position - 1, 0);
    } else if(input::key_pressed(KeyCode::RIGHT)) {
        new_cursor_position = math::min(new_cursor_position + 1, text_length);
    }

    // Handle home and end keys.
    if (input::key_pressed(KeyCode::HOME)) {
        new_cursor_position = 0;
    } else if (input::key_pressed(KeyCode::END)) {
        new_cursor_position = text_length;
    }

    // Handle delete and backspace keys,
    if(input::key_pressed(KeyCode::DEL)) {
        if(new_cursor_position < text_length) {
            // Delete the character pointed to by cursor.
            delete_character(text, text_length, new_cursor_position);

            text_changed = true;
        }
    } else if (input::key_pressed(KeyCode::BACKSPACE)) {
        if(new_cursor_position > 0) {
            // Delete the character right before the cursor.
            delete_character(text, text_length, new_cursor_position - 1);

            // Update the cursor position.
            new_cursor_position -= 1;

            text_changed = true;
        }
    }
    
    // Handle character input.
    int characters_entered_count = input::characters_entered(NULL);
    if(characters_entered_count > 0 && new_cursor_position + characters_entered_count < buffer_size) {
        // Prepare space for entered characters by moving text from after the point to the right.
        char *src_ptr = text + new_cursor_position;
        char *dst_ptr = src_ptr + characters_entered_count;
        int text_length = int(strlen(text));
        int text_length_after_ptr = text_length - new_cursor_position;
        memcpy(dst_ptr, src_ptr, text_length_after_ptr);

        // Write the entered characters.
        input::characters_entered(text + new_cursor_position);
        text[text_length + characters_entered_count] = 0; // NULL terminating character.

        // Update the cursor position.
        new_cursor_position += characters_entered_count;

        text_changed = true;
    }

    // Write output data.
    *cursor_position = new_cursor_position;
    
    return text_changed;
}

void render_textbox_state(
    int32_t textbox_id, Vector2 pos, Vector2 size, int cursor_position, char *text, int buffer_size, char *label
) {
    // Update visual parameters.
    bool textbox_active = is_hot(textbox_id) || is_active(textbox_id);
    float bounds_width = textbox_active ? LINES_WIDTH * 2.0f : LINES_WIDTH;
    float color_modifier = textbox_active ? ACTIVE_COLOR_MODIFIER : INACTIVE_COLOR_MODIFIER;
    Vector4 color = COLOR_LABEL * color_modifier;
    Vector4 inverted_color = Vector4(1.0f - color.x, 1.0f - color.y, 1.0f - color.z, 1.0f);
    Vector4 selected_text_color = is_active(textbox_id) ? inverted_color : color;

    // Text parameters.
    // NOTE: This assumes monospace font!
    float character_width = font::get_string_width("A", ui_draw::get_font());
    int text_length = int(strlen(text));

    // Get the first and last character which will be shown.
    int max_chars_shown = int(size.x / character_width);
    // Ideally we want the cursor to be in the middle of the text bar.
    int ideal_cursor_relative_position = max_chars_shown / 2;
    int first_char_shown = math::clamp(
        cursor_position - ideal_cursor_relative_position,
        0,
        // We show/can point to one character after the last, so we can add to the text.
        text_length + 1 - max_chars_shown
    );
    int last_char_shown = math::min(text_length + 1, first_char_shown + max_chars_shown);

    // Add text which is before the cursor.
    int pre_cursor_text_start = first_char_shown;
    int pre_cursor_text_end = cursor_position;
    int pre_cursor_text_length = pre_cursor_text_end - pre_cursor_text_start;
    Vector2 text_pos = pos + Vector2(INNER_PADDING, 0.0f);
    add_text(text_pos, text + pre_cursor_text_start, pre_cursor_text_length, color);

    // Add text which is after the cursor.
    if(cursor_position < text_length) {
        int post_cursor_text_start = cursor_position + 1;
        int post_cursor_text_end = last_char_shown;
        // NOTE: This assumes monospace font!
        float post_cursor_text_x_offset = character_width * (pre_cursor_text_length + 1);
        add_text(
            text_pos + Vector2(post_cursor_text_x_offset, 0.0f),
            text + post_cursor_text_start, post_cursor_text_end - post_cursor_text_start, color
        );
    }

    // Add the selected text (under cursor).
    float cursor_x_offset = character_width * pre_cursor_text_length;
    add_text(
        text_pos + Vector2(cursor_x_offset,0.0f),
        text + cursor_position, 1, selected_text_color
    );

    // Add the cursor.
    if(is_active(textbox_id)) {
        Vector2 cursor_pos = text_pos + Vector2(cursor_x_offset, 0.0f);
        Vector2 cursor_size = Vector2(character_width, get_item_height());
        add_rect(cursor_pos, cursor_size, color);
    }

    // Add min/max markers.
    add_min_max_markers(pos, bounds_width, get_item_height(), color);

    // Add label.
    Vector2 label_pos = pos + Vector2(ITEMS_WIDTH + LABEL_PADDING, 0);
    add_text(label_pos, label, color);
}

bool ui::add_textbox(Panel *panel, char *label, char *text, int buffer_size, int *cursor_position) {
    int32_t textbox_id = hash_string(label);

    // Set up textbox size and position.
    float height = get_item_height();
    Vector2 textbox_pos = panel->pos + panel->item_pos;
    Vector2 textbox_size = Vector2(ITEMS_WIDTH, height);

    // Update state.
    update_hot_and_active(textbox_id, textbox_pos, textbox_size, STICKY);
    bool text_changed = update_textbox_state(
        textbox_id, textbox_pos, textbox_size, cursor_position, text, buffer_size
    );

    // Render state.
    render_textbox_state(textbox_id, textbox_pos, textbox_size, *cursor_position, text, buffer_size, label);

    // Move current panel's item position.
    panel->item_pos.y += height + INNER_PADDING;

    // Update panel's width.
    panel->width = math::max(panel->width, compute_item_width(textbox_size.x, label));

    return text_changed;
}
