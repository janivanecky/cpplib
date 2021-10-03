#pragma once
#include "font.h"
#include "maths.h"

enum ShadingType {
    SOLID_COLOR,
    LINES
};

struct Font;
struct Texture2D;

namespace ui_draw {
    void init(float screen_width, float screen_height);
    void set_screen_size(float screen_width, float screen_height);

    void draw_text(char *text, float x, float y, Vector4 color, Vector2 origin = Vector2(0,0), Font *font = NULL, Texture2D *font_texture = NULL);
    void draw_text(char *text, Vector2 pos, Vector4 color, Vector2 origin = Vector2(0,0), Font *font = NULL, Texture2D *font_texture = NULL);

    void draw_rect(float x, float y, float width, float height, Vector4 color, ShadingType shading_type=SOLID_COLOR);
    void draw_rect(Vector2 pos, float width, float height, Vector4 color, ShadingType shading_type=SOLID_COLOR);
    void draw_rect(Vector2 pos, Vector2 size, Vector4 color, ShadingType shading_type=SOLID_COLOR);
    void draw_rect_textured(float x, float y, float width, float height, Texture2D *texture);

    void draw_triangle(Vector2 v1, Vector2 v2, Vector2 v3, Vector4 color);

    void draw_line(Vector2 *points, int point_count, float width, Vector4 color);

    void draw_circle(Vector2 pos, float radius, Vector4 color);

    void draw_arc(
        Vector2 pos, float radius_min, float radius_max, float start_radian, float end_radian, Vector4 color
    );

    void release();

    Font *get_font();
    float get_screen_width();
    float get_screen_height();
}

#ifdef CPPLIB_UIDRAW_IMPL
#include "ui_draw.cpp"
#endif