#define CPPLIB_PLATFORM_IMPL
#define CPPLIB_GRAPHICS_IMPL
#define CPPLIB_FILESYSTEM_IMPL
#define CPPLIB_MATHS_IMPL
#define CPPLIB_MEMORY_IMPL
#define CPPLIB_UI_IMPL
#define CPPLIB_FONT_IMPL
#define CPPLIB_INPUT_IMPL
#include "platform.h"
#include "graphics.h"
#include "memory.h"
#include "ui.h"
#include "font.h"
#include "input.h"
#include <cassert>

int main(int argc, char **argv) {
    // Set up window
    uint32_t window_width = 800, window_height = 800;
 	Window window = platform::get_window("UI demo", window_width, window_height);
    assert(platform::is_window_valid(&window));

    // Init graphics
    graphics::init();
    graphics::init_swap_chain(&window);

    font::init();
    ui::init((float)window_width, (float)window_height);
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
    char text_buffer[100] = "HELLO THERE, WHAT'S UP";
    int text_cursor = 0;

    // Function plot state variables
    float sin_x[100];
    float sin_y[100];
    for(int i = 0; i < 100; ++i) {
        sin_x[i] = math::PI2 / 100.0f * i;
        sin_y[i] = math::sin(sin_x[i]);
    }
    float sin_x_selected = 0.5f;
    float sin_y_selected = math::sin(sin_x_selected);

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
        graphics::clear_render_target(&render_target_window, 0.0f, 0.0f, 0.0f, 1);

        // Start the UI panel
        Panel panel = ui::start_panel("UI TEST", Vector2(10, 10.0f), 420.0f);
        ui::set_background_opacity(0.0f);

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
        ui::add_function_plot(&panel, "sin function", sin_x, sin_y, ARRAYSIZE(sin_x), &sin_x_selected, sin_y_selected);

        // Test text drawing
        ui::draw_text("TEST", 600, 10, Vector4(1,1,1,1));

        // Test rect drawing
        ui::draw_rect(600, 50, 100, 20, Vector4(1,0,0,1));

        // Test triangle drawing
        ui::draw_triangle(Vector2(600, 80), Vector2(700, 80), Vector2(650, 100), Vector4(0,1,0,1));
        Vector2 line_points[4] = {
            Vector2(600, 110),
            Vector2(600, 120),
            Vector2(700, 110),
            Vector2(700, 120),
        };

        // Test line drawing
        ui::draw_line(line_points, 4, 4, Vector4(0,1,1,1));

        // End frame
        ui::end_panel(&panel);
        ui::end();
        graphics::swap_frames();
    }

    ui::release();
    graphics::release();
    return 0;
}