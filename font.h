#pragma once
#include "graphics.h"
#include "stb_truetype.h"

// TODO: do floats
struct Glyph
{
    int bitmap_x, bitmap_y;
    int bitmap_width, bitmap_height;
    int x_offset, y_offset;
    int advance;
};

struct Font
{
    Glyph glyphs[96];
    float row_height;
    float top_pad;
    float scale;
    Texture texture;
    stbtt_fontinfo font_info;
};

namespace font
{
    Font get(uint8_t *data, float size, uint32_t texture_size);
    float get_row_height(Font *font);
    float get_kerning(Font *font, char c1, char c2);
    void release(Font *font);
}