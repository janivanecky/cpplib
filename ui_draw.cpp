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
#define TRIANGLE_VERTICES_BUFFER_INDEX 1
#define LINE_VERTICES_BUFFER_INDEX 1
#define LINE_SETTINGS_BUFFER_INDEX 2

// Max number of line points we can draw at once.
#define LINE_POINTS_TO_DRAW_BATCH_SIZE 4096

// Font parameters.
const float FONT_TEXTURE_SIZE = 256.0f;
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
	float alpha = tex.Sample(texSampler, input.texcoord).r;
	return float4(color.xyz, color.w * alpha);;
}
)";

char vertex_shader_rect_string[] = R"(
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

cbuffer ModelMatrix : register(b)" STRINGIFY(MODEL_MATRICES_BUFFER_INDEX) R"() {
	matrix model;
};

VertexOutput main(VertexInput input) {
	VertexOutput result;

	result.svPosition = mul(projection, mul(view, mul(model, input.position)));

	return result;
}
)";

char vertex_shader_triangle_string[] = R"(
struct VertexInput {
	float3 position: POSITION;
};

struct VertexOutput {
	float4 svPosition: SV_POSITION;
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
};

cbuffer PVMatrices : register(b)" STRINGIFY(PV_MATRICES_BUFFER_INDEX) R"() {
    matrix projection;
    matrix view;
};

cbuffer LineVertices : register(b)" STRINGIFY(LINE_VERTICES_BUFFER_INDEX) R"() {
    float4 vertices[)" STRINGIFY(LINE_POINTS_TO_DRAW_BATCH_SIZE) R"(];
};

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
};

cbuffer PVMatrices : register(b)" STRINGIFY(PV_MATRICES_BUFFER_INDEX) R"() {
    matrix projection;
    matrix view;
};

cbuffer LineVertices : register(b)" STRINGIFY(LINE_VERTICES_BUFFER_INDEX) R"() {
    float4 vertices[)" STRINGIFY(LINE_POINTS_TO_DRAW_BATCH_SIZE) R"(];
};

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

    return result;
}
)";

char pixel_shader_solid_color_string[] = R"(
struct PixelInput {
    float4 svPosition: SV_POSITION;
};

cbuffer ColorBuffer: register(b)" STRINGIFY(COLOR_BUFFER_INDEX) R"() {
    float4 color;
}

float4 main(PixelInput input) : SV_TARGET {
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
ConstantBuffer buffer_vertices;
ConstantBuffer buffer_vertices_line;
ConstantBuffer buffer_line_width;

// Vertex shaders.
VertexShader vertex_shader_font;
VertexShader vertex_shader_rect;
VertexShader vertex_shader_triangle;
VertexShader vertex_shader_line;
VertexShader vertex_shader_miter;

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

// Font + font texture.
Font font_ui;
Texture2D font_ui_texture;
// We have to keep the file pointer because we cannot release the file until we release the font.
File font_file;

// We need the screen size for rendering. These values are set in ui::init
float screen_width = -1, screen_height = -1;

}

// Public API to get the font.
Font *ui_draw::get_font() {
    return &_ui_draw::font_ui;
}

/*

This section defines init/release functions for set up/tear down of the global state.

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
    _ui_draw::buffer_vertices = graphics::get_constant_buffer(sizeof(Vector4) * 3);
    _ui_draw::buffer_vertices_line = graphics::get_constant_buffer(sizeof(Vector4) * LINE_POINTS_TO_DRAW_BATCH_SIZE);
    _ui_draw::buffer_line_width = graphics::get_constant_buffer(sizeof(float));
    assert(graphics::is_ready(&_ui_draw::buffer_model));
    assert(graphics::is_ready(&_ui_draw::buffer_pv));
    assert(graphics::is_ready(&_ui_draw::buffer_rect));
    assert(graphics::is_ready(&_ui_draw::buffer_color));
    assert(graphics::is_ready(&_ui_draw::buffer_vertices));
    assert(graphics::is_ready(&_ui_draw::buffer_vertices_line));
    assert(graphics::is_ready(&_ui_draw::buffer_line_width));

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
    assert(graphics::is_ready(&_ui_draw::vertex_shader_rect));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_font));
    assert(graphics::is_ready(&_ui_draw::pixel_shader_font));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_triangle));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_line));
    assert(graphics::is_ready(&_ui_draw::vertex_shader_miter));
    assert(graphics::is_ready(&_ui_draw::pixel_shader_solid_color));

    // Create texture sampler
    _ui_draw::texture_sampler = graphics::get_texture_sampler(SampleMode::CLAMP);
    assert(graphics::is_ready(&_ui_draw::texture_sampler));

    // Init font
    _ui_draw::font_file = file_system::read_file("consola.ttf");
    assert(_ui_draw::font_file.data);
    _ui_draw::font_ui = font::get(
        (uint8_t *)_ui_draw::font_file.data, _ui_draw::font_file.size,
        FONT_HEIGHT, (uint32_t)FONT_TEXTURE_SIZE
    );

    // Initialize D3D texture for the Font
    _ui_draw::font_ui_texture = graphics::get_texture2D(
        _ui_draw::font_ui.bitmap,
        _ui_draw::font_ui.bitmap_width, _ui_draw::font_ui.bitmap_height,
        DXGI_FORMAT_R8_UNORM, 1
    );
    assert(graphics::is_ready(&_ui_draw::font_ui_texture));
}



void ui_draw::release() {
    graphics::release(&_ui_draw::buffer_rect);
    graphics::release(&_ui_draw::buffer_pv);
    graphics::release(&_ui_draw::buffer_model);
    graphics::release(&_ui_draw::buffer_color);
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
    file_system::release_file(_ui_draw::font_file);
    graphics::release(&_ui_draw::font_ui_texture);
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

void ui_draw::draw_rect(float x, float y, float width, float height, Vector4 color) {
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
    graphics::update_constant_buffer(&_ui_draw::buffer_model, &model_matrix);

    // Render rect
    graphics::draw_mesh(&_ui_draw::quad_mesh);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}

void ui_draw::draw_rect(Vector2 pos, float width, float height, Vector4 color) {
    ui_draw::draw_rect(pos.x, pos.y, width, height, color);
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

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        get_projection_matrix(),
        math::get_identity()
    };
    v1.y = _ui_draw::screen_height - v1.y;
    v2.y = _ui_draw::screen_height - v2.y;
    v3.y = _ui_draw::screen_height - v3.y;
    Vector4 vertices[3] = {Vector4(v1, 0, 0), Vector4(v2, 0, 0), Vector4(v3, 0, 0)};

    // Update constant buffers
    graphics::update_constant_buffer(&_ui_draw::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&_ui_draw::buffer_color, &color);
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
    graphics::set_constant_buffer(&_ui_draw::buffer_vertices_line, LINE_VERTICES_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_line_width, LINE_SETTINGS_BUFFER_INDEX);
    graphics::set_constant_buffer(&_ui_draw::buffer_color, COLOR_BUFFER_INDEX);

    // Get constant buffer values
    Matrix4x4 pv_matrices[2] = {
        get_projection_matrix(),
        math::get_identity()
    };

    // Update constant buffers
    graphics::update_constant_buffer(&_ui_draw::buffer_pv, pv_matrices);
    graphics::update_constant_buffer(&_ui_draw::buffer_color, &color);
    graphics::update_constant_buffer(&_ui_draw::buffer_line_width, &width);

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
        graphics::update_constant_buffer(&_ui_draw::buffer_vertices_line, point_buffer);

        // Draw line segments
        graphics::set_vertex_shader(&_ui_draw::vertex_shader_line);
        graphics::draw_mesh_instanced(&_ui_draw::line_mesh, no_points_lines - 1);
    
        // Draw miter joints
        graphics::set_vertex_shader(&_ui_draw::vertex_shader_miter);
        graphics::draw_mesh_instanced(&_ui_draw::miter_mesh, no_points_joints - 2);
    
        // Update the first point for the next batch.
        first_point += no_points_lines - 1;
    } while(first_point + 1 < point_count);

    // Reset previous blending state
    graphics::set_blend_state(old_blend_state);
}