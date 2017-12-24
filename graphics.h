#pragma once
#include <d3dcompiler.h>
#include <D3D11.h>
#include <dxgi.h>

#include "platform.h"
#include "math.h"

struct GraphicsContext
{
	ID3D11Device *device;
	ID3D11DeviceContext *context;
};

extern GraphicsContext *graphics_context;

struct SwapChain
{
	IDXGISwapChain *swap_chain;
};

struct RenderTarget
{
	ID3D11RenderTargetView *rt_view;
	ID3D11ShaderResourceView *sr_view;
	ID3D11Texture2D *texture;
	uint32_t width;
	uint32_t height;
};

struct DepthBuffer
{
	ID3D11DepthStencilView *ds_view;
	ID3D11ShaderResourceView *sr_view;
	ID3D11Texture2D *texture;
	uint32_t width;
	uint32_t height;
};

struct Texture
{
	ID3D11Texture2D *texture;
	ID3D11ShaderResourceView *sr_view;
	uint32_t width;
	uint32_t height;
};

struct Mesh
{
	ID3D11Buffer *vertex_buffer;
	ID3D11Buffer *index_buffer;
	uint32_t vertex_stride;
	uint32_t vertex_offset;
	uint32_t index_count;
	DXGI_FORMAT index_format;
	D3D11_PRIMITIVE_TOPOLOGY topology;
};

struct VertexShader
{
	ID3D11VertexShader *vertex_shader;
	ID3D11InputLayout *input_layout;
};

struct GeometryShader
{
	ID3D11GeometryShader *geometry_shader;
};

struct PixelShader
{
	ID3D11PixelShader *pixel_shader;
};

struct ConstantBuffer
{
	ID3D11Buffer *buffer;
	uint32_t size;
};

const uint32_t MAX_SEMANTIC_NAME_LENGTH = 10;
struct VertexInputDesc
{
	char semantic_name[MAX_SEMANTIC_NAME_LENGTH];
	DXGI_FORMAT format;
};

struct CompiledShader
{
	ID3DBlob *blob;
};

enum SampleMode
{
	CLAMP = 0,
	WRAP,
	BORDER,
};

struct TextureSampler
{
	ID3D11SamplerState *sampler;
};

enum BlendType
{
	ALPHA_BLEND = 0,
	OPAQUE_BLEND
};

struct BlendState
{
	ID3D11BlendState *state;
};

struct Viewport
{
	float x;
	float y;
	float width;
	float height;
};

// TODO: move to another file with utils
const uint32_t VERTEX_SHADER_MAX_INPUT_COUNT = 5;

namespace graphics
{
	void init(LUID *adapter_luid = NULL);
	void init_swap_chain(Window *window);

	RenderTarget get_render_target_window();
	RenderTarget get_render_target(uint32_t width, uint32_t height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
	void clear_render_target(RenderTarget *buffer, float r, float g, float b, float a);

	DepthBuffer get_depth_buffer(uint32_t width, uint32_t height);
	void clear_depth_buffer(DepthBuffer *buffer);

	void set_render_targets(DepthBuffer *buffer);
	void set_render_targets(RenderTarget *buffer);
	void set_render_targets(RenderTarget *buffer, DepthBuffer *depth_buffer);

	void set_viewport(RenderTarget *buffer);
	void set_viewport(DepthBuffer *buffer);
	void set_viewport(Viewport *viewport);

	void set_render_targets_viewport(RenderTarget *buffer);
    void set_render_targets_viewport(RenderTarget *buffer, DepthBuffer *depth_buffer);
	void set_render_targets_viewport(RenderTarget *buffers, uint32_t buffer_count, DepthBuffer *depth_buffer);

	Texture get_texture(void *data, uint32_t width, uint32_t height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, uint32_t pixel_byte_count = 4);

	void set_texture(RenderTarget *buffer, uint32_t slot);
	void set_texture(DepthBuffer *buffer, uint32_t slot);
	void set_texture(Texture *texture, uint32_t slot);
	void unset_texture(uint32_t slot);

	BlendState get_blend_state(BlendType type = OPAQUE_BLEND);
	void set_blend_state(BlendState *blend_state);

	TextureSampler get_texture_sampler(SampleMode mode = CLAMP);
	void set_texture_sampler(TextureSampler *sampler, uint32_t slot);

	Mesh get_mesh(void *vertices, uint32_t vertex_count, uint32_t vertex_stride, void *indices, uint32_t index_count, uint32_t index_byte_size);
	void draw_mesh(Mesh *mesh);

	ConstantBuffer get_constant_buffer(uint32_t size);
	void update_constant_buffer(ConstantBuffer *buffer, void *data);
	void set_constant_buffer(ConstantBuffer *buffer, uint32_t slot);

	CompiledShader compile_vertex_shader(void *source, uint32_t source_size);
	CompiledShader compile_pixel_shader(void *source, uint32_t source_size);
	CompiledShader compile_geometry_shader(void *source, uint32_t source_size);

	VertexShader get_vertex_shader(CompiledShader *compiled_shader, VertexInputDesc *vertex_input_descs, uint32_t vertex_input_count);
	VertexShader get_vertex_shader(void *shader_byte_code, uint32_t shader_size, VertexInputDesc *vertex_input_descs, uint32_t vertex_input_count);
	void set_vertex_shader(VertexShader *shader);

	PixelShader get_pixel_shader(CompiledShader *compiled_shader);
	PixelShader get_pixel_shader(void *shader_byte_code, uint32_t shader_size);
	void set_pixel_shader();
	void set_pixel_shader(PixelShader *shader);

	GeometryShader get_geometry_shader(CompiledShader *compiled_shader);
	GeometryShader get_geometry_shader(void *shader_byte_code, uint32_t shader_size);
	void set_geometry_shader();
	void set_geometry_shader(GeometryShader *shader);

	void swap_frames();

	void show_live_objects();

	void release();
	void release(SwapChain *swap_chain);
	void release(RenderTarget *buffer);
	void release(DepthBuffer *buffer);
	void release(Texture *texture);
	void release(TextureSampler *sampler);
	void release(Mesh *mesh);
	void release(ConstantBuffer *buffer);
	void release(VertexShader *shader);
	void release(PixelShader *shader);
	void release(GeometryShader *shader);
	void release(BlendState *state);
	void release(CompiledShader *shader);


	// UTILS
	// TODO: move to another file
	uint32_t get_vertex_input_desc_from_shader(char *vertex_string, uint32_t size, VertexInputDesc *vertex_input_descs);

}
