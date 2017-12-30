#include "font.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "memory.h"
#include "logging.h"

// Globally unique objects
FT_Library ft_library;

void font::init()
{
    int32_t error = FT_Init_FreeType(&ft_library);
    if (error)
    {
        logging::print_error("Error initializing FreeType library!");
    }
}

Font font::get(uint8_t *data, int32_t data_size, int32_t size, int32_t texture_size)
{
    Font font = {};
    
    // Store temp allocator stack state    
    memory::push_temp_state();

    // Allocate memory for the texture
    uint8_t *font_buffer = memory::alloc_temp<uint8_t>(texture_size * texture_size);
#ifdef STB_FONTS
    // Initialize stb font data
    stbtt_InitFont(&font.font_info, data, stbtt_GetFontOffsetForIndex(data,0));

    // Get font scale for certaing pixel height
    float font_scale = stbtt_ScaleForPixelHeight(&font.font_info, size);
    font.scale = font_scale;
    
    // Get non-letter-specific data about Font
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font.font_info, &ascent, &descent, &line_gap);
    font.row_height = (float)(ascent - descent + line_gap) * font_scale;
    font.top_pad = (float)line_gap * font_scale;
    
    // Get information about each ASCII character
    int32_t x = 0, y = 0;
    int32_t letter_width, letter_height;
    letter_width = letter_height = 32;
    for (unsigned char c = 32; c < 128; ++c)
    {
        // Rasterize letter and insert into allocated buffer
        stbtt_MakeCodepointBitmapSubpixel(&font.font_info, &font_buffer[texture_size * y + x], letter_width, letter_height, texture_size, font_scale, font_scale, 0, 0, c);

        // Get position of the letter in the rasterized bitmap
        int32_t x0,x1,y0,y1;
        stbtt_GetCodepointBitmapBoxSubpixel(&font.font_info, c, font_scale, font_scale, 0, 0, &x0, &y0, &x1, &y1);

        // Get letter-specific info
        int advance, lsb;                                    
        stbtt_GetCodepointHMetrics(&font.font_info, c, &advance, &lsb);

        // Store a glyph for the letter
        font.glyphs[c - 32] = {x, y, x1 - x0, y1 - y0, (int32_t) (lsb * font_scale), (int32_t)(ascent * font_scale) + y0, (int32_t)(advance * font_scale)};

        // Move in the texture/buffer
        x += letter_width;
        if(x >= (int32_t)texture_size)
        {
            x = 0;
            y += letter_height;
        }
    }
#else
    FT_Face face;
    int32_t error = FT_New_Memory_Face(ft_library, data, data_size, 0, &face);
    if (error)
    {
        logging::print_error("Error creating font face!");
    }

    int32_t supersampling = 1;
    error = FT_Set_Pixel_Sizes(face, 0, size);
    if (error)
    {
        logging::print_error("Error setting pixel size!");
    }

    int32_t row_height = face->size->metrics.height / 64;
    int32_t ascender = face->size->metrics.ascender / 64;
    int32_t x = 0, y = 0;

    font.row_height = (float)row_height;
    font.top_pad = 0.0f;
    font.scale = 1.0f;
    font.face = face;
 
    for (unsigned char c = 32; c < 128; ++c)
    {
        // Load character c
        error = FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT);
        if(error)
        {
            logging::print_error("Error loading character %c!", c);
        }

        // Get glyph specific metrics
        int x_offset = face->glyph->metrics.horiBearingX / 64;
        int y_offset = ascender - face->glyph->metrics.horiBearingY / 64;
        int width = face->glyph->metrics.width / 64;
        int height = face->glyph->metrics.height / 64;
        int advance = face->glyph->metrics.horiAdvance / 64;

        // Get bitmap parameters
        int bitmap_width = face->glyph->bitmap.width, bitmap_height = face->glyph->bitmap.rows;
        int pitch = face->glyph->bitmap.pitch;

        // In case we don't fit in the row, let's move to next row
        if(x > texture_size - bitmap_width)
        {
            x = 0;
            y += row_height;
        }

        // Copy over bitmap
        for(int32_t r = 0; r < bitmap_height; ++r)
        {
            uint8_t *source = face->glyph->bitmap.buffer + pitch * r;
            uint8_t *dest = font_buffer + (y + r) * texture_size + x;
            memcpy(dest, source, bitmap_width);
        }

        // Store glyph settings
        font.glyphs[c - 32] = {x, y, bitmap_width, bitmap_height, x_offset, y_offset, advance};

        // Move in the bitmap
        x += bitmap_width;
        // Counts
    }
#endif
    // Initialize D3D texture for the Font
    font.texture = graphics::get_texture(font_buffer, texture_size, texture_size, DXGI_FORMAT_R8_UNORM, 1);
    if(!graphics::is_ready(&font.texture))
    {
        logging::print_error("Could not create texture for font.");
    }

    // Restore memory state of the temp allocator
    memory::pop_temp_state();

    return font;
}

float font::get_kerning(Font *font, char c1, char c2)
{
#ifdef STB_FONTS
    return font->scale * stbtt_GetCodepointKernAdvance(&font->font_info, c1, c2);
#else
    FT_Vector kerning;
    int32_t left_glyph_index = FT_Get_Char_Index(font->face, c1);
    int32_t right_glyph_index = FT_Get_Char_Index(font->face, c2);
    FT_Get_Kerning(font->face, left_glyph_index, right_glyph_index, FT_KERNING_DEFAULT, &kerning);
    return (float)kerning.x;
#endif
}

float font::get_row_height(Font *font)
{
    return font->row_height;
}

void font::release(Font *font)
{
    graphics::release(&font->texture);
}
