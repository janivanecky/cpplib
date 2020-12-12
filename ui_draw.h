#pragma once
#include "font.h"
#include "maths.h"

namespace ui_draw {
    void init(float screen_width, float screen_height);

    void draw_text(char *text, float x, float y, Vector4 color, Vector2 origin = Vector2(0,0));
    void draw_text(char *text, Vector2 pos, Vector4 color, Vector2 origin = Vector2(0,0));

    void draw_rect(float x, float y, float width, float height, Vector4 color);
    void draw_rect(Vector2 pos, float width, float height, Vector4 color);

    void draw_triangle(Vector2 v1, Vector2 v2, Vector2 v3, Vector4 color);

    void draw_line(Vector2 *points, int point_count, float width, Vector4 color);

    void release();

    Font *get_font();
}

#ifdef CPPLIB_UIDRAW_IMPL
#include "ui_draw.cpp"
#endif