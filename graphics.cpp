#include "graphics.h"
#include "logging.h"
#include "memory.h"
#include <DXGI.h>
#include <d3dcompiler.h>

static GraphicsContext graphics_context_;
GraphicsContext *graphics_context = &graphics_context_;
static SwapChain swap_chain_;
static SwapChain *swap_chain = &swap_chain_;

void graphics::init(LUID *adapter_luid)
{

	UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	IDXGIFactory *idxgi_factory;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&idxgi_factory));
	if (FAILED(hr))
	{
		logging::print_error("Failed to create IDXGI factory.");
	}

	IDXGIAdapter *adapter = NULL;
	if (adapter_luid)
	{
		IDXGIAdapter *temp_adapter = NULL;
		for (uint32_t i = 0; idxgi_factory->EnumAdapters(i, &temp_adapter) != DXGI_ERROR_NOT_FOUND; ++i) 
		{ 
			DXGI_ADAPTER_DESC temp_adapter_desc;
			temp_adapter->GetDesc(&temp_adapter_desc);
			if(memcmp(&temp_adapter_desc.AdapterLuid, adapter_luid, sizeof(LUID)) == 0)
			{
				adapter = temp_adapter;
				break;
			}
			temp_adapter->Release();
		}
	}
	idxgi_factory->Release();

	D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
	D3D_FEATURE_LEVEL supported_feature_level;

	auto driver_type = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
	hr = D3D11CreateDevice(adapter, driver_type, NULL, flags, &feature_level, 1, D3D11_SDK_VERSION, &graphics_context->device, &supported_feature_level, &graphics_context->context);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create D3D11 Device.");
	}
	if (adapter)
	{
		adapter->Release();
	}
	
	// TODO: More flexible rasterizer state setting (per draw/render pass?)
	ID3D11RasterizerState *rasterizer_state;
	D3D11_RASTERIZER_DESC rasterizer_desc = {};
	rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	rasterizer_desc.CullMode = D3D11_CULL_BACK;
	rasterizer_desc.FrontCounterClockwise = TRUE;

	hr = graphics_context->device->CreateRasterizerState(&rasterizer_desc, &rasterizer_state);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create Rasterizer state");
	}

	graphics_context->context->RSSetState(rasterizer_state);
	rasterizer_state->Release();
}

void graphics::init_swap_chain(Window *window)
{
	DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};

	swap_chain_desc.BufferDesc.Width = window->window_width;
	swap_chain_desc.BufferDesc.Height = window->window_height;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount = 2;
	swap_chain_desc.OutputWindow = window->window_handle;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.Windowed = true;
	swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	IDXGIFactory * idxgi_factory;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&idxgi_factory));
	if (FAILED(hr))
	{
		logging::print_error("Failed to create IDXGI factory.");
	}

	hr = idxgi_factory->CreateSwapChain(graphics_context->device, &swap_chain_desc, &swap_chain->swap_chain);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create DXGI swap chain.");
	}
	idxgi_factory->Release();
}

RenderTarget graphics::get_render_target_window()
{
	RenderTarget buffer = {};

	HRESULT hr = swap_chain->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&buffer.texture);
	if (FAILED(hr))
	{
		logging::print_error("Failed to get swap chain buffer.");
	}

	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	hr = swap_chain->swap_chain->GetDesc(&swap_chain_desc);
	if (FAILED(hr))
	{
		logging::print_error("Failed to get swap chain description.");
	}

	D3D11_RENDER_TARGET_VIEW_DESC render_target_desc = {};
	render_target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	render_target_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	hr = graphics_context->device->CreateRenderTargetView(buffer.texture, &render_target_desc, &buffer.rt_view);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create swap chain render target.");
	}


	buffer.width = swap_chain_desc.BufferDesc.Width;
	buffer.height = swap_chain_desc.BufferDesc.Height;

	return buffer;
}

RenderTarget graphics::get_render_target(uint32_t width, uint32_t height, DXGI_FORMAT format)
{
	RenderTarget buffer = {};

	D3D11_TEXTURE2D_DESC texture_desc = {};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = format;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.SampleDesc.Quality = 0;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

	HRESULT hr = graphics_context->device->CreateTexture2D(&texture_desc, NULL, &buffer.texture);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create texture for render target buffer.");
	}

	D3D11_RENDER_TARGET_VIEW_DESC render_target_desc = {};
	render_target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	render_target_desc.Format = format;

	hr = graphics_context->device->CreateRenderTargetView(buffer.texture, &render_target_desc, &buffer.rt_view);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create render target view.");
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc = {};
	shader_resource_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_desc.Format = format;
	shader_resource_desc.Texture2D.MipLevels = 1;
	shader_resource_desc.Texture2D.MostDetailedMip = 0;

	hr = graphics_context->device->CreateShaderResourceView(buffer.texture, &shader_resource_desc, &buffer.sr_view);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create shader resource view.");
	}

	buffer.width = width;
	buffer.height = height;

	return buffer;
}

void graphics::clear_render_target(RenderTarget *buffer, float r, float g, float b, float a)
{
	float color[4] = { r, g, b, a };
	graphics_context->context->ClearRenderTargetView(buffer->rt_view, color);
}

DepthBuffer graphics::get_depth_buffer(uint32_t width, uint32_t height)
{
	DepthBuffer buffer = {};

	D3D11_TEXTURE2D_DESC texture_desc = {};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.SampleDesc.Quality = 0;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;

	HRESULT hr = graphics_context->device->CreateTexture2D(&texture_desc, NULL, &buffer.texture);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create texture for depth stencil buffer.");
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
	depth_stencil_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	hr = graphics_context->device->CreateDepthStencilView(buffer.texture, &depth_stencil_desc, &buffer.ds_view);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create depth stencil view.");
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc = {};
	shader_resource_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	shader_resource_desc.Texture2D.MipLevels = 1;
	shader_resource_desc.Texture2D.MostDetailedMip = 0;

	hr = graphics_context->device->CreateShaderResourceView(buffer.texture, &shader_resource_desc, &buffer.sr_view);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create shader resource view.");
	}

	buffer.width = width;
	buffer.height = height;

	return buffer;
}

void graphics::clear_depth_buffer(DepthBuffer *buffer)
{
	graphics_context->context->ClearDepthStencilView(buffer->ds_view, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void graphics::set_viewport(RenderTarget *buffer)
{
	D3D11_VIEWPORT viewport = {};
	viewport.Width = (float)buffer->width;
	viewport.Height = (float)buffer->height;
	viewport.MaxDepth = 1.0f;
	graphics_context->context->RSSetViewports(1, &viewport);
}

void graphics::set_viewport(DepthBuffer *buffer)
{
	D3D11_VIEWPORT viewport = {};
	viewport.Width = (float)buffer->width;
	viewport.Height = (float)buffer->height;
	viewport.MaxDepth = 1.0f;
	graphics_context->context->RSSetViewports(1, &viewport);
}

void graphics::set_viewport(Viewport *target_viewport)
{
	D3D11_VIEWPORT viewport = {};

	viewport.Width    = target_viewport->width;
	viewport.Height   = target_viewport->height;
	viewport.TopLeftX = target_viewport->x;
	viewport.TopLeftY = target_viewport->y;
	viewport.MaxDepth = 1.0f;

	graphics_context->context->RSSetViewports(1, &viewport);
}

void graphics::set_render_targets(DepthBuffer *buffer)
{
	ID3D11RenderTargetView **null = { NULL };
	graphics_context->context->OMSetRenderTargets(0, null, buffer->ds_view);
}

void graphics::set_render_targets(RenderTarget *buffer)
{
	graphics_context->context->OMSetRenderTargets(1, &buffer->rt_view, NULL);
}

void graphics::set_render_targets(RenderTarget *buffer, DepthBuffer *depth_buffer)
{
	graphics_context->context->OMSetRenderTargets(1, &buffer->rt_view, depth_buffer->ds_view);
}

void graphics::set_render_targets_viewport(RenderTarget *buffers, uint32_t buffer_count, DepthBuffer *depth_buffer)
{
	D3D11_VIEWPORT *viewports = memory::alloc_temp<D3D11_VIEWPORT>(buffer_count);

	for (uint32_t i = 0; i < buffer_count; ++i)
	{
		viewports[i] = {};
		viewports[i].Width = (float)buffers[i].width;
		viewports[i].Height = (float)buffers[i].height;
		viewports[i].MaxDepth = 1.0f;
	}

	ID3D11RenderTargetView **rt_views = memory::alloc_temp<ID3D11RenderTargetView *>(buffer_count);
	for (uint32_t i = 0; i < buffer_count; ++i)
	{
		rt_views[i] = buffers[i].rt_view;
	}

	graphics_context->context->RSSetViewports(buffer_count, viewports);
	graphics_context->context->OMSetRenderTargets(buffer_count, rt_views, depth_buffer->ds_view);

	memory::free_temp();
}

void graphics::set_render_targets_viewport(RenderTarget *buffer, DepthBuffer *depth_buffer)
{
	set_viewport(buffer);
	set_render_targets(buffer, depth_buffer);
}

void graphics::set_render_targets_viewport(RenderTarget *buffer)
{
	set_viewport(buffer);
	set_render_targets(buffer);
}

Texture graphics::get_texture(void *data, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t pixel_byte_count)
{
	Texture texture;

	D3D11_TEXTURE2D_DESC texture_desc = {};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = format;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.SampleDesc.Quality = 0;
	texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA texture_data = {};
	texture_data.pSysMem = data;
	texture_data.SysMemPitch = width * pixel_byte_count;

	HRESULT hr = graphics_context->device->CreateTexture2D(&texture_desc, &texture_data, &texture.texture);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create 2D texture.");
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc = {};
	shader_resource_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_desc.Format = format;
	shader_resource_desc.Texture2D.MipLevels = 1;
	shader_resource_desc.Texture2D.MostDetailedMip = 0;

	hr = graphics_context->device->CreateShaderResourceView(texture.texture, &shader_resource_desc, &texture.sr_view);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create shader resource view.");
	}

	texture.width = width;
	texture.height = height;

	return texture;
}

void graphics::set_texture(RenderTarget *buffer, uint32_t slot)
{
	graphics_context->context->PSSetShaderResources(slot, 1, &buffer->sr_view);
}

void graphics::set_texture(DepthBuffer *buffer, uint32_t slot)
{
	graphics_context->context->PSSetShaderResources(slot, 1, &buffer->sr_view);
}

void graphics::set_texture(Texture *texture, uint32_t slot)
{
	graphics_context->context->PSSetShaderResources(slot, 1, &texture->sr_view);
}

void graphics::unset_texture(uint32_t slot)
{
	ID3D11ShaderResourceView *null[] = { NULL };
	graphics_context->context->PSSetShaderResources(slot, 1, null);
}

BlendState graphics::get_blend_state(BlendType blend_type)
{
	BlendState blend_state = {};

	D3D11_BLEND_DESC blend_state_desc = {};
	if (blend_type == ALPHA_BLEND)
	{
		blend_state_desc.RenderTarget[0].BlendEnable 		   = TRUE;
		blend_state_desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
		blend_state_desc.RenderTarget[0].DestBlend 			   = D3D11_BLEND_INV_SRC_ALPHA;
		blend_state_desc.RenderTarget[0].BlendOp	 		   = D3D11_BLEND_OP_ADD;
		blend_state_desc.RenderTarget[0].SrcBlendAlpha	 	   = D3D11_BLEND_ONE;
		blend_state_desc.RenderTarget[0].DestBlendAlpha	 	   = D3D11_BLEND_ZERO;
		blend_state_desc.RenderTarget[0].BlendOpAlpha	 	   = D3D11_BLEND_OP_ADD;
	}
	blend_state_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	HRESULT hr = graphics_context->device->CreateBlendState(&blend_state_desc, &blend_state.state);
	if (FAILED(hr)) logging::print_error("Failed to create blend state.");

	return blend_state;
}	

// TODO: change to boolean instead of BlendState
void graphics::set_blend_state(BlendState *blend_state)
{
	float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	graphics_context->context->OMSetBlendState(blend_state->state, blend_factor, 0xffffffff);
}


D3D11_TEXTURE_ADDRESS_MODE m2m[3] =
{
	D3D11_TEXTURE_ADDRESS_CLAMP,
	D3D11_TEXTURE_ADDRESS_WRAP,
	D3D11_TEXTURE_ADDRESS_BORDER
};

TextureSampler graphics::get_texture_sampler(SampleMode mode)
{
	TextureSampler sampler;

	D3D11_TEXTURE_ADDRESS_MODE address_mode = m2m[mode];
	D3D11_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = address_mode;
	sampler_desc.AddressV = address_mode;
	sampler_desc.AddressW = address_mode;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampler_desc.MinLOD = -D3D11_FLOAT32_MAX;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
	HRESULT hr = graphics_context->device->CreateSamplerState(&sampler_desc, &sampler.sampler);
	if(FAILED(hr))
	{
		logging::print_error("Failed to create sampler state.");
	}

	return sampler;
}

void graphics::set_texture_sampler(TextureSampler *sampler, uint32_t slot) 
{
	graphics_context->context->PSSetSamplers(slot, 1, &sampler->sampler);
}


Mesh graphics::get_mesh(void *vertices, uint32_t vertex_count, uint32_t vertex_stride, void *indices, uint32_t index_count, uint32_t index_byte_size)
{
	Mesh mesh;
	
	D3D11_BUFFER_DESC vertex_buffer_desc = {};
	vertex_buffer_desc.ByteWidth = vertex_count * vertex_stride;
	vertex_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertex_buffer_data = {};
	vertex_buffer_data.pSysMem = vertices;
	vertex_buffer_data.SysMemPitch = vertex_stride;

	HRESULT hr = graphics_context->device->CreateBuffer(&vertex_buffer_desc, &vertex_buffer_data, &mesh.vertex_buffer);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create vertex buffer.");
	}

	D3D11_BUFFER_DESC indxe_buffer_desc = {};
	indxe_buffer_desc.ByteWidth = index_count * index_byte_size;
	indxe_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
	indxe_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA index_buffer_data = {};
	index_buffer_data.pSysMem = indices;
	index_buffer_data.SysMemPitch = index_byte_size;

	hr = graphics_context->device->CreateBuffer(&indxe_buffer_desc, &index_buffer_data, &mesh.index_buffer);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create index buffer.");
	}

	mesh.vertex_stride = vertex_stride;
	mesh.vertex_offset = 0;
	mesh.index_count = index_count;
	mesh.index_format = index_byte_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	mesh.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	return mesh;
}

void graphics::draw_mesh(Mesh *mesh)
{
	graphics_context->context->IASetVertexBuffers(0, 1, &mesh->vertex_buffer, &mesh->vertex_stride, &mesh->vertex_offset);
	graphics_context->context->IASetIndexBuffer(mesh->index_buffer, mesh->index_format, 0);

	graphics_context->context->IASetPrimitiveTopology(mesh->topology);
	graphics_context->context->DrawIndexed(mesh->index_count, 0, 0);
}

ConstantBuffer graphics::get_constant_buffer(uint32_t size)
{
	ConstantBuffer buffer = {};

	D3D11_BUFFER_DESC constant_buffer_desc = {};
	constant_buffer_desc.ByteWidth = size;
	constant_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	graphics_context->device->CreateBuffer(&constant_buffer_desc, NULL, &buffer.buffer);
	
	buffer.size = size;

	return buffer;
}

void graphics::update_constant_buffer(ConstantBuffer *buffer, void *data)
{
	D3D11_MAPPED_SUBRESOURCE mapped_buffer;
	graphics_context->context->Map(buffer->buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_buffer);
	memcpy(mapped_buffer.pData, data, buffer->size);
	graphics_context->context->Unmap(buffer->buffer, 0);
}

void graphics::set_constant_buffer(ConstantBuffer *buffer, uint32_t slot)
{
	graphics_context->context->PSSetConstantBuffers(slot, 1, &buffer->buffer);
	graphics_context->context->GSSetConstantBuffers(slot, 1, &buffer->buffer);
	graphics_context->context->VSSetConstantBuffers(slot, 1, &buffer->buffer);
}

CompiledShader compile_shader(void *source, uint32_t source_size, char *target)
{
	CompiledShader compiled_shader;

	uint32_t flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
#ifdef DEBUG
	flags |= D3DCOMPILE_DEBUG;
#endif
	ID3DBlob *error_msg;
	HRESULT hr = D3DCompile(source, source_size, NULL, NULL, NULL, "main", target, flags, NULL, &compiled_shader.blob, &error_msg);
	if (FAILED(hr))
	{
		logging::print_error("Failed to compile shader!");
	}

	if (error_msg)
	{
		logging::print((char *)error_msg->GetBufferPointer());
		error_msg->Release();
	}

	return compiled_shader;
}

CompiledShader graphics::compile_vertex_shader(void *source, uint32_t source_size)
{
	CompiledShader vertex_shader = compile_shader(source, source_size, "vs_5_0");
	return vertex_shader;
}

CompiledShader graphics::compile_pixel_shader(void *source, uint32_t source_size)
{
	CompiledShader pixel_shader = compile_shader(source, source_size, "ps_5_0");
	return pixel_shader;
}

CompiledShader graphics::compile_geometry_shader(void *source, uint32_t source_size)
{
	CompiledShader geometry_shader = compile_shader(source, source_size, "gs_5_0");
	return geometry_shader;
}

VertexShader graphics::get_vertex_shader(CompiledShader *compiled_shader, VertexInputDesc *vertex_input_descs, uint32_t vertex_input_count) 
{
	VertexShader vertex_shader = graphics::get_vertex_shader(compiled_shader->blob->GetBufferPointer(),
															 (uint32_t)compiled_shader->blob->GetBufferSize(),
															 vertex_input_descs, vertex_input_count);
	return vertex_shader;
}

VertexShader graphics::get_vertex_shader(void *shader_byte_code, uint32_t shader_size, VertexInputDesc *vertex_input_descs, uint32_t vertex_input_count)
{
	VertexShader shader = {};

	HRESULT hr = graphics_context->device->CreateVertexShader(shader_byte_code, shader_size, NULL, &shader.vertex_shader);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create vertex shader.");
	}

	D3D11_INPUT_ELEMENT_DESC *input_layout_desc = memory::alloc_temp<D3D11_INPUT_ELEMENT_DESC>(vertex_input_count);
	for (uint32_t i = 0; i < vertex_input_count; ++i)
	{
		input_layout_desc[i] = {};
		input_layout_desc[i].SemanticName = vertex_input_descs[i].semantic_name;
		input_layout_desc[i].Format = vertex_input_descs[i].format;
		input_layout_desc[i].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	}

	hr = graphics_context->device->CreateInputLayout(input_layout_desc, vertex_input_count, shader_byte_code, shader_size, &shader.input_layout);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create input layout.");
	}

	memory::free_temp();

	return shader;
}

void graphics::set_vertex_shader(VertexShader *shader)
{
	graphics_context->context->IASetInputLayout(shader->input_layout);
	graphics_context->context->VSSetShader(shader->vertex_shader, NULL, 0);
}

PixelShader graphics::get_pixel_shader(CompiledShader *compiled_shader)
{
	PixelShader pixel_shader = graphics::get_pixel_shader(compiled_shader->blob->GetBufferPointer(), (uint32_t)compiled_shader->blob->GetBufferSize());
	return pixel_shader;
}

PixelShader graphics::get_pixel_shader(void *shader_byte_code, uint32_t shader_size)
{
	PixelShader shader = {};

	HRESULT hr = graphics_context->device->CreatePixelShader(shader_byte_code, shader_size, NULL, &shader.pixel_shader);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create pixel shader.");
	}
	
	return shader;
}

void graphics::set_pixel_shader()
{
	graphics_context->context->PSSetShader(NULL, NULL, 0);
}

void graphics::set_pixel_shader(PixelShader *shader)
{
	graphics_context->context->PSSetShader(shader->pixel_shader, NULL, 0);
}

GeometryShader graphics::get_geometry_shader(CompiledShader *compiled_shader)
{
	GeometryShader geometry_shader = graphics::get_geometry_shader(compiled_shader->blob->GetBufferPointer(), (uint32_t) compiled_shader->blob->GetBufferSize());
	return geometry_shader;
}

GeometryShader graphics::get_geometry_shader(void *shader_byte_code, uint32_t shader_size)
{
	GeometryShader shader = {};

	HRESULT hr = graphics_context->device->CreateGeometryShader(shader_byte_code, shader_size, NULL, &shader.geometry_shader);
	if (FAILED(hr))
	{
		logging::print_error("Failed to create geometry shader.");
	}

	return shader;
}

void graphics::set_geometry_shader()
{
	graphics_context->context->GSSetShader(NULL, NULL, 0);
}

void graphics::set_geometry_shader(GeometryShader *shader)
{
	graphics_context->context->GSSetShader(shader->geometry_shader, NULL, 0);
}

void graphics::swap_frames()
{
	swap_chain->swap_chain->Present(0, 0);
}

void graphics::show_live_objects()
{
	ID3D11Debug *debug_device;
	graphics_context->device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&debug_device));
	debug_device->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
}

#define RELEASE_DX_RESOURCE(resource) if(resource) resource->Release(); resource = NULL;

void graphics::release()
{
	RELEASE_DX_RESOURCE(swap_chain->swap_chain);
	RELEASE_DX_RESOURCE(graphics_context->context);
	RELEASE_DX_RESOURCE(graphics_context->device);
}

void graphics::release(RenderTarget *buffer)
{
	RELEASE_DX_RESOURCE(buffer->rt_view);
	RELEASE_DX_RESOURCE(buffer->sr_view);
	RELEASE_DX_RESOURCE(buffer->texture);
}

void graphics::release(DepthBuffer *buffer)
{
	RELEASE_DX_RESOURCE(buffer->ds_view);
	RELEASE_DX_RESOURCE(buffer->sr_view);
	RELEASE_DX_RESOURCE(buffer->texture);
}

void graphics::release(Texture *texture)
{
	RELEASE_DX_RESOURCE(texture->texture);
	RELEASE_DX_RESOURCE(texture->sr_view);
}

void graphics::release(Mesh *mesh)
{
	RELEASE_DX_RESOURCE(mesh->vertex_buffer);
	RELEASE_DX_RESOURCE(mesh->index_buffer);
}

void graphics::release(VertexShader *shader)
{
	RELEASE_DX_RESOURCE(shader->vertex_shader);
	RELEASE_DX_RESOURCE(shader->input_layout);
}

void graphics::release(GeometryShader *shader)
{
	RELEASE_DX_RESOURCE(shader->geometry_shader);
}

void graphics::release(PixelShader *shader)
{
	RELEASE_DX_RESOURCE(shader->pixel_shader);
}

void graphics::release(ConstantBuffer *buffer)
{
	RELEASE_DX_RESOURCE(buffer->buffer);
}

void graphics::release(TextureSampler *sampler)
{
	RELEASE_DX_RESOURCE(sampler->sampler);
}

void graphics::release(BlendState *state)
{
	RELEASE_DX_RESOURCE(state->state);
}

void graphics::release(CompiledShader *shader)
{
	RELEASE_DX_RESOURCE(shader->blob);
}

