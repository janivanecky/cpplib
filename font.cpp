#include "font.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "memory.h"

Font font::get(uint8_t *data, float size, uint32_t texture_size)
{
    Font font;
    
    stbtt_InitFont(&font.font_info, data, stbtt_GetFontOffsetForIndex(data,0));
    float font_scale = stbtt_ScaleForPixelHeight(&font.font_info, size);

    font.scale = font_scale;
    
    StackAllocator *allocator = memory::get_temp_stack();
    StackAllocatorState memory_state = memory::save_stack_state(allocator);

    uint8_t *font_buffer = memory::alloc_stack<uint8_t>(allocator, texture_size * texture_size);
    int32_t x = 0, y = 0;
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font.font_info, &ascent, &descent, &line_gap);
    font.row_height = (float)(ascent - descent + line_gap) * font_scale;
    font.top_pad = (float)line_gap * font_scale;
    for (unsigned char c = 32; c < 128; ++c)
    {
        stbtt_MakeCodepointBitmapSubpixel(&font.font_info, &font_buffer[texture_size * y + x], 32, 32, texture_size, font_scale, font_scale, 0, 0, c);
        int32_t x0,x1,y0,y1;
        stbtt_GetCodepointBitmapBoxSubpixel(&font.font_info, c, font_scale, font_scale, 0, 0, &x0, &y0, &x1, &y1);
        int advance, lsb;                                    
        stbtt_GetCodepointHMetrics(&font.font_info, c, &advance, &lsb);
        font.glyphs[c - 32] = {x, y, x1 - x0, y1 - y0, (int32_t) (lsb * font_scale), (int32_t)(ascent * font_scale) + y0, (int32_t)(advance * font_scale)};
        x += 32;
        if(x >= (int32_t)texture_size)
        {
            x = 0;
            y += 32;
        }
    }
    
    font.texture = graphics::get_texture(font_buffer, texture_size, texture_size, DXGI_FORMAT_R8_UNORM, 1);
    memory::load_stack_state(allocator, memory_state);

    return font;
}

float font::get_kerning(Font *font, char c1, char c2)
{
    return font->scale * stbtt_GetCodepointKernAdvance(&font->font_info, c1, c2);
}

float font::get_row_height(Font *font)
{
    return font->row_height;
}

void font::release(Font *font)
{
    graphics::release(&font->texture);
}
