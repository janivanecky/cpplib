#pragma once
#include "graphics.h"
#include "stb_truetype.h"
#include "ft2build.h"
#include FT_FREETYPE_H

/*
Uses stb_truetype library for rasterizing .ttf/.otf files.
*/

// TODO: do floats
// Glyph specifies information aobut a single letter at a single scale
struct Glyph
{
    int bitmap_x, bitmap_y;
    int bitmap_width, bitmap_height;
    int x_offset, y_offset;
    int advance;
};

// Font contains data for rendering text at specific scale
struct Font
{
    Glyph glyphs[96];
    float row_height;
    float top_pad;
    float scale;
    Texture texture;
    stbtt_fontinfo font_info;
    FT_Face face;
};

// font namespace handles loading Fonts and extracting information from it
namespace font
{
    /*
    Return initialized Font object

    Args:
        - data: binary data from .ttf/.otf file
        - data_size: size of data block in bytes
        - size: height of the font in pixels
        - texture_size: size of a side of texture that stores font bitmap, in pixels
    */
    Font get(uint8_t *data, int32_t data_size, int32_t size, int32_t texture_size);

    // Initialize font rasterization code (FreeType2)
    void init();

    // Return height of a single row for a Font
    float get_row_height(Font *font);

    // Get kerning between two letters of a Font
    float get_kerning(Font *font, char c1, char c2);

    // Release a Font object
    void release(Font *font);
}