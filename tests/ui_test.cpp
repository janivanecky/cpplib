#define CPPLIB_PLATFORM_IMPL
#define CPPLIB_GRAPHICS_IMPL
#define CPPLIB_FILESYSTEM_IMPL
#define CPPLIB_MATHS_IMPL
#define CPPLIB_MEMORY_IMPL
#define CPPLIB_UI_IMPL
#define CPPLIB_UIDRAW_IMPL
#define CPPLIB_TTF_IMPL
#define CPPLIB_FONT_IMPL
#define CPPLIB_INPUT_IMPL
#include "platform.h"
#include "graphics.h"
#include "memory.h"
#include "ui.h"
#include "ui_draw.h"
#include "font.h"
#include "input.h"
#include <cassert>

int main(int argc, char **argv) {
    // Set up window
    uint32_t window_width = 800, window_height = 800;
 	HWND window = platform::get_window("UI demo", window_width, window_height);
    assert(platform::is_window_valid(window));

    // Init graphics
    graphics::init();
    graphics::init_swap_chain(window, window_width, window_height);

    ui_draw::init((float)window_width, (float)window_height);
    ui::set_input_responsive(true);

    // Create window render target
	RenderTarget render_target_window = graphics::get_render_target_window(true);
    assert(graphics::is_ready(&render_target_window));

    // Slider state variables
    float slider_val = 0.0f;

    // Toggle state variables
    bool toggle_val = true;

    // Combobox state variables
    bool combobox_expanded = false;
    int combobox_selected_val = 0;

    // Textfield state variables
    char text_buffer[100] = "HELLO THERE, WHAT'S UP! SOMETHING.";
    int text_cursor = 0;

    // Function plot state variables
    float sin_x[100];
    float sin_y[100];
    for(int i = 0; i < 100; ++i) {
        sin_x[i] = math::PI2 / 100.0f * i;
        sin_y[i] = math::sin(sin_x[i]);
    }
    float sin_x_selected = 0.5f;

    // Initialize test texture
    const uint32_t texture_width = 256;
    const uint32_t texture_height = 256;
    uint32_t *texture_data = (uint32_t *)malloc(sizeof(uint32_t) * texture_width * texture_height);
    for(uint32_t y = 0; y < texture_height; ++y) {
        for(uint32_t x = 0; x < texture_width; ++x) {
            bool checker = (((y / 32) % 2) + ((x / 32) % 2)) % 2;
            texture_data[y * texture_width + x] = checker ? 0xFF000000 : 0xFFFFFFFF;
        }
    }
    Texture2D texture = graphics::get_texture2D(texture_data, texture_width, texture_height);
    free(texture_data);

    // Render loop
    bool is_running = true;
    while(is_running) {
        input::reset();

        // Event loop
        Event event;
        while(platform::get_event(&event)) {
            input::register_event(&event);

            // Check if close button pressed
            switch(event.type) {
                case EventType::EXIT: {
                    is_running = false;
                }
                break;
            }
        }
        // React to ESC key
        if (input::key_pressed(KeyCode::ESC)) is_running = false;

        // Set up render target.
        graphics::set_render_targets_viewport(&render_target_window);
        graphics::clear_render_target(&render_target_window, 0.5f, .5f, 0.5f, 1);

        // Start the UI panel
        Panel panel = ui::start_panel("UI TEST", Vector2(0, 0.0f));
        ui::set_background_opacity(1.0f);

        // Test slider
        ui::add_slider(&panel, "slider", &slider_val, -1.0f, 1.0f);

        // Test toggle
        ui::add_toggle(&panel, "toggle", &toggle_val);

        // Test combobox
        char *combo_values[] = {"val1", "val2"};
        ui::add_combobox(&panel, "combobox", combo_values, 2, &combobox_selected_val, &combobox_expanded);

        // Test textbox
        ui::add_textbox(&panel, "text", text_buffer, ARRAYSIZE(text_buffer), &text_cursor);

        // Test function plot
        float sin_y_selected = math::sin(sin_x_selected);
        ui::add_function_plot(&panel, "sin function", sin_x, sin_y, ARRAYSIZE(sin_x), &sin_x_selected, sin_y_selected);

        // Test text drawing
        float y_pos = 10.0f;
        ui_draw::draw_text("Test 123 0", 600, y_pos, Vector4(0,0,0,1), Vector2(0,0));
        y_pos += 30.0f;

        // Test rect drawing
        ui_draw::draw_rect(600, y_pos, 100, 20, Vector4(1,0,0,1));
        y_pos += 30.0f;

        // Test textured rect drawing
        ui_draw::draw_rect_textured(600, y_pos, 100, 100, &texture);
        y_pos += 110.0f;

        // Test triangle drawing
        ui_draw::draw_triangle(Vector2(600, y_pos), Vector2(700, y_pos), Vector2(650, y_pos + 20), Vector4(0,1,0,1));
        y_pos += 50.0f;


        // Test line drawing
        Vector2 line_points[4] = {
            Vector2(600, y_pos),
            Vector2(600, y_pos + 30),
            Vector2(700, y_pos),
            Vector2(700, y_pos + 30),
        };
        ui_draw::draw_line(line_points, 4, 8, Vector4(0,1,1,1));
        y_pos += 100;

        const int circle_point_count = 16000;
        Vector2 circle_points[circle_point_count];
        for(int i = 0; i < circle_point_count; ++i) {
            float a = math::PI2 / float(circle_point_count - 1) * i;
            circle_points[i].x = 650 + math::sin(a) * 50.0f;
            circle_points[i].y = y_pos + math::cos(a) * 50.0f;
        }

        // Test line drawing
        ui_draw::draw_line(circle_points, circle_point_count, 2, Vector4(0,1,1,1));

        // End frame
        ui::end_panel(&panel);
        ui::end_frame();
        graphics::swap_frames();
    }

    graphics::release();
    ui_draw::release();

    return 0;
}