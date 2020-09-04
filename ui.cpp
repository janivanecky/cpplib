#include <cassert>
#include "ui.h"
#include "font.h"
#include "graphics.h"
#include "array.h"
#include "input.h"
#include "file_system.h"

static ConstantBuffer buffer_rect;
static ConstantBuffer buffer_pv;
static ConstantBuffer buffer_model;
static ConstantBuffer buffer_color;
static ConstantBuffer buffer_vertices;
static ConstantBuffer buffer_vertices_line;
static ConstantBuffer buffer_line_width;

static VertexShader vertex_shader_font;
static PixelShader pixel_shader_font;
static VertexShader vertex_shader_rect;
static PixelShader pixel_shader_rect;
static VertexShader vertex_shader_triangle;
static PixelShader pixel_shader_triangle;
static VertexShader vertex_shader_line;
static VertexShader vertex_shader_miter;
static PixelShader pixel_shader_line;

static TextureSampler texture_sampler;

static Mesh quad_mesh;
static Mesh triangle_mesh;
static Mesh line_mesh;
static Mesh miter_mesh;

static Font font_ui;

static File font_file;

static bool is_input_rensposive_ = true;
static bool is_registering_input_ = false;

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
"   return color;"
"}";

char vertex_shader_triangle_string[] =
"struct VertexInput"
"{"
"	float3 position: POSITION;"
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
"cbuffer VerticesBuffer : register(b1)"
"{"
"	float2x3 vertices;"
"};"
""
"VertexOutput main(VertexInput input)"
"{"
"	VertexOutput result;"
""
"   float4 pos = float4(input.position.xyz, 1.0f);"
"   pos.xy = mul(vertices, pos.xyz);"
"   pos.z = 0;"
"	result.svPosition = mul(projection, mul(view, pos));"
""
"	return result;"
"}";

char pixel_shader_triangle_string[] =
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


char vertex_shader_line_string[] =
"struct VertexInput"
"{"
"	float2 position: POSITION;"
"   uint instance_id: SV_InstanceID;"
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
"cbuffer LineVertices : register(b1)"
"{"
"	float4 vertices[100];"
"};"
""
"cbuffer LineSettings : register(b2)"
"{"
"	float width;"
"};"
""
"VertexOutput main(VertexInput input)"
"{"
"	VertexOutput result;"
""
"   float2 v1 = vertices[input.instance_id].xy;"
"   float2 v2 = vertices[input.instance_id + 1].xy;"
"   float2 length_axis = normalize(v2 - v1);"
"   float2 width_axis = float2(-length_axis.y, length_axis.x);"
"   float2 p = v1 + (v2 - v1) * input.position.x + width_axis * input.position.y * width * 0.5f;"
"   float4 pos = float4(p, 0.0f, 1.0f);"
"	result.svPosition = mul(projection, mul(view, pos));"
""
"	return result;"
"}";

char vertex_shader_miter_string[] =
"struct VertexInput"
"{"
"	float4 position: POSITION;"
"   uint instance_id: SV_InstanceID;"
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
"cbuffer LineVertices : register(b1)"
"{"
"	float4 vertices[100];"
"};"
""
"cbuffer LineSettings : register(b2)"
"{"
"	float width;"
"};"
""
"VertexOutput main(VertexInput input)"
"{"
"	VertexOutput result;"
""
"   float2 v1 = vertices[input.instance_id].xy;"
"   float2 v2 = vertices[input.instance_id + 1].xy;"
"   float2 v3 = vertices[input.instance_id + 2].xy;"
"   float2 length_axis_1 = normalize(v2 - v1);"
"   float2 length_axis_2 = normalize(v2 - v3);"
"   float2 width_axis_1 = float2(-length_axis_1.y, length_axis_1.x);"
"   float2 width_axis_2 = float2(-length_axis_2.y, length_axis_2.x);"

"   float2 tangent = normalize(normalize(v2 - v1) + normalize(v3 - v2));"
"   float2 miter = float2(-tangent.y, tangent.x);"
"   float s = sign(dot(v3 - v2, miter));"
"   float2 p1 = v2;"
"   float2 p2 = p1 - width_axis_1 * width * 0.5f * s;"
"   float2 p3 = p1 + width_axis_2 * width * 0.5f * s;"
"   float2 p4 = p1 + miter * length(p2 - p1) / dot(normalize(p2 - p1), miter);"
"   float2 p = mul(float2x4(p1.x, p2.x, p3.x, p4.x, p1.y, p2.y, p3.y, p4.y), input.position);"
"   float4 pos = float4(p, 0.0f, 1.0f);"
"	result.svPosition = mul(projection, mul(view, pos));"
""
"	return result;"
"}";

char pixel_shader_line_string[] =
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
    char text[100];
    Vector2 origin = Vector2(0,0);
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

static float quad_vertices[] = {
    -1.0, 1.0, 0.0, 1.0, // LEFT TOP 1
    0.0, 0.0,
    1.0, 1.0, 0.0, 1.0, // RIGHT TOP 2
    1.0, 0.0,
    -1.0, -1.0, 0.0, 1.0, // LEFT BOT 3
    0.0, 1.0,
    1.0, -1.0, 0.0, 1.0, // RIGHT BOT 4
    1.0, 1.0
};

static uint16_t quad_indices[] = {
    2, 3, 1,
    2, 1, 0
};

static float triangle_vertices[] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
};

static float line_vertices[] = {
    0.0f, -1.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,

    0.0f, -1.0f,
    1.0f, -1.0f,
    1.0f, 1.0f,
};

static float miter_vertices[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 0.0f,

    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};

static float screen_width = -1, screen_height = -1;

const float FONT_TEXTURE_SIZE = 256.0f;
const int32_t FONT_HEIGHT = 20;
const float ITEMS_WIDTH = 225.0f * 1.25f;

#define ASSERT_SCREEN_SIZE assert(screen_width > 0 && screen_height > 0)

void ui::init(float screen_width_ui, float screen_height_ui) {
    // Set screen size
    screen_width = screen_width_ui;
    screen_height = screen_height_ui;

    // Create constant buffers
    buffer_model = graphics::get_constant_buffer(sizeof(Matrix4x4));
    buffer_pv = graphics::get_constant_buffer(sizeof(Matrix4x4) * 2);
    buffer_rect = graphics::get_constant_buffer(sizeof(Vector4));
    buffer_color = graphics::get_constant_buffer(sizeof(Vector4));
    buffer_vertices = graphics::get_constant_buffer(sizeof(Vector4) * 3);
    buffer_vertices_line = graphics::get_constant_buffer(sizeof(Vector4) * 100);
    buffer_line_width = graphics::get_constant_buffer(sizeof(float));

    // Assert constant buffers are ready
    assert(graphics::is_ready(&buffer_model));
    assert(graphics::is_ready(&buffer_pv));
    assert(graphics::is_ready(&buffer_rect));
    assert(graphics::is_ready(&buffer_color));
    assert(graphics::is_ready(&buffer_vertices));
    assert(graphics::is_ready(&buffer_vertices_line));
    assert(graphics::is_ready(&buffer_line_width));

    // Create mesh
    quad_mesh = graphics::get_mesh(quad_vertices, 4, sizeof(float) * 6, quad_indices, 6, 2);
    assert(graphics::is_ready(&quad_mesh));
    triangle_mesh = graphics::get_mesh(triangle_vertices, 3, sizeof(float) * 3, NULL, 0, 0);
    assert(graphics::is_ready(&triangle_mesh));
    line_mesh = graphics::get_mesh(line_vertices, 6, sizeof(float) * 2, NULL, 0, 0);
    assert(graphics::is_ready(&line_mesh));
    miter_mesh = graphics::get_mesh(miter_vertices, 6, sizeof(float) * 4, NULL, 0, 0);
    assert(graphics::is_ready(&miter_mesh));

    // Create shaders
    vertex_shader_font = graphics::get_vertex_shader_from_code(vertex_shader_font_string, ARRAYSIZE(vertex_shader_font_string));
    pixel_shader_font = graphics::get_pixel_shader_from_code(pixel_shader_font_string, ARRAYSIZE(pixel_shader_font_string));
    vertex_shader_rect = graphics::get_vertex_shader_from_code(vertex_shader_rect_string, ARRAYSIZE(vertex_shader_rect_string));
    pixel_shader_rect = graphics::get_pixel_shader_from_code(pixel_shader_rect_string, ARRAYSIZE(pixel_shader_rect_string));
    vertex_shader_triangle = graphics::get_vertex_shader_from_code(vertex_shader_triangle_string, ARRAYSIZE(vertex_shader_triangle_string));
    pixel_shader_triangle = graphics::get_pixel_shader_from_code(pixel_shader_triangle_string, ARRAYSIZE(pixel_shader_triangle_string));
    vertex_shader_line = graphics::get_vertex_shader_from_code(vertex_shader_line_string, ARRAYSIZE(vertex_shader_line_string));
    vertex_shader_miter = graphics::get_vertex_shader_from_code(vertex_shader_miter_string, ARRAYSIZE(vertex_shader_miter_string));
    pixel_shader_line = graphics::get_pixel_shader_from_code(pixel_shader_line_string, ARRAYSIZE(pixel_shader_line_string));
    assert(graphics::is_ready(&vertex_shader_rect));
    assert(graphics::is_ready(&vertex_shader_font));
    assert(graphics::is_ready(&pixel_shader_font));
    assert(graphics::is_ready(&pixel_shader_rect));
    assert(graphics::is_ready(&vertex_shader_triangle));
    assert(graphics::is_ready(&pixel_shader_triangle));
    assert(graphics::is_ready(&vertex_shader_line));
    assert(graphics::is_ready(&vertex_shader_miter));
    assert(graphics::is_ready(&pixel_shader_line));

    // Create texture sampler
    texture_sampler = graphics::get_texture_sampler(SampleMode::CLAMP);
    assert(graphics::is_ready(&texture_sampler));

    // Init font
    font_file = file_system::read_file("consola.ttf");
    assert(font_file.data);
    font_ui = font::get((uint8_t *)font_file.data, font_file.size, FONT_HEIGHT, (uint32_t)FONT_TEXTURE_SIZE);
    assert(graphics::is_ready(&font_ui.texture));

    // Init rendering arrays
    array::init(&text_items, 100);
    array::init(&rect_items, 100);
    array::init(&rect_items_bg, 100);
}

void ui::draw_text(char *text, Font *font, float x, float y, Vector4 color, Vector2 origin) {
    ASSERT_SCREEN_SIZE;

    // Set font shaders
    graphics::set_pixel_shader(&pixel_shader_font);
    graphics::set_vertex_shader(&vertex_shader_font);

    // Set font texture
    graphics::set_texture(&font->texture, 0);
    graphics::set_texture_sampler(&texture_sampler, 0);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant buffers
    graphics::set_constant_buffer(&buffer_pv, 0);
    graphics::set_constant_buffer(&buffer_model, 1);
    graphics::set_constant_buffer(&buffer_rect, 6);
    graphics::set_constant_buffer(&buffer_color, 7);

    // Update constant buffer values
    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, screen_width, 0, screen_height, -1, 1),
        math::get_identity()
    };
    graphics::update_constant_buffer(&buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&buffer_color, &color);

    // Get final text dimensions
    float text_width = font::get_string_width(text, font);
    float text_height = font::get_row_height(font);

    // Adjust starting point based on the origin
    x = math::floor(x - origin.x * text_width);
    y = math::floor(y - origin.y * text_height);

    y += font->top_pad;
    while(*text)
    {
        char c = *text;
        Glyph glyph = font->glyphs[c - 32];

        // Set up source rectangle
        float rel_x = glyph.bitmap_x / FONT_TEXTURE_SIZE;
        float rel_y = glyph.bitmap_y / FONT_TEXTURE_SIZE;
        float rel_width = glyph.bitmap_width / FONT_TEXTURE_SIZE;
        float rel_height = glyph.bitmap_height / FONT_TEXTURE_SIZE;
        Vector4 source_rect = {rel_x, rel_y, rel_width, rel_height};
        graphics::update_constant_buffer(&buffer_rect, &source_rect);

        // Get final letter start
        float final_x = x + glyph.x_offset;
        float final_y = y + glyph.y_offset;

        // Set up model matrix
        // TODO: solve y axis downwards in an elegant way
        Matrix4x4 model_matrix = math::get_translation(final_x, screen_height - final_y, 0) * math::get_scale((float)glyph.bitmap_width, (float)glyph.bitmap_height, 1.0f) * math::get_translation(Vector3(0.5f, -0.5f, 0.0f)) * math::get_scale(0.5f);
        graphics::update_constant_buffer(&buffer_model, &model_matrix);

        graphics::draw_mesh(&quad_mesh);

        // Update current position for next letter
        if (*(text + 1)) x += font::get_kerning(font, c, *(text + 1));
        x += glyph.advance;
        text++;
    }

    // Reset previous blend state
    graphics::set_blend_state(old_blend_state);
};

void ui::draw_text(char *text, Font *font, Vector2 pos, Vector4 color, Vector2 origin) {
    ui::draw_text(text, font, pos.x, pos.y, color, origin);
}

void ui::draw_rect(float x, float y, float width, float height, Vector4 color) {
    ASSERT_SCREEN_SIZE;

    // Set rect shaders
    graphics::set_pixel_shader(&pixel_shader_rect);
    graphics::set_vertex_shader(&vertex_shader_rect);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&buffer_pv, 0);
    graphics::set_constant_buffer(&buffer_model, 1);
    graphics::set_constant_buffer(&buffer_color, 7);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, screen_width, 0, screen_height, -1, 1),
        math::get_identity()
    };
    Matrix4x4 model_matrix = math::get_translation(x, screen_height - y, 0) * math::get_scale(width, height, 1.0f) * math::get_translation(Vector3(0.5f, -0.5f, 0.0f)) * math::get_scale(0.5f);

    // Update constant buffers
    graphics::update_constant_buffer(&buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&buffer_color, &color);
    graphics::update_constant_buffer(&buffer_model, &model_matrix);

    // Render rect
    graphics::draw_mesh(&quad_mesh);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}

void ui::draw_rect(Vector2 pos, float width, float height, Vector4 color) {
    ui::draw_rect(pos.x, pos.y, width, height, color);
}

void ui::draw_triangle(Vector2 v1, Vector2 v2, Vector2 v3, Vector4 color) {
    ASSERT_SCREEN_SIZE;

    // Set rect shaders
    graphics::set_pixel_shader(&pixel_shader_triangle);
    graphics::set_vertex_shader(&vertex_shader_triangle);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&buffer_pv, 0);
    graphics::set_constant_buffer(&buffer_vertices, 1);
    graphics::set_constant_buffer(&buffer_color, 7);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, screen_width, 0, screen_height, -1, 1),
        math::get_identity()
    };
    v1.y = screen_height - v1.y;
    v2.y = screen_height - v2.y;
    v3.y = screen_height - v3.y;
    Vector4 vertices[3] = {Vector4(v1, 0, 0), Vector4(v2, 0, 0), Vector4(v3, 0, 0)};

    // Update constant buffers
    graphics::update_constant_buffer(&buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&buffer_color, &color);
    graphics::update_constant_buffer(&buffer_vertices, &vertices);

    // Render rect
    graphics::draw_mesh(&triangle_mesh);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}


void ui::draw_line(Vector2 *points, int point_count, float width, Vector4 color) {
    ASSERT_SCREEN_SIZE;

    // Set rect shaders
    graphics::set_pixel_shader(&pixel_shader_line);
    graphics::set_vertex_shader(&vertex_shader_line);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&buffer_pv, 0);
    graphics::set_constant_buffer(&buffer_vertices_line, 1);
    graphics::set_constant_buffer(&buffer_line_width, 2);
    graphics::set_constant_buffer(&buffer_color, 7);

    // TODO: Switch to using temporary mem buffer instead of heap
    Vector4 *real_points = memory::alloc_heap<Vector4>(point_count);
    for(int i = 0; i < point_count; ++i) {
        real_points[i].x = points[i].x;
        real_points[i].y = screen_height - points[i].y;
    }
    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, screen_width, 0, screen_height, -1, 1),
        math::get_identity()
    };

    // Update constant buffers
    graphics::update_constant_buffer(&buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&buffer_color, &color);
    graphics::update_constant_buffer(&buffer_vertices_line, real_points);
    graphics::update_constant_buffer(&buffer_line_width, &width);

    // Render rect
    graphics_context->context->IASetVertexBuffers(0, 1, &line_mesh.vertex_buffer, &line_mesh.vertex_stride, &line_mesh.vertex_offset);
	graphics_context->context->IASetPrimitiveTopology(line_mesh.topology);
    graphics_context->context->DrawInstanced(line_mesh.vertex_count, point_count - 1, 0, 0);

    graphics::set_vertex_shader(&vertex_shader_miter);
    graphics_context->context->IASetVertexBuffers(0, 1, &miter_mesh.vertex_buffer, &miter_mesh.vertex_stride, &miter_mesh.vertex_offset);
	graphics_context->context->IASetPrimitiveTopology(miter_mesh.topology);
    graphics_context->context->DrawInstanced(miter_mesh.vertex_count, point_count - 2, 0, 0);

    memory::free_heap(real_points);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}

const float vertical_padding = 5.0f;
const float horizontal_padding = 15.0f;
const float inner_padding = 5.0f;
int32_t active_id = -1;
int32_t hot_id = -1;


#define COLOR(r,g,b,a) Vector4((r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a) / 255.0f)
// This means that the color has been NOT gamma corrected - it was seen displayed incorrectly.
#define COLOR_LINEAR(r,g,b,a) Vector4(math::pow((r) / 255.0f, 2.2f), math::pow((g) / 255.0f, 2.2f), math::pow((b) / 255.0f, 2.2f), math::pow((a) / 255.0f, 2.2f))
static Vector4 color_background = Vector4(0.1f, 0.1f, 0.1f, .9f);
static Vector4 color_foreground = Vector4(0.8f, 0.8f, 0.8f, 0.6f);
//static Vector4 color_foreground = Vector4(0.0f, 0.2f, 0.1f, 0.8f);
static Vector4 color_title = COLOR_LINEAR(28, 224, 180, 255);
static Vector4 color_label = Vector4(1.0f,1.0f,1.0f,0.8f);
//static Vector4 color_label = Vector4(0.0f,1.0f,0.5f,0.8f);

Panel ui::start_panel(char *name, Vector2 pos, float width) {
    Panel panel = {};

    panel.pos = pos;
    panel.width = width;
    panel.item_pos.x = horizontal_padding;
    panel.item_pos.y = font::get_row_height(&font_ui) + inner_padding;
    panel.name = name;

    return panel;
}

Panel ui::start_panel(char *name, float x, float y, float width) {
    return ui::start_panel(name, Vector2(x,y), width);
}

Vector4 ui::get_panel_rect(Panel *panel) {
    Vector4 result = Vector4(panel->pos.x, panel->pos.y, panel->width, panel->item_pos.y + horizontal_padding - inner_padding);
    return result;
}

void ui::end_panel(Panel *panel) {
    Vector4 panel_rect = get_panel_rect(panel);
    RectItem panel_bg = {color_background, Vector2(panel_rect.x, panel_rect.y), Vector2(panel_rect.z, panel_rect.w)};
    array::add(&rect_items_bg, panel_bg);

    //float title_bar_height = font::get_row_height(&font_ui);
    //RectItem title_bar = {color_foreground, panel->pos, Vector2(panel->width, title_bar_height)};
    //array::add(&rect_items_bg, title_bar);

    Vector2 title_pos = panel->pos + Vector2(horizontal_padding, 0);
    TextItem title = {};
    title.color = color_label;
    title.pos = title_pos;
    memcpy(title.text, panel->name, strlen(panel->name) + 1);
    array::add(&text_items, title);
}

bool is_in_rect(Vector2 position, Vector2 rect_position, Vector2 rect_size) {
    if (position.x >= rect_position.x && position.x <= rect_position.x + rect_size.x &&
        position.y >= rect_position.y && position.y <= rect_position.y + rect_size.y)
        return true;
    return false;
}

void unset_hot(int32_t item_id) {
    if(hot_id == item_id) {
        hot_id = -1;
    }
}

void unset_active(int32_t item_id) {
    if(active_id == item_id) {
        active_id = -1;
        is_registering_input_ = false;
    }
}

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

bool ui::add_toggle(Panel *panel, char *label, int *active) {
    bool a = *active;
    bool changed = add_toggle(panel, label, &a);
    *active = a;
    return changed;
}

bool ui::add_toggle(Panel *panel, char *label, bool *active) {
    bool changed = false;
    const float box_middle_to_total = 0.8f;

    float height = font::get_row_height(&font_ui);
    Vector2 item_pos = panel->pos + panel->item_pos;
    int32_t toggle_id = hash_string(label);

    Vector2 box_bg_size = Vector2(height, height);
    Vector2 box_bg_pos = item_pos;

    float color_modifier = 0.8f;

    // Check for mouse input
    if(ui::is_input_responsive()) {
        // Check if mouse over
        Vector2 mouse_position = input::mouse_position();
        if(is_in_rect(mouse_position, box_bg_pos, box_bg_size)) {
            set_hot(toggle_id);
        } else {
        // Remove hotness only if this toggle was hot before
            unset_hot(toggle_id);
        }

        // If toggle is hot, check for mouse press
        if (is_hot(toggle_id) && input::mouse_left_button_pressed()) {
            *active = !(*active);
            changed = true;

            set_active(toggle_id);
        } else if(is_active(toggle_id) && !input::mouse_left_button_down()) {
            unset_active(toggle_id);
        }
    } else {
        unset_hot(toggle_id);
    }

    // Toggle box background
    Vector4 color_box = color_foreground;
    Vector4 color_middle = color_background * 0.8f;
    color_middle.a = 1.0f;
    if (is_hot(toggle_id)) {
        color_modifier = 1.0f;
    }

    // Draw bg rectangle
    float box_border_width = 2;
    {
        Vector2 box_size = Vector2(box_bg_size.x, box_border_width);
        Vector2 box_pos = box_bg_pos;
        RectItem border = {color_box * color_modifier, box_pos, box_size};
        array::add(&rect_items, border);

        box_pos.y += box_bg_size.y - box_border_width;
        border = {color_box * color_modifier, box_pos, box_size};
        array::add(&rect_items, border);

        box_size = Vector2(box_border_width, box_bg_size.y - box_border_width * 2);
        box_pos = box_bg_pos + Vector2(0, box_border_width);
        border = {color_box * color_modifier, box_pos, box_size};
        array::add(&rect_items, border);

        box_pos.x += box_bg_size.x - box_border_width;
        border = {color_box * color_modifier, box_pos, box_size};
        array::add(&rect_items, border);
    }

    // Active part of toggle box
    if (*active) {
        Vector2 box_fg_size = Vector2(height - box_border_width * 4.0f, height - box_border_width * 4.0f);
        Vector2 box_fg_pos = box_bg_pos + (box_bg_size - box_fg_size) / 2.0f;
        RectItem toggle_fg = {color_box * color_modifier, box_fg_pos, box_fg_size};
        array::add(&rect_items, toggle_fg);
    }

    // Draw toggle label
    Vector2 text_pos = box_bg_pos + Vector2(inner_padding + box_bg_size.x, 0);
    TextItem toggle_label = {};
    toggle_label.color = color_label * color_modifier;
    toggle_label.pos = text_pos;
    memcpy(toggle_label.text, label, strlen(label) + 1);
    array::add(&text_items, toggle_label);

    // Move current panel item position
    panel->item_pos.y += height + inner_padding;
    return changed;
}

bool ui::add_slider(Panel *panel, char *label, int *pos, int min, int max) {
    float pos_f = float(*pos);
    bool changed = add_slider(panel, label, &pos_f, float(min), float(max));
    *pos = int(pos_f);
    return changed;
}

bool ui::add_slider(Panel *panel, char *label, float *pos, float min, float max) {
    bool changed = false;
    int32_t slider_id = hash_string(label);
    Vector2 item_pos = panel->pos + panel->item_pos;

    float height = font::get_row_height(&font_ui);
    float slider_width = ITEMS_WIDTH;

    // Slider bar
    Vector2 slider_bar_pos = item_pos;
    Vector2 slider_bar_size = Vector2(slider_width, height);

    // Check for mouse input
    if(ui::is_input_responsive()) {
        Vector2 mouse_position = input::mouse_position();
        if(is_in_rect(mouse_position, slider_bar_pos, slider_bar_size)) {
            set_hot(slider_id);
        } else if(!is_active(slider_id)) {
            unset_hot(slider_id);
        }

        if(is_hot(slider_id) && !is_active(slider_id) && input::mouse_left_button_down()) {
            set_active(slider_id);
        } else if (is_active(slider_id) && !input::mouse_left_button_down()) {
            unset_active(slider_id);
        }
    } else {
        unset_hot(slider_id);
        unset_active(slider_id);
    }

    float bounds_width = 2.0f;
    float color_modifier = 0.8f;
    if(is_hot(slider_id)) {
        bounds_width *= 2.0f;
        color_modifier = 1.0f;
    }

    Vector4 bounds_color = color_label * color_modifier;
    Vector2 min_bound_pos = item_pos + Vector2(-bounds_width, 0.0f);
    Vector2 min_bound_size = Vector2(bounds_width, height);
    RectItem min_bound = { bounds_color, min_bound_pos, min_bound_size };
    array::add(&rect_items, min_bound);

    Vector2 max_bound_pos = item_pos + Vector2(slider_bar_size.x, 0.0f);
    Vector2 max_bound_size = Vector2(bounds_width, height);
    RectItem max_bound = { bounds_color, max_bound_pos, max_bound_size };
    array::add(&rect_items, max_bound);

    Vector4 slider_color = color_foreground * color_modifier;
    Vector2 slider_size = Vector2((*pos - min) / (max - min) * slider_width, height);
    Vector2 slider_pos = slider_bar_pos;

    // Max number
    Vector2 current_pos = Vector2(slider_bar_pos.x + slider_bar_size.x / 2.0f, item_pos.y);
    TextItem current_label = {};
    current_label.color = color_label * color_modifier;
    current_label.pos = current_pos;
    current_label.origin = Vector2(0.5f, 0.0f);
    sprintf_s(current_label.text, ARRAYSIZE(current_label.text), "%.2f", *pos);
    array::add(&text_items, current_label);

    if(is_active(slider_id)) {
        float mouse_x = input::mouse_position().x;
        float mouse_x_rel = (mouse_x - slider_bar_pos.x) / (slider_bar_size.x);
        mouse_x_rel = math::clamp(mouse_x_rel, 0.0f, 1.0f);

        *pos = mouse_x_rel * (max - min) + min;

        changed = true;
    }

    // Slider
    RectItem slider = { slider_color, slider_pos, slider_size };
    array::add(&rect_items, slider);

    // Slider label
    Vector2 text_pos = Vector2(slider_bar_size.x + slider_bar_pos.x + inner_padding + 2, item_pos.y);
    TextItem slider_label = {};
    slider_label.color = color_label * color_modifier;
    slider_label.pos = text_pos;
    memcpy(slider_label.text, label, strlen(label) + 1);
    array::add(&text_items, slider_label);

    panel->item_pos.y += height + inner_padding;
    return changed;
}

bool ui::add_combobox(Panel *panel, char *label, char **values, int value_count, int *selected_value, bool *expanded) {
    bool changed = false;
    int32_t slider_id = hash_string(label);
    float height = font::get_row_height(&font_ui);

    float bounds_width = 2.0f;
    float color_modifier = .8f;
    float text_field_width = ITEMS_WIDTH;

    Vector2 position = panel->pos + panel->item_pos;
    TextItem selected_value_item = {};
    selected_value_item.pos = position + Vector2(2 + inner_padding, 0);

    if(ui::is_input_responsive()) {
        Vector2 mouse_position = input::mouse_position();
        if(is_in_rect(mouse_position, selected_value_item.pos, Vector2(text_field_width, height))) {
            set_hot(slider_id);
        } else if(!is_active(slider_id)) {
            unset_hot(slider_id);
        }
        if(is_hot(slider_id) && !is_active(slider_id) && input::mouse_left_button_down()) {
            set_active(slider_id);
            *expanded = !*expanded;
        } else if (is_active(slider_id) && !input::mouse_left_button_down()) {
            unset_active(slider_id);
        }
    } else {
        unset_hot(slider_id);
        unset_active(slider_id);
    }

    if(is_hot(slider_id)) {
        color_modifier = 1.0f;
        bounds_width *= 2.0f;
    }

    selected_value_item.color = color_label * color_modifier;
    memcpy(selected_value_item.text, values[*selected_value], strlen(values[*selected_value]) + 1);
    array::add(&text_items, selected_value_item);

    // Slider label
    Vector2 text_pos = panel->pos + panel->item_pos + Vector2(text_field_width + inner_padding + 2, 0);
    TextItem slider_label = {};
    slider_label.color = color_label * color_modifier;
    slider_label.pos = text_pos;
    memcpy(slider_label.text, label, strlen(label) + 1);
    array::add(&text_items, slider_label);

    Vector2 arrow_center = Vector2(text_pos.x - inner_padding - 20, text_pos.y + height / 2.0f);
    float arrow_height = height * 0.5f;
    float arrow_width = 2.0f * arrow_height / math::sqrt(3.0f);
    if(*expanded) {
        Vector2 v1 = arrow_center + Vector2(-arrow_width / 2.0f, arrow_height / 3.0f);
        Vector2 v2 = arrow_center + Vector2(arrow_width / 2.0f, arrow_height / 3.0f);
        Vector2 v3 = arrow_center + Vector2(0, -arrow_height * 2.0f / 3.0f);

        ui::draw_triangle(v1, v2, v3, color_label * color_modifier);
    } else {
        Vector2 v1 = arrow_center + Vector2(-arrow_width / 2.0f, -arrow_height / 3.0f);
        Vector2 v2 = arrow_center + Vector2(arrow_width / 2.0f, -arrow_height / 3.0f);
        Vector2 v3 = arrow_center + Vector2(0, arrow_height * 2.0f / 3.0f);

        ui::draw_triangle(v1, v2, v3, color_label * color_modifier);
    }

    Vector4 bounds_color = color_label * color_modifier;
    Vector2 min_bound_pos = panel->pos + panel->item_pos + Vector2(-bounds_width, 0.0f);
    Vector2 min_bound_size = Vector2(bounds_width, height);
    RectItem min_bound = { bounds_color, min_bound_pos, min_bound_size };
    array::add(&rect_items, min_bound);

    Vector2 max_bound_pos = panel->pos + panel->item_pos + Vector2(text_field_width, 0.0f);
    Vector2 max_bound_size = Vector2(bounds_width, height);
    RectItem max_bound = { bounds_color, max_bound_pos, max_bound_size };
    array::add(&rect_items, max_bound);

    position.y += height + inner_padding;
    color_modifier = 1.0f;
    bounds_width = 2.0f;
    if(*expanded) {
        for (int i = 0; i < value_count; ++i) {
            float color_modifier = 0.8f;
            // Slider label
            Vector2 text_pos = position;
            TextItem slider_label = {};
            slider_label.pos = text_pos + Vector2(2 + inner_padding, 0);

            Vector2 mouse_position = input::mouse_position();
            bool mouse_over = is_in_rect(mouse_position, slider_label.pos, Vector2(text_field_width, height));
            if(mouse_over) {
                color_modifier = 1.0f;
                bounds_width *= 2.0f;
                if(input::mouse_left_button_pressed()) {
                    set_active(slider_id);
                    *selected_value = i;
                    *expanded = false;
                    changed = true;
                }
            }

            if(*selected_value == i || mouse_over) {
                if(mouse_over) color_modifier = 1.0f;

                Vector4 bounds_color = color_label * color_modifier;
                Vector2 min_bound_pos = text_pos + Vector2(-bounds_width, 0.0f);
                Vector2 min_bound_size = Vector2(bounds_width, height);
                RectItem min_bound = { bounds_color, min_bound_pos, min_bound_size };
                array::add(&rect_items, min_bound);

                Vector2 max_bound_pos = text_pos + Vector2(text_field_width, 0.0f);
                Vector2 max_bound_size = Vector2(bounds_width, height);
                RectItem max_bound = { bounds_color, max_bound_pos, max_bound_size };
                array::add(&rect_items, max_bound);
            }

            slider_label.color = color_label * color_modifier;
            memcpy(slider_label.text, values[i], strlen(values[i]) + 1);
            array::add(&text_items, slider_label);
            position.y += height + inner_padding;
        }
    }

    panel->item_pos.y += height + inner_padding + (*expanded ? (height + inner_padding) * value_count : 0);
    return changed;
}

bool ui::add_function_plot(Panel *panel, char *label, float *x, float *y, int point_count, float *select_x, float select_y) {
    int32_t plot_id = hash_string(label);
    float height = font::get_row_height(&font_ui);
    float plot_width = ITEMS_WIDTH;
    float color_modifier = .8f;
    Vector2 item_pos = panel->pos + panel->item_pos;
    float plot_box_padding = 5.0f;

    Vector2 plot_box_pos = item_pos + Vector2(0, plot_box_padding);
    Vector2 plot_box_size = Vector2(plot_width, height * 4 - plot_box_padding * 2.0f);

    if(ui::is_input_responsive()) {
        Vector2 mouse_position = input::mouse_position();
        if(is_in_rect(mouse_position, plot_box_pos, plot_box_size)) {
            set_hot(plot_id);
        } else if(!is_active(plot_id)) {
            unset_hot(plot_id);
        }
        if(is_hot(plot_id) && !is_active(plot_id) && input::mouse_left_button_down()) {
            set_active(plot_id);
        } else if (is_active(plot_id) && !input::mouse_left_button_down()) {
            unset_active(plot_id);
        }
    } else {
        unset_hot(plot_id);
        unset_active(plot_id);
    }

    float bounds_width = 2.0f;
    if(is_hot(plot_id)) {
        color_modifier = 1.0f;
        bounds_width *= 2.0f;
    }

    float min_x = 1000.0f, max_x = -1000.0f;
    float min_y = 1000.0f, max_y = -1000.0f;
    #undef min
    #undef max
    for(int i = 0; i < point_count; ++i) {
        min_x = math::min(min_x, x[i]);
        max_x = math::max(max_x, x[i]);
        min_y = math::min(min_y, y[i]);
        max_y = math::max(max_y, y[i]);
    }

    float new_x_value = *select_x;
    if(is_active(plot_id)) {
        Vector2 mouse_position = input::mouse_position();
        Vector2 pos_relative_to_plot = mouse_position - plot_box_pos;
        Vector2 pos_relative_in_plot = Vector2(
            pos_relative_to_plot.x / plot_box_size.x,
            pos_relative_to_plot.y / plot_box_size.y
        );
        new_x_value = math::clamp(pos_relative_in_plot.x, 0.0f, 1.0f) * (max_x - min_x) + min_x;
    }

    Vector4 y_axis_color = color_label * color_modifier;
    Vector2 y_axis_size = Vector2(bounds_width, plot_box_size.y + 2 * plot_box_padding);

    Vector2 y_axis_min_pos = item_pos + Vector2(-bounds_width, 0.0f);
    RectItem y_axis_min = { y_axis_color, y_axis_min_pos, y_axis_size };
    array::add(&rect_items, y_axis_min);

    Vector2 y_axis_max_pos = item_pos + Vector2(plot_box_size.x, 0.0f);
    RectItem y_axis_max = { y_axis_color, y_axis_max_pos, y_axis_size };
    array::add(&rect_items, y_axis_max);

    Vector2 text_pos = panel->pos + panel->item_pos + Vector2(plot_width + inner_padding + 2, 0);
    TextItem slider_label = {};
    slider_label.color = color_label * color_modifier;
    slider_label.pos = text_pos;
    memcpy(slider_label.text, label, strlen(label) + 1);
    array::add(&text_items, slider_label);

    Vector2 select_text_pos = panel->pos + panel->item_pos + Vector2(plot_width + inner_padding + 2, height);
    TextItem select_text = {};
    select_text.color = color_label * color_modifier;
    select_text.pos = select_text_pos;
    sprintf_s(select_text.text, 40, "[%.2f, %.2f]", *select_x, select_y);
    array::add(&text_items, select_text);

    Vector4 grid_color = color_label * color_modifier * 0.5f;
    float grid_thickness = 2.0f;
    for(int i = 0; i < 5; ++i) {
        Vector2 grid_pos = plot_box_pos + Vector2(0, plot_box_size.y / 4.0f * i);
        Vector2 grid_size = Vector2(plot_box_size.x, grid_thickness);
        RectItem grid = { grid_color, grid_pos, grid_size };
        array::add(&rect_items, grid);
    }

    for(int i = 0; i < 8; ++i) {
        Vector2 grid_pos = plot_box_pos + Vector2((plot_box_size.x + grid_thickness) / 7.0f * i - grid_thickness, 0);
        Vector2 grid_size = Vector2(grid_thickness, plot_box_size.y);
        RectItem grid = { grid_color, grid_pos, grid_size };
        array::add(&rect_items, grid);
    }

    // TODO: Use temporary mem buffer instead of heap
    Vector2 *points = memory::alloc_heap<Vector2>(point_count);
    for(int i = 0; i < point_count; ++i) {
        float x_plot = (x[i] - min_x) / (max_x - min_x);
        float y_plot = (y[i] - min_y) / (max_y - min_y);
        points[i].x = plot_box_pos.x + x_plot * plot_box_size.x;
        points[i].y = plot_box_pos.y + plot_box_size.y - y_plot * plot_box_size.y;
    }
    Vector4 line_color = color_label * color_modifier;
    line_color.x *= 0.6f;
    line_color.y *= 0.6f;
    line_color.z *= 0.6f;
    ui::draw_line(points, point_count, 2.0f, line_color);

    Vector4 select_box_color = color_label * color_modifier;
    Vector2 select_box_size = Vector2(3.0f, plot_box_size.y);
    Vector2 select_box_pos = Vector2(
        plot_box_pos.x + math::clamp((*select_x - min_x) / (max_x - min_x), 0.0f, 1.0f) * plot_box_size.x - select_box_size.x * 0.5f,
        plot_box_pos.y
    );
    RectItem select_box = { select_box_color, select_box_pos, select_box_size };
    array::add(&rect_items, select_box);

    select_box_size = Vector2(3.0f, 3.0f);
    select_box_pos = Vector2(
        plot_box_pos.x + math::clamp((*select_x - min_x) / (max_x - min_x), 0.0f, 1.0f) * plot_box_size.x - select_box_size.x * 0.5f,
        plot_box_pos.y + plot_box_size.y - math::clamp((select_y - min_y) / (max_y - min_y), 0.0f, 1.0f) * plot_box_size.y + select_box_size.y * 0.5f
    );
    select_box = { select_box_color, select_box_pos, select_box_size };
    array::add(&rect_items, select_box);


    panel->item_pos.y += height * 4 + inner_padding;
    memory::free_heap(points);
    *select_x = new_x_value;
    return false;
}

bool ui::add_textbox(Panel *panel, char *label, char *text, int buffer_size, int *cursor_position) {
    int32_t textfield_id = hash_string(label);
    float height = font::get_row_height(&font_ui);
    float color_modifier = 0.8f;
    float textbox_width = ITEMS_WIDTH;
    float bounds_width = 2.0f;

    Vector2 textfield_pos = panel->pos + panel->item_pos + Vector2(inner_padding, 0.0f);
    Vector2 textfield_size = Vector2(textbox_width, height);

    if(ui::is_input_responsive()) {
        Vector2 mouse_position = input::mouse_position();
        if(is_in_rect(mouse_position, textfield_pos, textfield_size)) {
            set_hot(textfield_id);
        } else if(!is_active(textfield_id)) {
            unset_hot(textfield_id);
        } else if(!is_hot(textfield_id) && is_active(textfield_id) && input::mouse_left_button_pressed()) {
            unset_active(textfield_id);
        }
        if(is_hot(textfield_id) && !is_active(textfield_id) && input::mouse_left_button_down()) {
            set_active(textfield_id);
        } else if (is_active(textfield_id) && !input::mouse_left_button_down()) {
            unset_hot(textfield_id);
        }
    } else {
        unset_hot(textfield_id);
        unset_active(textfield_id);
    }

    if(is_hot(textfield_id) || is_active(textfield_id)) {
        color_modifier = 1.0f;
        bounds_width *= 2.0f;
    }

    if(is_active(textfield_id)) {
        if(input::key_pressed(KeyCode::LEFT)) {
            *cursor_position -= 1;
            if(*cursor_position < 0) {
                *cursor_position = 0;
            }
        } else if(input::key_pressed(KeyCode::RIGHT)) {
            *cursor_position += 1;
            int total_text_length = int(strlen(text));
            if(*cursor_position > total_text_length) {
                *cursor_position = total_text_length;
            }
        }
        if(input::key_pressed(KeyCode::DEL)) {
            int total_text_length = int(strlen(text));
            if(*cursor_position < total_text_length) {
                memcpy(text + *cursor_position, text + *cursor_position + 1, total_text_length - *cursor_position - 1);
                text[total_text_length - 1] = 0;
            }
        } else if (input::key_pressed(KeyCode::BACKSPACE)) {
            if(*cursor_position > 0) {
                int total_text_length = int(strlen(text));
                memcpy(text + *cursor_position - 1, text + *cursor_position, total_text_length - *cursor_position);
                text[total_text_length - 1] = 0;
                *cursor_position -= 1;
            }
        } else if (input::key_pressed(KeyCode::HOME)) {
            *cursor_position = 0;
        } else if (input::key_pressed(KeyCode::END)) {
            int total_text_length = int(strlen(text));
            *cursor_position = total_text_length;
        }
        int total_text_length = int(strlen(text));
        int characters_entered_count = input::characters_entered(NULL);
        if(*cursor_position + characters_entered_count < buffer_size) {
            memcpy(text + *cursor_position + characters_entered_count, text + *cursor_position, total_text_length - *cursor_position);
            input::characters_entered(text + *cursor_position);
            *cursor_position += characters_entered_count;
            // Add NULL terminating character.
            text[total_text_length + characters_entered_count] = 0;
        }
        // Text input
        // Shift selection handling
        // Ctrl handling
    }

    int max_chars_shown = textfield_size.x / font::get_string_width("A", &font_ui);
    int pre_cursor_max_chars = max_chars_shown / 2;
    int first_char_shown = math::max(0, *cursor_position - pre_cursor_max_chars);
    int last_char_shown = math::min(strlen(text) + 1, first_char_shown + max_chars_shown);
    if (last_char_shown == strlen(text) + 1) {
        first_char_shown = math::max(0.0f, int(strlen(text)) + 1 - max_chars_shown);
    }

    TextItem textbox_label = {};
    Vector2 label_pos = panel->pos + panel->item_pos + Vector2(textbox_width + inner_padding + 2, 0);
    textbox_label.color = color_label * color_modifier;
    textbox_label.pos = label_pos;
    memcpy(textbox_label.text, label, strlen(label) + 1);
    array::add(&text_items, textbox_label);

    TextItem textbox_text_pre_cursor = {};
    Vector2 text_pos = textfield_pos;
    textbox_text_pre_cursor.color = color_label * color_modifier;
    textbox_text_pre_cursor.pos = text_pos;
    int cursor_loc = *cursor_position;
    int pre_cursor_text_start = first_char_shown;
    int pre_cursor_text_end = cursor_loc;
    memcpy(textbox_text_pre_cursor.text, text + pre_cursor_text_start, pre_cursor_text_end - pre_cursor_text_start);
    textbox_text_pre_cursor.text[pre_cursor_text_end - pre_cursor_text_start] = 0;
    array::add(&text_items, textbox_text_pre_cursor);

    if(cursor_loc < strlen(text)) {
        TextItem textbox_text_post_cursor = {};
        textbox_text_post_cursor.color = color_label * color_modifier;
        textbox_text_post_cursor.pos = text_pos + Vector2(font::get_string_width(text + first_char_shown, cursor_loc + 1 - first_char_shown, &font_ui), 0.0f);
        int post_cursor_text_start = cursor_loc + 1;
        int post_cursor_text_end = last_char_shown;
        memcpy(textbox_text_post_cursor.text, text + post_cursor_text_start, post_cursor_text_end - post_cursor_text_start);
        textbox_text_post_cursor.text[post_cursor_text_end - post_cursor_text_start] = 0;
        array::add(&text_items, textbox_text_post_cursor);
    }

    TextItem textbox_text_cursor = {};
    textbox_text_cursor.color = color_label * color_modifier;
    if(is_active(textfield_id)) {
        textbox_text_cursor.color.x = 1.0f - textbox_text_cursor.color.x;
        textbox_text_cursor.color.y = 1.0f - textbox_text_cursor.color.y;
        textbox_text_cursor.color.z = 1.0f - textbox_text_cursor.color.z;
        textbox_text_cursor.color.w = 1.0f - textbox_text_cursor.color.z;
    }
    textbox_text_cursor.pos = text_pos + Vector2(font::get_string_width(text + first_char_shown, cursor_loc - first_char_shown, &font_ui), 0.0f);
    memcpy(textbox_text_cursor.text, text + cursor_loc, 1);
    textbox_text_cursor.text[1] = 0;
    array::add(&text_items, textbox_text_cursor);

    Vector4 bounds_color = color_label * color_modifier;
    Vector2 min_bound_pos = panel->pos + panel->item_pos + Vector2(-bounds_width, 0.0f);
    Vector2 min_bound_size = Vector2(bounds_width, height);
    RectItem min_bound = { bounds_color, min_bound_pos, min_bound_size };
    array::add(&rect_items, min_bound);

    Vector2 max_bound_pos = panel->pos + panel->item_pos + Vector2(textbox_width, 0.0f);
    Vector2 max_bound_size = Vector2(bounds_width, height);
    RectItem max_bound = { bounds_color, max_bound_pos, max_bound_size };
    array::add(&rect_items, max_bound);

    if(is_active(textfield_id)) {
        Vector2 cursor_pos = textfield_pos + Vector2(font::get_string_width(text + first_char_shown, cursor_loc - first_char_shown, &font_ui), 0.0f);
        Vector2 cursor_size = Vector2(font::get_string_width("A", 1, &font_ui), height);
        RectItem cursor = { color_label * color_modifier, cursor_pos, cursor_size };
        array::add(&rect_items, cursor);
    }

    panel->item_pos.y += height + inner_padding;
    return false;
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
        ui::draw_text(item->text, &font_ui, item->pos, item->color, item->origin);
    }

    array::reset(&rect_items_bg);
    array::reset(&rect_items);
    array::reset(&text_items);
}

void ui::set_input_responsive(bool is_responsive)
{
    is_input_rensposive_ = is_responsive;
}

bool ui::is_input_responsive()
{
    return is_input_rensposive_;
}

bool ui::is_registering_input()
{
    return is_registering_input_;
}

float ui::get_screen_width()
{
    return screen_width;
}

Font *ui::get_font()
{
    return &font_ui;
}

void ui::set_background_opacity(float opacity) {
    color_background.w = opacity;
}


void ui::release()
{
    graphics::release(&buffer_rect);
    graphics::release(&buffer_pv);
    graphics::release(&buffer_model);
    graphics::release(&buffer_color);

    graphics::release(&quad_mesh);
    graphics::release(&texture_sampler);

    graphics::release(&vertex_shader_font);
    graphics::release(&pixel_shader_font);
    graphics::release(&vertex_shader_rect);
    graphics::release(&pixel_shader_rect);

    font::release(&font_ui);
    file_system::release_file(font_file);
}