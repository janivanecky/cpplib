#include <cassert>
#include "file_system.h"
#include "graphics.h"
#include "ui_draw.h"

// Some macro hackery so we can expand other macros as strings.
#define XSTRINGIFY(x) #x
#define STRINGIFY(x) XSTRINGIFY(x)

/*

This section defines CPU and GPU-side constants.

*/

// Constant buffer indices (this is used to sync CPU and GPU side).
#define PV_MATRICES_BUFFER_INDEX 0
#define MODEL_MATRICES_BUFFER_INDEX 1
#define SOURCE_RECT_BUFFER_INDEX 2
#define COLOR_BUFFER_INDEX 3
#define SHADING_BUFFER_INDEX 4
#define TRIANGLE_VERTICES_BUFFER_INDEX 1
#define LINE_VERTICES_BUFFER_INDEX 1
#define LINE_SETTINGS_BUFFER_INDEX 2
#define ARC_SETTINGS_BUFFER_INDEX 2

// Max number of line points we can draw at once.
#define LINE_POINTS_TO_DRAW_BATCH_SIZE 4096

// Font parameters.
const float FONT_TEXTURE_SIZE = 512.0f;
const int32_t FONT_HEIGHT = 20;

/*

This section defines shaders.

*/

// Note that we use _ui_draw namespace as a "private" namespace to prevent name collisions.
namespace _ui_draw {

char vertex_shader_font_string[] = R"(
struct VertexInput {
    float4 position: POSITION;
    float2 texcoord: TEXCOORD;
};

struct VertexOutput {
    float4 svPosition: SV_POSITION;
    float2 texcoord: TEXCOORD;
};

cbuffer PVMatrices : register(b)" STRINGIFY(PV_MATRICES_BUFFER_INDEX) R"() {
    matrix projection;
    matrix view;
};

cbuffer ModelMatrix : register(b)" STRINGIFY(MODEL_MATRICES_BUFFER_INDEX) R"() {
    matrix model;
};

cbuffer SourceRectBuffer: register(b)" STRINGIFY(SOURCE_RECT_BUFFER_INDEX) R"() {
    float4 source_rect;
}

VertexOutput main(VertexInput input) {
    VertexOutput result;

    result.svPosition = mul(projection, mul(view, mul(model, input.position)));
    result.texcoord = input.texcoord * source_rect.zw + source_rect.xy;

    return result;
}
)";

char pixel_shader_font_string[] = R"(
struct PixelInput {
	float4 svPosition: SV_POSITION;
	float2 texcoord: TEXCOORD;
};

SamplerState texSampler: register(s0);
Texture2D tex: register(t0);

cbuffer ColorBuffer: register(b)" STRINGIFY(COLOR_BUFFER_INDEX) R"() {
	float4 color;
}

float4 main(PixelInput input) : SV_TARGET {
	const float smoothing = 1.0f / 16.0f;
	float alpha = tex.Sample(texSampler, input.texcoord).r;
    alpha = 1.0f - smoothstep(0.5f - smoothing, 0.5f + smoothing, alpha);
    return float4(color.xyz, color.w * alpha);
}
)";

char vertex_shader_rect_string[] = R"(
struct VertexInput {
	float4 position: POSITION;
};

struct VertexOutput {
	float4 svPosition: SV_POSITION;
    float4 screenPos: SCREEN_POS;
};

cbuffer PVMatrices : register(b)" STRINGIFY(PV_MATRICES_BUFFER_INDEX) R"() {
	matrix projection;
	matrix view;
};

cbuffer ModelMatrix : register(b)" STRINGIFY(MODEL_MATRICES_BUFFER_INDEX) R"() {
	matrix model;
};

VertexOutput main(VertexInput input) {
	VertexOutput result;

    float4 screenPos = mul(model, input.position);
    float4 leftTop = mul(model, float4(-1, 1, 0, 1));
    result.svPosition = mul(projection, mul(view, screenPos));
    result.screenPos = screenPos - leftTop;

	return result;
}
)";

char vertex_shader_arc_string[] = R"(
struct VertexInput {
	float4 position: POSITION;
};

struct VertexOutput {
	float4 svPosition: SV_POSITION;
};

cbuffer PVMatrices : register(b)" STRINGIFY(PV_MATRICES_BUFFER_INDEX) R"() {
	matrix projection;
	matrix view;
};

cbuffer ArcSettings : register(b)" STRINGIFY(ARC_SETTINGS_BUFFER_INDEX) R"() {
	float2 pos;
    float min_radius;
    float max_radius;
    float min_radian;
    float max_radian;
};

VertexOutput main(VertexInput input) {
	VertexOutput result;

    float r = min_radius + (max_radius - min_radius) * input.position.y;
    float a = min_radian + (max_radian - min_radian) * input.position.x;

    float2 p = float2(sin(a) * r, cos(a) * r);

    float4 vertex_position = float4(p + pos, 0.0f, 1.0f);

    result.svPosition = mul(projection, mul(view, vertex_position));

    return result;
}
)";

char vertex_shader_triangle_string[] = R"(
struct VertexInput {
	float3 position: POSITION;
};

struct VertexOutput {
	float4 svPosition: SV_POSITION;
    float4 screenPos: SCREEN_POS;
};

cbuffer PVMatrices : register(b)" STRINGIFY(PV_MATRICES_BUFFER_INDEX) R"() {
	matrix projection;
	matrix view;
};

cbuffer VerticesBuffer : register(b)" STRINGIFY(TRIANGLE_VERTICES_BUFFER_INDEX) R"() {
	float2x3 vertices;
};

VertexOutput main(VertexInput input) {
	VertexOutput result;

    float2 screen_pos = mul(vertices, input.position.xyz);
    float4 pos = float4(screen_pos, 0.0f, 1.0f);
	result.svPosition = mul(projection, mul(view, pos));
    result.screenPos = pos;

	return result;
}
)";

char vertex_shader_line_string[] = R"(
struct VertexInput {
    float2 position: POSITION;
    uint instance_id: SV_InstanceID;
};

struct VertexOutput {
    float4 svPosition: SV_POSITION;
    float4 screenPos: SCREEN_POS;
};

cbuffer PVMatrices : register(b)" STRINGIFY(PV_MATRICES_BUFFER_INDEX) R"() {
    matrix projection;
    matrix view;
};

StructuredBuffer<float4> vertices: register(t)" STRINGIFY(LINE_VERTICES_BUFFER_INDEX) R"();

cbuffer LineSettings : register(b)" STRINGIFY(LINE_SETTINGS_BUFFER_INDEX) R"() {
    float width;
};

VertexOutput main(VertexInput input) {
    VertexOutput result;

    float2 v1 = vertices[input.instance_id].xy;
    float2 v2 = vertices[input.instance_id + 1].xy;

    float2 length_axis = normalize(v2 - v1);
    float2 width_axis = float2(-length_axis.y, length_axis.x);

    float2 screen_pos = v1 + (v2 - v1) * input.position.x + width_axis * input.position.y * width * 0.5f;
    float4 pos = float4(screen_pos, 0.0f, 1.0f);
    result.svPosition = mul(projection, mul(view, pos));
    result.screenPos = float4(0,0,0,0);

    return result;
}
)";

char vertex_shader_miter_string[] = R"(
struct VertexInput {
    float4 position: POSITION;
    uint instance_id: SV_InstanceID;
};

struct VertexOutput {
    float4 svPosition: SV_POSITION;
    float4 screenPos: SCREEN_POS;
};

cbuffer PVMatrices : register(b)" STRINGIFY(PV_MATRICES_BUFFER_INDEX) R"() {
    matrix projection;
    matrix view;
};

StructuredBuffer<float4> vertices: register(t)" STRINGIFY(LINE_VERTICES_BUFFER_INDEX) R"();

cbuffer LineSettings : register(b)" STRINGIFY(LINE_SETTINGS_BUFFER_INDEX) R"() {
    float width;
};

VertexOutput main(VertexInput input) {
    VertexOutput result;

    float2 v1 = vertices[input.instance_id].xy;
    float2 v2 = vertices[input.instance_id + 1].xy;
    float2 v3 = vertices[input.instance_id + 2].xy;

    float2 length_axis_1 = normalize(v2 - v1);
    float2 length_axis_2 = normalize(v2 - v3);

    float2 width_axis_1 = float2(-length_axis_1.y, length_axis_1.x);
    float2 width_axis_2 = float2(-length_axis_2.y, length_axis_2.x);

    float2 tangent = normalize(normalize(v2 - v1) + normalize(v3 - v2));
    float2 miter = float2(-tangent.y, tangent.x);
    float s = sign(dot(v3 - v2, miter));

    float2 p1 = v2;
    float2 p2 = p1 - width_axis_1 * width * 0.5f * s;
    float2 p3 = p1 + width_axis_2 * width * 0.5f * s;
    float2 p4 = p1 + miter * length(p2 - p1) / dot(normalize(p2 - p1), miter);

    float2 screen_pos = mul(float2x4(p1.x, p2.x, p3.x, p4.x, p1.y, p2.y, p3.y, p4.y), input.position);
    float4 pos = float4(screen_pos, 0.0f, 1.0f);
    result.svPosition = mul(projection, mul(view, pos));
    result.screenPos = float4(0,0,0,0);

    return result;
}
)";

char pixel_shader_solid_color_string[] = R"(
struct PixelInput {
    float4 svPosition: SV_POSITION;
    float4 screenPos: SCREEN_POS;
};

cbuffer ColorBuffer: register(b)" STRINGIFY(COLOR_BUFFER_INDEX) R"() {
    float4 color;
}

cbuffer ColorBuffer: register(b)" STRINGIFY(SHADING_BUFFER_INDEX) R"() {
    uint shading;
}

float4 main(PixelInput input) : SV_TARGET {
    if(shading > 0) {
        // Line shading.
        float t = input.screenPos.x - input.screenPos.y;
        float a = sin(t * 3.1415 * 2.0f  / 7.0f);
        a = smoothstep(0, 0.01f, a);
        return color * a;
    }
    return color;
}
)";

}

/*

This section defines vertices/indices for meshes used to render UI.

*/

// Note that we use _ui_draw namespace as a "private" namespace to prevent name collisions.
namespace _ui_draw {

// Plain quad.
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

// These are essentially indices into a matrix of actual triangle points.
float triangle_vertices[] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
};

// x-coordinate is position along the line.
// y-coordinate is position perpendicular to the line.
float line_vertices[] = {
    0.0f, -1.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,

    0.0f, -1.0f,
    1.0f, -1.0f,
    1.0f, 1.0f,
};

// Same as for triangle_vertices, these are indices into a matrix of
// points which create miter joint.
float miter_vertices[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 0.0f,

    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};
}

/*

This section declares global state and graphics objects used for ui rendering.

*/

// Note that we use _ui_draw namespace as a "private" namespace to prevent name collisions.
namespace _ui_draw {

// Constant buffers.
ConstantBuffer buffer_rect;
ConstantBuffer buffer_pv;
ConstantBuffer buffer_model;
ConstantBuffer buffer_color;
ConstantBuffer buffer_shading;
ConstantBuffer buffer_vertices;
StructuredBuffer buffer_vertices_line;
ConstantBuffer buffer_line_width;
ConstantBuffer buffer_arc;

// Vertex shaders.
VertexShader vertex_shader_font;
VertexShader vertex_shader_rect;
VertexShader vertex_shader_triangle;
VertexShader vertex_shader_line;
VertexShader vertex_shader_miter;
VertexShader vertex_shader_arc;

// Pixel shaders.
PixelShader pixel_shader_font;
PixelShader pixel_shader_solid_color;

// Texture samplers.
TextureSampler texture_sampler;

// Meshes.
Mesh quad_mesh;
Mesh triangle_mesh;
Mesh line_mesh;
Mesh miter_mesh;
Mesh circle_mesh;
Mesh arc_mesh;

// Font + font texture.
Font font_ui;
Texture2D font_ui_texture;

// We need the screen size for rendering. These values are set in ui::init
float screen_width = -1, screen_height = -1;

}

// Public API to get the font.
Font *ui_draw::get_font() {
    return &_ui_draw::font_ui;
}

/*

This section defines init/release functions for set up/tear down/update of the global state.

*/

void ui_draw::init(float screen_width_ui, float screen_height_ui) {
    // Set screen size
    _ui_draw::screen_width = screen_width_ui;
    _ui_draw::screen_height = screen_height_ui;

    // Create constant buffers
    _ui_draw::buffer_model = graphics::get_constant_buffer(sizeof(Matrix4x4));
    _ui_draw::buffer_pv = graphics::get_constant_buffer(sizeof(Matrix4x4) * 2);
    _ui_draw::buffer_rect = graphics::get_constant_buffer(sizeof(Vector4));
    _ui_draw::buffer_color = graphics::get_constant_buffer(sizeof(Vector4));
    _ui_draw::buffer_shading = graphics::get_constant_buffer(sizeof(Vector4));
    _ui_draw::buffer_vertices = graphics::get_constant_buffer(sizeof(Vector4) * 3);
    _ui_draw::buffer_line_width = graphics::get_constant_buffer(sizeof(float));
    _ui_draw::buffer_arc = graphics::get_constant_buffer(sizeof(float) * 6);
    _ui_draw::buffer_vertices_line = graphics::get_structured_buffer(sizeof(Vector4), LINE_POINTS_TO_DRAW_BATCH_SIZE);
    assert(graphics::is_ready(&_ui_draw::buffer_model));
    assert(graphics::is_ready(&_ui_draw::buffer_pv));
    assert(graphics::is_ready(&_ui_draw::buffer_rect));
    assert(graphics::is_ready(&_ui_draw::buffer_color));
    assert(graphics::is_ready(&_ui_draw::buffer_vertices));
    assert(graphics::is_ready(&_ui_draw::buffer_vertices_line));
    assert(graphics::is_ready(&_ui_draw::buffer_line_width));
    assert(graphics::is_ready(&_ui_draw::buffer_arc));

    // Create mesh
    _ui_draw::quad_mesh = graphics::get_mesh(
        _ui_draw::quad_vertices, 4, sizeof(float) * 6, _ui_draw::quad_indices, 6, 2
    );
    _ui_draw::triangle_mesh = graphics::get_mesh(
        _ui_draw::triangle_vertices, 3, sizeof(float) * 3, NULL, 0, 0
    );
    _ui_draw::line_mesh = graphics::get_mesh(
        _ui_draw::line_vertices, 6, sizeof(float) * 2, NULL, 0, 0
    );
    _ui_draw::miter_mesh = graphics::get_mesh(
        _ui_draw::miter_vertices, 6, sizeof(float) * 4, NULL, 0, 0
    );
    assert(graphics::is_ready(&_ui_draw::triangle_mesh));
    assert(graphics::is_ready(&_ui_draw::quad_mesh));
    assert(graphics::is_ready(&_ui_draw::line_mesh));
    assert(graphics::is_ready(&_ui_draw::miter_mesh));

    // Create shaders
    _ui_draw::vertex_shader_font = graphics::get_vertex_shader_from_code(
        _ui_draw::vertex_shader_font_string, ARRAYSIZE(_ui_draw::vertex_shader_font_string));
    _ui_draw::pixel_shader_font = graphics::get_pixel_shader_from_code(
        _ui_draw::pixel_shader_font_string, ARRAYSIZE(_ui_draw::pixel_shader_font_string));
    _ui_draw::vertex_shader_rect = graphics::get_vertex_shader_from_code(
        _ui_draw::vertex_shader_rect_string, ARRAYSIZE(_ui_draw::vertex_shader_rect_string));
    _ui_draw::vertex_shader_triangle = graphics::get_vertex_shader_from_code(
        _ui_draw::vertex_shader_triangle_string, ARRAYSIZE(_ui_draw::vertex_shader_triangle_string));
    _ui_draw::vertex_shader_line = graphics::get_vertex_shader_from_code(
        _ui_draw::vertex_shader_line_string, ARRAYSIZE(_ui_draw::vertex_shader_line_string));
    _ui_draw::vertex_shader_miter = graphics::get_vertex_shader_from_code(
        _ui_draw::vertex_shader_miter_string, ARRAYSIZE(_ui_draw::vertex_shader_miter_string));
    _ui_draw::pixel_shader_solid_color = graphics::get_pixel_shader_from_code(
        _ui_draw::pixel_shader_solid_color_string, ARRAYSIZE(_ui_draw::pixel_shader_solid_color_string));
    _ui_draw::vertex_shader_arc = graphics::get_vertex_shader_from_code(
        _ui_draw::vertex_shader_arc_string, ARRAYSIZE(_ui_draw::vertex_shader_arc_string));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_rect));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_font));
    assert(graphics::is_ready(&_ui_draw::pixel_shader_font));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_triangle));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_line));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_miter));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_arc));
    assert(graphics::is_ready(&_ui_draw::pixel_shader_solid_color));

    // Create texture sampler
    _ui_draw::texture_sampler = graphics::get_texture_sampler(SampleMode::CLAMP);
    assert(graphics::is_ready(&_ui_draw::texture_sampler));

    // Init font
    File font_file = file_system::read_file("consola.ttf");
    assert(font_file.data);
    _ui_draw::font_ui = font::get(
        (uint8_t *)font_file.data, font_file.size,
        FONT_HEIGHT, (uint32_t)FONT_TEXTURE_SIZE
    );
    file_system::release_file(font_file);

    // Initialize D3D texture for the Font
    _ui_draw::font_ui_texture = graphics::get_texture2D(
        _ui_draw::font_ui.bitmap,
        _ui_draw::font_ui.bitmap_width, _ui_draw::font_ui.bitmap_height,
        DXGI_FORMAT_R8_UNORM, 1
    );
    assert(graphics::is_ready(&_ui_draw::font_ui_texture));

    // Initialize circle mesh.
    const int CIRCLE_PARTS_COUNT = 64;
    Vector4 *circle_vertices = (Vector4 *)malloc(sizeof(Vector4) * 3 * CIRCLE_PARTS_COUNT);
    for (int i = 0; i < CIRCLE_PARTS_COUNT; ++i) {
        float a1 = math::PI2 / float(CIRCLE_PARTS_COUNT) * i;
        float a2 = math::PI2 / float(CIRCLE_PARTS_COUNT) * (i + 1);

        circle_vertices[i * 3] = Vector4(
            0.0f, 0.0f, 0.0f, 1.0f
        );
        circle_vertices[i * 3 + 1] = Vector4(
            math::sin(a1), math::cos(a1), 0.0f, 1.0f
        );
        circle_vertices[i * 3 + 2] = Vector4(
            math::sin(a2), math::cos(a2), 0.0f, 1.0f
        );
    }
    _ui_draw::circle_mesh = graphics::get_mesh(circle_vertices, CIRCLE_PARTS_COUNT * 3, sizeof(Vector4), NULL, 0, 0);
    free(circle_vertices);

    // Initialize arc mesh.
    const int ARC_PARTS_COUNT = 128;
    Vector4 *arc_vertices = (Vector4 *)malloc(sizeof(Vector4) * 6 * ARC_PARTS_COUNT);
    for (int i = 0; i < ARC_PARTS_COUNT; ++i) {
        float a1 = 1.0f / float(ARC_PARTS_COUNT) * i;
        float a2 = 1.0f / float(ARC_PARTS_COUNT) * (i + 1);

        arc_vertices[i * 6] = Vector4(a1, 0.0f, 0.0f, 1.0f);
        arc_vertices[i * 6 + 1] = Vector4(a2, 1.0f, 0.0f, 1.0f);
        arc_vertices[i * 6 + 2] = Vector4(a1, 1.0f, 0.0f, 1.0f);
        arc_vertices[i * 6 + 3] = Vector4(a1, 0.0f, 0.0f, 1.0f);
        arc_vertices[i * 6 + 4] = Vector4(a2, 0.0f, 0.0f, 1.0f);
        arc_vertices[i * 6 + 5] = Vector4(a2, 1.0f, 0.0f, 1.0f);
    }
    _ui_draw::arc_mesh = graphics::get_mesh(arc_vertices, ARC_PARTS_COUNT * 6, sizeof(Vector4), NULL, 0, 0);
    free(arc_vertices);
}


void ui_draw::release() {
    graphics::release(&_ui_draw::buffer_rect);
    graphics::release(&_ui_draw::buffer_pv);
    graphics::release(&_ui_draw::buffer_model);
    graphics::release(&_ui_draw::buffer_color);
    graphics::release(&_ui_draw::buffer_shading);
    graphics::release(&_ui_draw::buffer_vertices);
    graphics::release(&_ui_draw::buffer_vertices_line);
    graphics::release(&_ui_draw::buffer_line_width);

    graphics::release(&_ui_draw::vertex_shader_font);
    graphics::release(&_ui_draw::pixel_shader_font);
    graphics::release(&_ui_draw::vertex_shader_rect);
    graphics::release(&_ui_draw::vertex_shader_triangle);
    graphics::release(&_ui_draw::vertex_shader_line);
    graphics::release(&_ui_draw::vertex_shader_miter);
    graphics::release(&_ui_draw::pixel_shader_solid_color);

    graphics::release(&_ui_draw::quad_mesh);
    graphics::release(&_ui_draw::triangle_mesh);
    graphics::release(&_ui_draw::line_mesh);
    graphics::release(&_ui_draw::miter_mesh);

    graphics::release(&_ui_draw::texture_sampler);

    font::release(&_ui_draw::font_ui);
    graphics::release(&_ui_draw::font_ui_texture);
}


void ui_draw::set_screen_size(float screen_width, float screen_height) {
    _ui_draw::screen_width = screen_width;
    _ui_draw::screen_height = screen_height;
}

/*

This section defines the core ui_draw functionality - functions for rendering UI.

*/

// Convenience function to set up ortographic projection matrix.
Matrix4x4 get_projection_matrix() {
    return math::get_orthographics_projection_dx_rh(
        0, _ui_draw::screen_width, 0, _ui_draw::screen_height, -1, 1
    );
}

void ui_draw::draw_text(char *text, float x, float y, Vector4 color, Vector2 origin) {
    // Set font shaders
    graphics::set_pixel_shader(&_ui_draw::pixel_shader_font);
    graphics::set_vertex_shader(&_ui_draw::vertex_shader_font);

    // Set font texture
    graphics::set_texture(&_ui_draw::font_ui_texture, 0);
    graphics::set_texture_sampler(&_ui_draw::texture_sampler, 0);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant buffers
    graphics::set_constant_buffer(&_ui_draw::buffer_pv, PV_MATRICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_model, MODEL_MATRICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_rect, SOURCE_RECT_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_color, COLOR_BUFFER_INDEX);

    // Update constant buffer values
    Matrix4x4 pv_matrices[2] = {
        get_projection_matrix(),
        math::get_identity()
    };
    graphics::update_constant_buffer(&_ui_draw::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&_ui_draw::buffer_color, &color);

    // Get final text dimensions
    float text_width = font::get_string_width(text, &_ui_draw::font_ui);
    float text_height = font::get_row_height(&_ui_draw::font_ui);

    // Adjust starting point based on the origin
    x = math::floor(x - origin.x * text_width);
    y = math::floor(y - origin.y * text_height);

    y += _ui_draw::font_ui.top_pad;
    while(*text) {
        char c = *text;
        Glyph glyph = _ui_draw::font_ui.glyphs[c - 32];

        // Set up source rectangle
        float rel_x = glyph.bitmap_x / FONT_TEXTURE_SIZE;
        float rel_y = glyph.bitmap_y / FONT_TEXTURE_SIZE;
        float rel_width = glyph.bitmap_width / FONT_TEXTURE_SIZE;
        float rel_height = glyph.bitmap_height / FONT_TEXTURE_SIZE;
        Vector4 source_rect = {rel_x, rel_y, rel_width, rel_height};
        graphics::update_constant_buffer(&_ui_draw::buffer_rect, &source_rect);

        // Get final letter start
        float final_x = x + glyph.x_offset;
        float final_y = y + glyph.y_offset;

        // Set up model matrix
        // TODO: solve y axis downwards in an elegant way
        Matrix4x4 model_matrix =
            math::get_translation(final_x, _ui_draw::screen_height - final_y, 0) * 
            math::get_scale((float)glyph.bitmap_width, (float)glyph.bitmap_height, 1.0f) *
            math::get_translation(Vector3(0.5f, -0.5f, 0.0f)) *
            math::get_scale(0.5f);
        graphics::update_constant_buffer(&_ui_draw::buffer_model, &model_matrix);

        graphics::draw_mesh(&_ui_draw::quad_mesh);

        // Update current position for next letter
        if (*(text + 1)) x += font::get_kerning(&_ui_draw::font_ui, c, *(text + 1));
        x += glyph.advance;
        text++;
    }

    // Reset previous blend state
    graphics::set_blend_state(old_blend_state);
};

void ui_draw::draw_text(char *text, Vector2 pos, Vector4 color, Vector2 origin) {
    ui_draw::draw_text(text, pos.x, pos.y, color, origin);
}

void ui_draw::draw_rect(float x, float y, float width, float height, Vector4 color, ShadingType shading_type) {
    // Set rect shaders
    graphics::set_pixel_shader(&_ui_draw::pixel_shader_solid_color);
    graphics::set_vertex_shader(&_ui_draw::vertex_shader_rect);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&_ui_draw::buffer_pv, PV_MATRICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_model, MODEL_MATRICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_color, COLOR_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_shading, SHADING_BUFFER_INDEX);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        get_projection_matrix(),
        math::get_identity()
    };
    Matrix4x4 model_matrix =
        math::get_translation(x, _ui_draw::screen_height - y, 0) *
        math::get_scale(width, height, 1.0f) *
        math::get_translation(Vector3(0.5f, -0.5f, 0.0f)) *
        math::get_scale(0.5f);

    // Update constant buffers
    graphics::update_constant_buffer(&_ui_draw::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&_ui_draw::buffer_color, &color);
    graphics::update_constant_buffer(&_ui_draw::buffer_shading, &shading_type);
    graphics::update_constant_buffer(&_ui_draw::buffer_model, &model_matrix);

    // Render rect
    graphics::draw_mesh(&_ui_draw::quad_mesh);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}

void ui_draw::draw_rect(Vector2 pos, float width, float height, Vector4 color, ShadingType shading_type) {
    ui_draw::draw_rect(pos.x, pos.y, width, height, color, shading_type);
}

void ui_draw::draw_circle(Vector2 pos, float radius, Vector4 color) {
    // Set circle shaders (same as for rectangle).
    graphics::set_pixel_shader(&_ui_draw::pixel_shader_solid_color);
    graphics::set_vertex_shader(&_ui_draw::vertex_shader_rect);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&_ui_draw::buffer_pv, PV_MATRICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_model, MODEL_MATRICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_color, COLOR_BUFFER_INDEX);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        get_projection_matrix(),
        math::get_identity()
    };
    Matrix4x4 model_matrix =
        math::get_translation(pos.x, _ui_draw::screen_height - pos.y, 0) *
        math::get_scale(radius, radius, 1.0f);

    // Update constant buffers
    graphics::update_constant_buffer(&_ui_draw::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&_ui_draw::buffer_color, &color);
    graphics::update_constant_buffer(&_ui_draw::buffer_model, &model_matrix);

    // Render rect
    graphics::draw_mesh(&_ui_draw::circle_mesh);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}

void ui_draw::draw_arc(
    Vector2 pos, float radius_min, float radius_max, float start_radian, float end_radian, Vector4 color
) {
    // Set arc shaders.
    graphics::set_pixel_shader(&_ui_draw::pixel_shader_solid_color);
    graphics::set_vertex_shader(&_ui_draw::vertex_shader_arc);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&_ui_draw::buffer_pv, PV_MATRICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_color, COLOR_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_arc, ARC_SETTINGS_BUFFER_INDEX);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        get_projection_matrix(),
        math::get_identity()
    };
    float arc_values[6] = {
        pos.x, _ui_draw::screen_height - pos.y,
        radius_min, radius_max,
        start_radian, end_radian
    };

    // Update constant buffers
    graphics::update_constant_buffer(&_ui_draw::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&_ui_draw::buffer_color, &color);
    graphics::update_constant_buffer(&_ui_draw::buffer_arc, &arc_values);

    // Render rect
    graphics::draw_mesh(&_ui_draw::arc_mesh);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}

void ui_draw::draw_triangle(Vector2 v1, Vector2 v2, Vector2 v3, Vector4 color) {
    // Set rect shaders
    graphics::set_pixel_shader(&_ui_draw::pixel_shader_solid_color);
    graphics::set_vertex_shader(&_ui_draw::vertex_shader_triangle);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&_ui_draw::buffer_pv, PV_MATRICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_vertices, TRIANGLE_VERTICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_color, COLOR_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_shading, SHADING_BUFFER_INDEX);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        get_projection_matrix(),
        math::get_identity()
    };
    v1.y = _ui_draw::screen_height - v1.y;
    v2.y = _ui_draw::screen_height - v2.y;
    v3.y = _ui_draw::screen_height - v3.y;
    Vector4 vertices[3] = {Vector4(v1, 0, 0), Vector4(v2, 0, 0), Vector4(v3, 0, 0)};
    ShadingType shading = SOLID_COLOR;

    // Update constant buffers
    graphics::update_constant_buffer(&_ui_draw::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&_ui_draw::buffer_color, &color);
    graphics::update_constant_buffer(&_ui_draw::buffer_shading, &shading);
    graphics::update_constant_buffer(&_ui_draw::buffer_vertices, &vertices);

    // Render rect
    graphics::draw_mesh(&_ui_draw::triangle_mesh);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}


void ui_draw::draw_line(Vector2 *points, int point_count, float width, Vector4 color) {
    // Set rect shaders
    graphics::set_pixel_shader(&_ui_draw::pixel_shader_solid_color);

    // Set alpha blending state
    BlendType old_blend_state = graphics::get_blend_state();
    graphics::set_blend_state(BlendType::ALPHA);

    // Set constant bufers
    graphics::set_constant_buffer(&_ui_draw::buffer_pv, PV_MATRICES_BUFFER_INDEX);
    graphics::set_texture(&_ui_draw::buffer_vertices_line, LINE_VERTICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_line_width, LINE_SETTINGS_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_color, COLOR_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_shading, SHADING_BUFFER_INDEX);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        get_projection_matrix(),
        math::get_identity()
    };
    ShadingType shading = SOLID_COLOR;

    // Update constant buffers
    graphics::update_constant_buffer(&_ui_draw::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&_ui_draw::buffer_color, &color);
    graphics::update_constant_buffer(&_ui_draw::buffer_line_width, &width);
    graphics::update_constant_buffer(&_ui_draw::buffer_shading, &shading);

    // Static buffer so we don't have to allocate memory for points when drawing lines.
    static Vector4 point_buffer[LINE_POINTS_TO_DRAW_BATCH_SIZE];

    // Render the line.
    int first_point = 0; // This is an index of the first point we render for a single batch.
    do {
        // -1 because to draw joints for N line points we need N + 1 points. This means that we need
        // N + 1 points and our batch size is LINE_POINTS_TO_DRAW_BATCH_SIZE, which means that
        // N = LINE_POINTS_TO_DRAW_BATCH_SIZE - 1.
        int last_point_lines = math::min(first_point + LINE_POINTS_TO_DRAW_BATCH_SIZE - 1, point_count);
        int no_points_lines = last_point_lines - first_point;
        int last_point_joints = math::min(first_point + LINE_POINTS_TO_DRAW_BATCH_SIZE, point_count);
        int no_points_joints = last_point_joints - first_point;

        // Set points buffer.
        for(int i = 0; i < no_points_joints; ++i) {
            point_buffer[i].x = points[i + first_point].x;
            point_buffer[i].y = _ui_draw::screen_height - points[i + first_point].y;
        }
        graphics::update_structured_buffer(&_ui_draw::buffer_vertices_line, point_buffer);

        // Draw line segments
        graphics::set_vertex_shader(&_ui_draw::vertex_shader_line);
        graphics::draw_mesh_instanced(&_ui_draw::line_mesh, no_points_lines - 1);
    
        // Draw miter joints
        graphics::set_vertex_shader(&_ui_draw::vertex_shader_miter);
        graphics::draw_mesh_instanced(&_ui_draw::miter_mesh, no_points_joints - 2);
    
        // Update the first point for the next batch.
        first_point += no_points_lines - 1;
    } while(first_point + 1 < point_count);

    // Unset structured buffer.
    graphics::unset_texture(LINE_VERTICES_BUFFER_INDEX);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}