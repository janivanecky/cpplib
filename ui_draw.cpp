#include <cassert>
#include "ui_draw.h"
#include "font.h"
#include "graphics.h"
#include "file_system.h"

namespace ui_draw_private {

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
    static Texture2D font_ui_texture;

    static File font_file;

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
}


void ui_draw::init(float screen_width_ui, float screen_height_ui) {
    // Set screen size
    ui_draw_private::screen_width = screen_width_ui;
    ui_draw_private::screen_height = screen_height_ui;

    // Create constant buffers
    ui_draw_private::buffer_model = graphics::get_constant_buffer(sizeof(Matrix4x4));
    ui_draw_private::buffer_pv = graphics::get_constant_buffer(sizeof(Matrix4x4) * 2);
    ui_draw_private::buffer_rect = graphics::get_constant_buffer(sizeof(Vector4));
    ui_draw_private::buffer_color = graphics::get_constant_buffer(sizeof(Vector4));
    ui_draw_private::buffer_vertices = graphics::get_constant_buffer(sizeof(Vector4) * 3);
    ui_draw_private::buffer_vertices_line = graphics::get_constant_buffer(sizeof(Vector4) * 100);
    ui_draw_private::buffer_line_width = graphics::get_constant_buffer(sizeof(float));

    // Assert constant buffers are ready
    assert(graphics::is_ready(&ui_draw_private::buffer_model));
    assert(graphics::is_ready(&ui_draw_private::buffer_pv));
    assert(graphics::is_ready(&ui_draw_private::buffer_rect));
    assert(graphics::is_ready(&ui_draw_private::buffer_color));
    assert(graphics::is_ready(&ui_draw_private::buffer_vertices));
    assert(graphics::is_ready(&ui_draw_private::buffer_vertices_line));
    assert(graphics::is_ready(&ui_draw_private::buffer_line_width));

    // Create mesh
    ui_draw_private::quad_mesh = graphics::get_mesh(ui_draw_private::quad_vertices, 4, sizeof(float) * 6, ui_draw_private::quad_indices, 6, 2);
    assert(graphics::is_ready(&ui_draw_private::quad_mesh));
    ui_draw_private::triangle_mesh = graphics::get_mesh(ui_draw_private::triangle_vertices, 3, sizeof(float) * 3, NULL, 0, 0);
    assert(graphics::is_ready(&ui_draw_private::triangle_mesh));
    ui_draw_private::line_mesh = graphics::get_mesh(ui_draw_private::line_vertices, 6, sizeof(float) * 2, NULL, 0, 0);
    assert(graphics::is_ready(&ui_draw_private::line_mesh));
    ui_draw_private::miter_mesh = graphics::get_mesh(ui_draw_private::miter_vertices, 6, sizeof(float) * 4, NULL, 0, 0);
    assert(graphics::is_ready(&ui_draw_private::miter_mesh));

    // Create shaders
    ui_draw_private::vertex_shader_font = graphics::get_vertex_shader_from_code(ui_draw_private::vertex_shader_font_string, ARRAYSIZE(ui_draw_private::vertex_shader_font_string));
    ui_draw_private::pixel_shader_font = graphics::get_pixel_shader_from_code(ui_draw_private::pixel_shader_font_string, ARRAYSIZE(ui_draw_private::pixel_shader_font_string));
    ui_draw_private::vertex_shader_rect = graphics::get_vertex_shader_from_code(ui_draw_private::vertex_shader_rect_string, ARRAYSIZE(ui_draw_private::vertex_shader_rect_string));
    ui_draw_private::pixel_shader_rect = graphics::get_pixel_shader_from_code(ui_draw_private::pixel_shader_rect_string, ARRAYSIZE(ui_draw_private::pixel_shader_rect_string));
    ui_draw_private::vertex_shader_triangle = graphics::get_vertex_shader_from_code(ui_draw_private::vertex_shader_triangle_string, ARRAYSIZE(ui_draw_private::vertex_shader_triangle_string));
    ui_draw_private::pixel_shader_triangle = graphics::get_pixel_shader_from_code(ui_draw_private::pixel_shader_triangle_string, ARRAYSIZE(ui_draw_private::pixel_shader_triangle_string));
    ui_draw_private::vertex_shader_line = graphics::get_vertex_shader_from_code(ui_draw_private::vertex_shader_line_string, ARRAYSIZE(ui_draw_private::vertex_shader_line_string));
    ui_draw_private::vertex_shader_miter = graphics::get_vertex_shader_from_code(ui_draw_private::vertex_shader_miter_string, ARRAYSIZE(ui_draw_private::vertex_shader_miter_string));
    ui_draw_private::pixel_shader_line = graphics::get_pixel_shader_from_code(ui_draw_private::pixel_shader_line_string, ARRAYSIZE(ui_draw_private::pixel_shader_line_string));
    assert(graphics::is_ready(&ui_draw_private::vertex_shader_rect));
    assert(graphics::is_ready(&ui_draw_private::vertex_shader_font));
    assert(graphics::is_ready(&ui_draw_private::pixel_shader_font));
    assert(graphics::is_ready(&ui_draw_private::pixel_shader_rect));
    assert(graphics::is_ready(&ui_draw_private::vertex_shader_triangle));
    assert(graphics::is_ready(&ui_draw_private::pixel_shader_triangle));
    assert(graphics::is_ready(&ui_draw_private::vertex_shader_line));
    assert(graphics::is_ready(&ui_draw_private::vertex_shader_miter));
    assert(graphics::is_ready(&ui_draw_private::pixel_shader_line));

    // Create texture sampler
    ui_draw_private::texture_sampler = graphics::get_texture_sampler(SampleMode::CLAMP);
    assert(graphics::is_ready(&ui_draw_private::texture_sampler));

    // Init font
    ui_draw_private::font_file = file_system::read_file("consola.ttf");
    assert(ui_draw_private::font_file.data);
    ui_draw_private::font_ui = font::get((uint8_t *)ui_draw_private::font_file.data, ui_draw_private::font_file.size, ui_draw_private::FONT_HEIGHT, (uint32_t)ui_draw_private::FONT_TEXTURE_SIZE);

    // Initialize D3D texture for the Font
    ui_draw_private::font_ui_texture = graphics::get_texture2D(ui_draw_private::font_ui.bitmap, ui_draw_private::font_ui.bitmap_width, ui_draw_private::font_ui.bitmap_height, DXGI_FORMAT_R8_UNORM, 1);
    assert(graphics::is_ready(&ui_draw_private::font_ui_texture));
}


void ui_draw::draw_text(char *text, float x, float y, Vector4 color, Vector2 origin) {
    //ui_draw_private::ASSERT_SCREEN_SIZE;

    // Set font shaders
    graphics::set_pixel_shader(&ui_draw_private::pixel_shader_font);
    graphics::set_vertex_shader(&ui_draw_private::vertex_shader_font);

    // Set font texture
    graphics::set_texture(&ui_draw_private::font_ui_texture, 0);
    graphics::set_texture_sampler(&ui_draw_private::texture_sampler, 0);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant buffers
    graphics::set_constant_buffer(&ui_draw_private::buffer_pv, 0);
    graphics::set_constant_buffer(&ui_draw_private::buffer_model, 1);
    graphics::set_constant_buffer(&ui_draw_private::buffer_rect, 6);
    graphics::set_constant_buffer(&ui_draw_private::buffer_color, 7);

    // Update constant buffer values
    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, ui_draw_private::screen_width, 0, ui_draw_private::screen_height, -1, 1),
        math::get_identity()
    };
    graphics::update_constant_buffer(&ui_draw_private::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&ui_draw_private::buffer_color, &color);

    // Get final text dimensions
    float text_width = font::get_string_width(text, &ui_draw_private::font_ui);
    float text_height = font::get_row_height(&ui_draw_private::font_ui);

    // Adjust starting point based on the origin
    x = math::floor(x - origin.x * text_width);
    y = math::floor(y - origin.y * text_height);

    y += ui_draw_private::font_ui.top_pad;
    while(*text) {
        char c = *text;
        Glyph glyph = ui_draw_private::font_ui.glyphs[c - 32];

        // Set up source rectangle
        float rel_x = glyph.bitmap_x / ui_draw_private::FONT_TEXTURE_SIZE;
        float rel_y = glyph.bitmap_y / ui_draw_private::FONT_TEXTURE_SIZE;
        float rel_width = glyph.bitmap_width / ui_draw_private::FONT_TEXTURE_SIZE;
        float rel_height = glyph.bitmap_height / ui_draw_private::FONT_TEXTURE_SIZE;
        Vector4 source_rect = {rel_x, rel_y, rel_width, rel_height};
        graphics::update_constant_buffer(&ui_draw_private::buffer_rect, &source_rect);

        // Get final letter start
        float final_x = x + glyph.x_offset;
        float final_y = y + glyph.y_offset;

        // Set up model matrix
        // TODO: solve y axis downwards in an elegant way
        Matrix4x4 model_matrix = math::get_translation(final_x, ui_draw_private::screen_height - final_y, 0) * math::get_scale((float)glyph.bitmap_width, (float)glyph.bitmap_height, 1.0f) * math::get_translation(Vector3(0.5f, -0.5f, 0.0f)) * math::get_scale(0.5f);
        graphics::update_constant_buffer(&ui_draw_private::buffer_model, &model_matrix);

        graphics::draw_mesh(&ui_draw_private::quad_mesh);

        // Update current position for next letter
        if (*(text + 1)) x += font::get_kerning(&ui_draw_private::font_ui, c, *(text + 1));
        x += glyph.advance;
        text++;
    }

    // Reset previous blend state
    graphics::set_blend_state(old_blend_state);
};

void ui_draw::draw_text(char *text, Vector2 pos, Vector4 color, Vector2 origin) {
    ui_draw::draw_text(text, pos.x, pos.y, color, origin);
}

void ui_draw::draw_rect(float x, float y, float width, float height, Vector4 color) {
    //ASSERT_SCREEN_SIZE;

    // Set rect shaders
    graphics::set_pixel_shader(&ui_draw_private::pixel_shader_rect);
    graphics::set_vertex_shader(&ui_draw_private::vertex_shader_rect);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&ui_draw_private::buffer_pv, 0);
    graphics::set_constant_buffer(&ui_draw_private::buffer_model, 1);
    graphics::set_constant_buffer(&ui_draw_private::buffer_color, 7);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, ui_draw_private::screen_width, 0, ui_draw_private::screen_height, -1, 1),
        math::get_identity()
    };
    Matrix4x4 model_matrix = math::get_translation(x, ui_draw_private::screen_height - y, 0) * math::get_scale(width, height, 1.0f) * math::get_translation(Vector3(0.5f, -0.5f, 0.0f)) * math::get_scale(0.5f);

    // Update constant buffers
    graphics::update_constant_buffer(&ui_draw_private::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&ui_draw_private::buffer_color, &color);
    graphics::update_constant_buffer(&ui_draw_private::buffer_model, &model_matrix);

    // Render rect
    graphics::draw_mesh(&ui_draw_private::quad_mesh);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}

void ui_draw::draw_rect(Vector2 pos, float width, float height, Vector4 color) {
    ui_draw::draw_rect(pos.x, pos.y, width, height, color);
}

void ui_draw::draw_triangle(Vector2 v1, Vector2 v2, Vector2 v3, Vector4 color) {
    // Set rect shaders
    graphics::set_pixel_shader(&ui_draw_private::pixel_shader_triangle);
    graphics::set_vertex_shader(&ui_draw_private::vertex_shader_triangle);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&ui_draw_private::buffer_pv, 0);
    graphics::set_constant_buffer(&ui_draw_private::buffer_vertices, 1);
    graphics::set_constant_buffer(&ui_draw_private::buffer_color, 7);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, ui_draw_private::screen_width, 0, ui_draw_private::screen_height, -1, 1),
        math::get_identity()
    };
    v1.y = ui_draw_private::screen_height - v1.y;
    v2.y = ui_draw_private::screen_height - v2.y;
    v3.y = ui_draw_private::screen_height - v3.y;
    Vector4 vertices[3] = {Vector4(v1, 0, 0), Vector4(v2, 0, 0), Vector4(v3, 0, 0)};

    // Update constant buffers
    graphics::update_constant_buffer(&ui_draw_private::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&ui_draw_private::buffer_color, &color);
    graphics::update_constant_buffer(&ui_draw_private::buffer_vertices, &vertices);

    // Render rect
    graphics::draw_mesh(&ui_draw_private::triangle_mesh);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}


// NOTE: Memory allocation inside
void ui_draw::draw_line(Vector2 *points, int point_count, float width, Vector4 color) {
    //ASSERT_SCREEN_SIZE;

    // Set rect shaders
    graphics::set_pixel_shader(&ui_draw_private::pixel_shader_line);
    graphics::set_vertex_shader(&ui_draw_private::vertex_shader_line);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&ui_draw_private::buffer_pv, 0);
    graphics::set_constant_buffer(&ui_draw_private::buffer_vertices_line, 1);
    graphics::set_constant_buffer(&ui_draw_private::buffer_line_width, 2);
    graphics::set_constant_buffer(&ui_draw_private::buffer_color, 7);

    // TODO: Switch to using temporary mem buffer instead of heap
    Vector4 *real_points = (Vector4 *)malloc(point_count * sizeof(Vector4));
    for(int i = 0; i < point_count; ++i) {
        real_points[i].x = points[i].x;
        real_points[i].y = ui_draw_private::screen_height - points[i].y;
    }
    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        math::get_orthographics_projection_dx_rh(0, ui_draw_private::screen_width, 0, ui_draw_private::screen_height, -1, 1),
        math::get_identity()
    };

    // Update constant buffers
    graphics::update_constant_buffer(&ui_draw_private::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&ui_draw_private::buffer_color, &color);
    graphics::update_constant_buffer(&ui_draw_private::buffer_vertices_line, real_points);
    graphics::update_constant_buffer(&ui_draw_private::buffer_line_width, &width);

    // Render rect
    graphics_context->context->IASetVertexBuffers(0, 1, &ui_draw_private::line_mesh.vertex_buffer, &ui_draw_private::line_mesh.vertex_stride, &ui_draw_private::line_mesh.vertex_offset);
	graphics_context->context->IASetPrimitiveTopology(ui_draw_private::line_mesh.topology);
    graphics_context->context->DrawInstanced(ui_draw_private::line_mesh.vertex_count, point_count - 1, 0, 0);

    graphics::set_vertex_shader(&ui_draw_private::vertex_shader_miter);
    graphics_context->context->IASetVertexBuffers(0, 1, &ui_draw_private::miter_mesh.vertex_buffer, &ui_draw_private::miter_mesh.vertex_stride, &ui_draw_private::miter_mesh.vertex_offset);
	graphics_context->context->IASetPrimitiveTopology(ui_draw_private::miter_mesh.topology);
    graphics_context->context->DrawInstanced(ui_draw_private::miter_mesh.vertex_count, point_count - 2, 0, 0);

    free(real_points);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}

void ui_draw::release()
{
    graphics::release(&ui_draw_private::buffer_rect);
    graphics::release(&ui_draw_private::buffer_pv);
    graphics::release(&ui_draw_private::buffer_model);
    graphics::release(&ui_draw_private::buffer_color);

    graphics::release(&ui_draw_private::quad_mesh);
    graphics::release(&ui_draw_private::texture_sampler);

    graphics::release(&ui_draw_private::vertex_shader_font);
    graphics::release(&ui_draw_private::pixel_shader_font);
    graphics::release(&ui_draw_private::vertex_shader_rect);
    graphics::release(&ui_draw_private::pixel_shader_rect);

    font::release(&ui_draw_private::font_ui);
    file_system::release_file(ui_draw_private::font_file);
}