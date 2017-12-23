#include "ui.h"
#include "font.h"
#include "graphics.h"
#include "array.h"
#include "input.h"
// TODO: maybe set as not dependent on resources, but rather pass in necessary resources during init call
#include "resources.h"
#include "file_system.h"

static ConstantBuffer buffer_rect;
static ConstantBuffer buffer_pv;
static ConstantBuffer buffer_model;
static ConstantBuffer buffer_color;
static BlendState alpha_blend_state;
static Mesh quad_mesh;
static VertexShader vertex_shader_font;
static PixelShader pixel_shader_font;
static VertexShader vertex_shader_rect;
static PixelShader pixel_shader_rect;
static Font font_ui;

char vertex_shader_font_string[] = 
"struct VertexInput"
"{"
"	float4 position: POSITION;"
"	float2 texcoord: TEXCOORD;"
"};"
""
"struct VertexOutput"
"{"
"	float4 svPosition: SV_POSITION;"
"	float2 texcoord: TEXCOORD;"
"};"
""
"cbuffer PVBuffer : register(b0)"
"{"
"	matrix projection;"
"	matrix view;"
"};"
""
"cbuffer PVBuffer : register(b1)"
"{"
"	matrix model;"
"};"
""
"cbuffer SourceRectBuffer: register(b6)"
"{"
"    float4 source_rect;"
"}"
""
"VertexOutput main(VertexInput input)"
"{"
"	VertexOutput result;"
""
"	result.svPosition = mul(projection, mul(view, mul(model, input.position)));"
"	result.texcoord = input.texcoord * source_rect.zw + source_rect.xy;"
""
"	return result;"
"}";

char pixel_shader_font_string[] = 
"struct PixelInput"
"{"
"	float4 svPosition: SV_POSITION;"
"	float2 texcoord: TEXCOORD;"
"};"
""
"SamplerState texSampler: register(s0);"
"Texture2D tex: register(t0);"
""
"cbuffer ColorBuffer: register(b7)"
"{"
"	float4 color;"
"}"
""
"float4 main(PixelInput input) : SV_TARGET"
"{"
"	float alpha = tex.Sample(texSampler, input.texcoord).r;"
"	float4 output = float4(color.xyz, color.w * alpha);"
"	return output;"
"}";

char vertex_shader_rect_string[] =
"struct VertexInput"
"{"
"	float4 position: POSITION;"
"};"
""
"struct VertexOutput"
"{"
"	float4 svPosition: SV_POSITION;"
"};"
""
"cbuffer PVBuffer : register(b0)"
"{"
"	matrix projection;"
"	matrix view;"
"};"
""
"cbuffer PVBuffer : register(b1)"
"{"
"	matrix model;"
"};"
""
"VertexOutput main(VertexInput input)"
"{"
"	VertexOutput result;"
""
"	result.svPosition = mul(projection, mul(view, mul(model, input.position)));"
""
"	return result;"
"}";

char pixel_shader_rect_string[] = 
"struct PixelInput"
"{"
"	float4 svPosition: SV_POSITION;"
"};"
""
"cbuffer ColorBuffer: register(b7)"
"{"
"	float4 color;"
"}"
""
"float4 main(PixelInput input) : SV_TARGET"
"{"
"	return color;"
"}";

struct TextItem
{
    Vector4 color;
    Vector2 pos;
    char *text;
};

struct RectItem
{
    Vector4 color;
    Vector2 pos;
    Vector2 size;
};

static Array<TextItem> text_items;
static Array<RectItem> rect_items;
static Array<RectItem> rect_items_bg;

int32_t active_id = -1;

int32_t hash_string(char *string)
{
    int32_t hash_value = 5381;
    while(*string)
    {
        hash_value = ((hash_value << 5) + hash_value) + *string;
        string++;
    }

    return hash_value + 1;
}

float quad_vertices[] = {
    -1.0, 1.0, 0.0, 1.0, // LEFT TOP 1 
    0.0, 0.0,
    1.0, 1.0, 0.0, 1.0, // RIGHT TOP 2
    1.0, 0.0,
    -1.0, -1.0, 0.0, 1.0, // LEFT BOT 3
    0.0, 1.0,
    1.0, -1.0, 0.0, 1.0, // RIGHT BOT 4
    1.0, 1.0
};

uint16_t quad_indices[] = {
    2, 3, 1,
    2, 1, 0
};

void ui::init()
{
    buffer_model = graphics::get_constant_buffer(sizeof(Matrix4x4));
    buffer_pv = graphics::get_constant_buffer(sizeof(Matrix4x4) * 2);
    buffer_rect = graphics::get_constant_buffer(sizeof(Vector4));
    buffer_color = graphics::get_constant_buffer(sizeof(Vector4));
    alpha_blend_state = graphics::get_blend_state(ALPHA_BLEND);
    quad_mesh = graphics::get_mesh(quad_vertices, 4, sizeof(float) * 6, quad_indices, 6, 2);
    
    // TODO: Parse attributes from shader
    VertexInputDesc vertex_shader_input_descs[2];
    memcpy(vertex_shader_input_descs[0].semantic_name, "POSITION", 8);
    vertex_shader_input_descs[0].semantic_name[8] = 0;
    vertex_shader_input_descs[0].format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    memcpy(vertex_shader_input_descs[1].semantic_name, "TEXCOORD", 8);
    vertex_shader_input_descs[1].semantic_name[8] = 0;
    vertex_shader_input_descs[1].format = DXGI_FORMAT_R32G32_FLOAT;

    // Init shaders
    CompiledShader vertex_shader_font_compiled =
        graphics::compile_vertex_shader(vertex_shader_font_string, ARRAYSIZE(vertex_shader_font_string));
    vertex_shader_font = graphics::get_vertex_shader(&vertex_shader_font_compiled, vertex_shader_input_descs, 2);
    graphics::release(&vertex_shader_font_compiled);

    CompiledShader pixel_shader_font_compiled =
        graphics::compile_pixel_shader(pixel_shader_font_string, ARRAYSIZE(pixel_shader_font_string));
    pixel_shader_font = graphics::get_pixel_shader(&pixel_shader_font_compiled);
    graphics::release(&pixel_shader_font_compiled);

    CompiledShader vertex_shader_rect_compiled =
        graphics::compile_vertex_shader(vertex_shader_rect_string, ARRAYSIZE(vertex_shader_rect_string));
    vertex_shader_rect = graphics::get_vertex_shader(&vertex_shader_rect_compiled, vertex_shader_input_descs, 1);
    graphics::release(&vertex_shader_rect_compiled);

    CompiledShader pixel_shader_rect_compiled =
        graphics::compile_pixel_shader(pixel_shader_rect_string, ARRAYSIZE(pixel_shader_rect_string));
    pixel_shader_rect = graphics::get_pixel_shader(&pixel_shader_rect_compiled);
    graphics::release(&pixel_shader_rect_compiled);

    // Init font
    File font_file = file_system::read_file("consola.ttf");
	
	FontData font_data;
	font_data.data = memory::alloc_heap<uint8_t>(font_file.size);
	memcpy(font_data.data, font_file.data, font_file.size);

    font_ui = font::get(&font_data, 24);

    file_system::release_file(font_file);
    free(font_data.data);

    array::init(&text_items, 100);
    array::init(&rect_items, 100);
    array::init(&rect_items_bg, 100);
}

// TODO: change screen size to arguments
static float SCREEN_WIDTH = 800, SCREEN_HEIGHT = 600;
void ui::draw_text(char *text, Font *font, float x, float y, Vector4 color)
{
    graphics::set_pixel_shader(&pixel_shader_font);
    graphics::set_vertex_shader(&vertex_shader_font);
    graphics::set_texture(&font->texture, 0);
    graphics::set_blend_state(&alpha_blend_state);
    graphics::set_constant_buffer(&buffer_pv, 0);
    graphics::set_constant_buffer(&buffer_model, 1);
    graphics::set_constant_buffer(&buffer_rect, 6);
    graphics::set_constant_buffer(&buffer_color, 7);

    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, SCREEN_WIDTH, 0, SCREEN_HEIGHT, -1, 1),
        math::get_identity()
    };
    graphics::update_constant_buffer(&buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&buffer_color, &color);

    y += font->top_pad;
    while(*text)
    {
        char c = *text;
        Glyph glyph = font->glyphs[c - 32];
        float rel_x = glyph.bitmap_x / 512.0f;
        float rel_y = glyph.bitmap_y / 512.0f;
        float rel_width = glyph.bitmap_width / 512.0f;
        float rel_height = glyph.bitmap_height / 512.0f;
        Vector4 source_rect = {rel_x, rel_y, rel_width, rel_height};
        graphics::update_constant_buffer(&buffer_rect, &source_rect);

        float xx = x;
        float yy = y;
        xx += glyph.x_offset;
        yy += glyph.y_offset;

        // TODO: solve y axis downwards in an elegant way
        Matrix4x4 model_matrix = math::get_translation(xx, SCREEN_HEIGHT - yy, 0) * math::get_scale((float)glyph.bitmap_width, (float)glyph.bitmap_height, 1.0f) * math::get_translation(Vector3(0.5f, -0.5f, 0.0f)) * math::get_scale(0.5f);
        graphics::update_constant_buffer(&buffer_model, &model_matrix);
        graphics::draw_mesh(&quad_mesh);
        if (*(text + 1)) x += font::get_kerning(font, c, *(text + 1));
        x += glyph.advance;

        text++;
    }
};

void ui::draw_text(char *text, Font *font, Vector2 pos, Vector4 color)
{
    ui::draw_text(text, font, pos.x, pos.y, color);
}

void ui::draw_rect(float x, float y, float width, float height, Vector4 color)
{
    graphics::set_pixel_shader(&pixel_shader_rect);
    graphics::set_vertex_shader(&vertex_shader_rect);
    graphics::set_blend_state(&alpha_blend_state);
    graphics::set_constant_buffer(&buffer_pv, 0);
    graphics::set_constant_buffer(&buffer_model, 1);
    graphics::set_constant_buffer(&buffer_color, 7);

    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, SCREEN_WIDTH, 0, SCREEN_HEIGHT, -1, 1),
        math::get_identity()
    };
    graphics::update_constant_buffer(&buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&buffer_color, &color);

    Matrix4x4 model_matrix = math::get_translation(x, SCREEN_HEIGHT - y, 0) * math::get_scale(width, height, 1.0f) * math::get_translation(Vector3(0.5f, -0.5f, 0.0f)) * math::get_scale(0.5f);
    graphics::update_constant_buffer(&buffer_model, &model_matrix);
    graphics::draw_mesh(&quad_mesh);
}

void ui::draw_rect(Vector2 pos, float width, float height, Vector4 color)
{
    ui::draw_rect(pos.x, pos.y, width, height, color);
}

const float vertical_padding = 15.0f;
const float horizontal_padding = 15.0f;
const float inner_padding = 5.0f;

Panel ui::start_panel(char *name, Vector2 pos, float width)
{
    Panel panel = {};

    panel.pos = pos;
    panel.width = width;
    panel.item_pos.x = horizontal_padding;
    panel.item_pos.y = font::get_row_height(&font_ui) + vertical_padding * 2.0f;
    panel.name = name;
    
    return panel;
}

Panel ui::start_panel(char *name, float x, float y, float width)
{
    return ui::start_panel(name, Vector2(x,y), width);
}

void ui::end_panel(Panel *panel)
{
    float panel_height = panel->item_pos.y + vertical_padding - inner_padding;
    Vector4 color = Vector4(0.2f, 0.2f, 0.2f, .7f);
    RectItem panel_bg = {color, panel->pos, Vector2(panel->width, panel_height)};
    array::add(&rect_items_bg, panel_bg);

    Vector4 title_color = Vector4(1, 0, 0, 1);
    Vector2 title_pos = panel->pos + Vector2(horizontal_padding, vertical_padding);
    TextItem title = {title_color, title_pos, panel->name};
    array::add(&text_items, title);
}

bool is_in_rect(Vector2 position, Vector2 rect_position, Vector2 rect_size)
{
    if (position.x >= rect_position.x && position.x <= rect_position.x + rect_size.x &&
        position.y >= rect_position.y && position.y <= rect_position.y + rect_size.y)
        return true;
    return false;
}

bool ui::add_toggle(Panel *panel, char *label, bool active)
{
    float height = font::get_row_height(&font_ui);
    Vector2 item_pos = panel->pos + panel->item_pos;
    
    // Toggle box background
    Vector4 box_bg_color = Vector4(0,1,0,1);
    Vector2 box_bg_size = Vector2(height, height);
    Vector2 box_bg_pos = item_pos;
    
    // Check for mouse input
    if(input::ui_active())
    {
        Vector2 mouse_position = input::mouse_position();
        if(is_in_rect(mouse_position, box_bg_pos, box_bg_size))
        {
            box_bg_color *= 0.8f;
            if(input::mouse_left_button_pressed()) active = !active;

            // In case some other element was active, now it shouldn't be
            active_id = -1;
        }
    }

    RectItem toggle_bg = {box_bg_color, box_bg_pos, box_bg_size};
    array::add(&rect_items, toggle_bg);

    if (active)
    {
        // Active part of toggle box
        Vector4 box_fg_color = Vector4(1,1,0,1);
        Vector2 box_fg_size = Vector2(height * 0.6f, height * 0.6f);
        Vector2 box_fg_pos = box_bg_pos + (box_bg_size - box_fg_size) / 2.0f;
        RectItem toggle_fg = {box_fg_color, box_fg_pos, box_fg_size};
        array::add(&rect_items, toggle_fg);
    }

    // Toggle label
    Vector4 text_color = Vector4(1,0,0,1);
    Vector2 text_pos = box_bg_pos + Vector2(inner_padding + box_bg_size.x, 0);
    TextItem toggle_label = {text_color, text_pos, label};
    array::add(&text_items, toggle_label);

    panel->item_pos.y += height + inner_padding;
    return active;
}

float ui::add_slider(Panel *panel, char *label, float pos, float min, float max)
{
    int32_t slider_id = hash_string(label);
    
    float height = font::get_row_height(&font_ui);
    float slider_width = 50.0f;
    Vector2 item_pos = panel->pos + panel->item_pos;
    
    // Slider bar
    Vector4 slider_bar_color = Vector4(0,1,0,1);
    Vector2 slider_bar_pos = item_pos + Vector2(0.0f, height * 0.25f);
    Vector2 slider_bar_size = Vector2(slider_width, height * 0.5f);
    RectItem slider_bar = { slider_bar_color, slider_bar_pos, slider_bar_size };
    array::add(&rect_items, slider_bar);

    Vector4 slider_color = Vector4(0,1,0,1);
    float slider_x = (pos - min) / max * slider_width;
    Vector2 slider_pos = item_pos + Vector2(slider_x, 0.0f);
    Vector2 slider_size = Vector2(height * 0.5f, height);
    // Check for mouse input
    if(input::ui_active())
    {
        Vector2 mouse_position = input::mouse_position();
        if(is_in_rect(mouse_position, slider_pos, slider_size))
        {
            slider_color *= 0.8f;
            slider_color.w = 1.0f;
            if(input::mouse_left_button_down())
            {
                active_id = slider_id;
            }
        }
        else if(!input::mouse_left_button_down())
        {
            active_id = -1;
        }

        if(slider_id == active_id)
        {
             float dx = input::mouse_delta_position().x;
             float x_movement = dx / slider_width;
             pos += x_movement;
             pos = math::clamp(pos, min, max);
        }
    }

    // Slider
    RectItem slider = { slider_color, slider_pos, slider_size };
    array::add(&rect_items, slider);

    // Toggle label
    Vector4 text_color = Vector4(1,0,0,1);
    Vector2 text_pos = item_pos + Vector2(inner_padding + slider_bar_size.x, 0);
    TextItem toggle_label = {text_color, text_pos, label};
    array::add(&text_items, toggle_label);

    panel->item_pos.y += height + inner_padding;
    return pos;
}

void ui::end()
{
    for(uint32_t i = 0; i < rect_items_bg.count; ++i)
    {
        RectItem *item = &rect_items_bg.data[i];
        ui::draw_rect(item->pos, item->size.x, item->size.y, item->color);
    }

    for(uint32_t i = 0; i < rect_items.count; ++i)
    {
        RectItem *item = &rect_items.data[i];
        ui::draw_rect(item->pos, item->size.x, item->size.y, item->color);
    }

    for(uint32_t i = 0; i < text_items.count; ++i)
    {
        TextItem *item = &text_items.data[i];
        ui::draw_text(item->text, &font_ui, item->pos, item->color);
    }

    array::reset(&rect_items_bg);
    array::reset(&rect_items);
    array::reset(&text_items);
}

void ui::release()
{
    graphics::release(&buffer_rect);
    graphics::release(&buffer_pv);
    graphics::release(&buffer_model);
    graphics::release(&buffer_color);
    graphics::release(&alpha_blend_state);

    graphics::release(&vertex_shader_font);
    graphics::release(&pixel_shader_font);
    graphics::release(&vertex_shader_rect);
    graphics::release(&pixel_shader_rect);

    font::release(&font_ui);
}