#include "graphics.h"
#include <d3dcompiler.h>
#include <stdio.h>
#ifdef DEBUG
#include<stdio.h>
#define PRINT_DEBUG(message, ...) {printf("ERROR in file %s on line %d: ", __FILE__, __LINE__); printf(message, __VA_ARGS__); printf("\n");}
#else
#define PRINT_DEBUG(message, ...)
#endif

// Global variables, only one of those for the whole application!

// Just so we don't have to reference graphics_context with &, we're doing this "trick"
static GraphicsContext graphics_context_;
GraphicsContext *graphics_context = &graphics_context_;

// The same as above
static SwapChain swap_chain_;
static SwapChain *swap_chain = &swap_chain_;

// We're simplifying blending to just two options - alpha blending and opaque (solid) blending
static ID3D11BlendState *blend_states[2];

// Let's store current blend type, useful when we want to restore original blending state in the end of the function
static BlendType current_blend_type;

static ID3D11RasterizerState *raster_states[2];

// Do the same for RasterType as for BlendType
static RasterType current_raster_type;

// 1MB buffer we can use as memory pool.
#define MEM_POOL_SIZE 1000000
static char *mem_pool;
static char *mem_pool_top;

/////////////////////////////////////////////////////
/// Public API
/////////////////////////////////////////////////////

bool graphics::init(LUID *adapter_luid) {
	UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	// Get adapter to use for creating D3D11Device
	IDXGIAdapter *adapter = NULL;

	// In case adapter LUID was specified, go through available adapters and pick the one with specified LUID
	if (adapter_luid) {
		// Create IDXGIFactory, needed for probing avaliable devices/adapters
		IDXGIFactory *idxgi_factory;
		HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&idxgi_factory));
		if (FAILED(hr)) {
			PRINT_DEBUG("Failed to create IDXGI factory.");
			return false;
		}

		IDXGIAdapter *temp_adapter = NULL;
		for (uint32_t i = 0; idxgi_factory->EnumAdapters(i, &temp_adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
			DXGI_ADAPTER_DESC temp_adapter_desc;
			temp_adapter->GetDesc(&temp_adapter_desc);
			if(memcmp(&temp_adapter_desc.AdapterLuid, adapter_luid, sizeof(LUID)) == 0) {
				adapter = temp_adapter;
				break;
			}
			temp_adapter->Release();
		}
		idxgi_factory->Release();
	}

	// Create D3D11Device and D3D11DeviceContext
	D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
	D3D_FEATURE_LEVEL supported_feature_level;

	auto driver_type = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
	HRESULT hr = D3D11CreateDevice(adapter, driver_type, NULL, flags, &feature_level, 1, D3D11_SDK_VERSION, &graphics_context->device, &supported_feature_level, &graphics_context->context);

	// Release adapter handle if not NULL
	if (adapter) {
		adapter->Release();
	}

	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create D3D11 Device.");
		return false;
	}

	// Initialize blending states
	// For solid blend state, blend_state_desc is zeroed.
	D3D11_BLEND_DESC blend_state_desc = {};
	blend_state_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = graphics_context->device->CreateBlendState(&blend_state_desc, &blend_states[BlendType::OPAQUE]);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create blend state.");
		return false;
	}

	// Initialize alpha blend state
	blend_state_desc.RenderTarget[0].BlendEnable 		   = TRUE;
	blend_state_desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
	blend_state_desc.RenderTarget[0].DestBlend 			   = D3D11_BLEND_INV_SRC_ALPHA;
	blend_state_desc.RenderTarget[0].BlendOp	 		   = D3D11_BLEND_OP_ADD;
	blend_state_desc.RenderTarget[0].SrcBlendAlpha	 	   = D3D11_BLEND_ONE;
	blend_state_desc.RenderTarget[0].DestBlendAlpha	 	   = D3D11_BLEND_INV_SRC_ALPHA;
	blend_state_desc.RenderTarget[0].BlendOpAlpha	 	   = D3D11_BLEND_OP_ADD;

	hr = graphics_context->device->CreateBlendState(&blend_state_desc, &blend_states[BlendType::ALPHA]);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create blend state.");
		return false;
	}

	current_blend_type = BlendType::OPAQUE;

	// Initialize rasterizer states
	// Initialize solid rasterizer state, let's follow RH coordinate system like sane people
	D3D11_RASTERIZER_DESC rasterizer_desc_solid = {};
	rasterizer_desc_solid.FillMode = D3D11_FILL_SOLID;
	rasterizer_desc_solid.CullMode = D3D11_CULL_NONE;
	rasterizer_desc_solid.FrontCounterClockwise = TRUE;

	hr = graphics_context->device->CreateRasterizerState(&rasterizer_desc_solid, &raster_states[RasterType::SOLID]);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create Rasterizer state");
		return false;
	}

	// Initialize wireframe rasterizer state, let's follow RH coordinate system like sane people
	D3D11_RASTERIZER_DESC rasterizer_desc_wireframe = {};
	rasterizer_desc_wireframe.FillMode = D3D11_FILL_WIREFRAME;
	rasterizer_desc_wireframe.CullMode = D3D11_CULL_BACK;
	rasterizer_desc_wireframe.FrontCounterClockwise = TRUE;

	hr = graphics_context->device->CreateRasterizerState(&rasterizer_desc_wireframe, &raster_states[RasterType::WIREFRAME]);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create Rasterizer state");
		return false;
	}

	current_raster_type = RasterType::SOLID;
	graphics_context->context->RSSetState(raster_states[current_raster_type]);

	// Allocate memory pool.
	mem_pool = (char *)malloc(MEM_POOL_SIZE);
	if(!mem_pool) {
		PRINT_DEBUG("Failed to allocate memory pool");
		return false;
	}
	mem_pool_top = mem_pool;

	return true;
}

bool graphics::init_swap_chain(HWND window, uint32_t window_width, uint32_t window_height) {
	DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};

	swap_chain_desc.BufferDesc.Width = window_width;
	swap_chain_desc.BufferDesc.Height = window_height;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount = 2;
	swap_chain_desc.OutputWindow = window;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.Windowed = true;
	swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	IDXGIFactory * idxgi_factory;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&idxgi_factory));
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create IDXGI factory.");
		return false;
	}

	hr = idxgi_factory->CreateSwapChain(graphics_context->device, &swap_chain_desc, &swap_chain->swap_chain);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create DXGI swap chain.");
		idxgi_factory->Release();
		return false;
	}

	idxgi_factory->Release();
	return true;
}

bool graphics::resize_swap_chain(uint32_t window_width, uint32_t window_height) {
	HRESULT hr = swap_chain->swap_chain->ResizeBuffers(
		0, window_width, window_height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	);
	if(FAILED(hr)) {
		PRINT_DEBUG("Failed to resize swap chain.");
		return false;
	}

	return true;
}

RenderTarget graphics::get_render_target_window(bool srgb) {
	// NOTE: buffer.sr_view is not filled and remains NULL - window render target cannot be used as a texture in shader
	RenderTarget buffer = {};

	HRESULT hr = swap_chain->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&buffer.texture);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to get swap chain buffer.");
		return RenderTarget{};
	}

	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	hr = swap_chain->swap_chain->GetDesc(&swap_chain_desc);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to get swap chain description.");
		return RenderTarget{};
	}

	D3D11_RENDER_TARGET_VIEW_DESC render_target_desc = {};
	DXGI_FORMAT format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	render_target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	render_target_desc.Format = format;
	hr = graphics_context->device->CreateRenderTargetView(buffer.texture, &render_target_desc, &buffer.rt_view);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create swap chain render target.");
		return RenderTarget{};
	}

	buffer.width = swap_chain_desc.BufferDesc.Width;
	buffer.height = swap_chain_desc.BufferDesc.Height;
	buffer.format = format;

	return buffer;
}

RenderTarget graphics::get_render_target(uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t num_samples) {
	RenderTarget buffer = {};

	D3D11_TEXTURE2D_DESC texture_desc = {};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = format;
	texture_desc.SampleDesc.Count = num_samples;
	texture_desc.SampleDesc.Quality = 0;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

	HRESULT hr = graphics_context->device->CreateTexture2D(&texture_desc, NULL, &buffer.texture);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create texture for render target buffer.");
		return RenderTarget{};
	}

	D3D11_RENDER_TARGET_VIEW_DESC render_target_desc = {};
	render_target_desc.ViewDimension = num_samples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
	render_target_desc.Format = format;

	hr = graphics_context->device->CreateRenderTargetView(buffer.texture, &render_target_desc, &buffer.rt_view);
	if (FAILED(hr)) {
		// Cleanup.
		buffer.texture->Release();

		PRINT_DEBUG("Failed to create render target view.");
		return RenderTarget{};
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc = {};
	shader_resource_desc.ViewDimension = num_samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_desc.Format = format;
	shader_resource_desc.Texture2D.MipLevels = 1;
	shader_resource_desc.Texture2D.MostDetailedMip = 0;

	hr = graphics_context->device->CreateShaderResourceView(buffer.texture, &shader_resource_desc, &buffer.sr_view);
	if (FAILED(hr)) {
		// Cleanup.
		buffer.texture->Release();
		buffer.rt_view->Release();

		PRINT_DEBUG("Failed to create shader resource view.");
		return RenderTarget{};
	}

	buffer.width = width;
	buffer.height = height;
	buffer.format = format;

	return buffer;
}

void graphics::clear_render_target(RenderTarget *buffer, float r, float g, float b, float a) {
	float color[4] = { r, g, b, a };
	graphics_context->context->ClearRenderTargetView(buffer->rt_view, color);
}

DepthBuffer graphics::get_depth_buffer(uint32_t width, uint32_t height, uint32_t num_samples) {
	DepthBuffer buffer = {};

	D3D11_TEXTURE2D_DESC texture_desc = {};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texture_desc.SampleDesc.Count = num_samples;
	texture_desc.SampleDesc.Quality = 0;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;

	HRESULT hr = graphics_context->device->CreateTexture2D(&texture_desc, NULL, &buffer.texture);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create texture for depth stencil buffer.");
		return DepthBuffer{};
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
	depth_stencil_desc.ViewDimension = num_samples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	hr = graphics_context->device->CreateDepthStencilView(buffer.texture, &depth_stencil_desc, &buffer.ds_view);
	if (FAILED(hr)) {
		// Cleanup.
		buffer.texture->Release();

		PRINT_DEBUG("Failed to create depth stencil view.");
		return DepthBuffer{};
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc = {};
	shader_resource_desc.ViewDimension = num_samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	shader_resource_desc.Texture2D.MipLevels = 1;
	shader_resource_desc.Texture2D.MostDetailedMip = 0;

	hr = graphics_context->device->CreateShaderResourceView(buffer.texture, &shader_resource_desc, &buffer.sr_view);
	if (FAILED(hr)) {
		// Cleanup.
		buffer.texture->Release();
		buffer.ds_view->Release();

		PRINT_DEBUG("Failed to create shader resource view.");
		return DepthBuffer{};
	}

	buffer.width = width;
	buffer.height = height;

	return buffer;
}

void graphics::clear_depth_buffer(DepthBuffer *buffer) {
	graphics_context->context->ClearDepthStencilView(buffer->ds_view, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void graphics::set_viewport(RenderTarget *buffer) {
	D3D11_VIEWPORT viewport = {};
	viewport.Width = (float)buffer->width;
	viewport.Height = (float)buffer->height;
	viewport.MaxDepth = 1.0f;
	graphics_context->context->RSSetViewports(1, &viewport);
}

void graphics::set_viewport(DepthBuffer *buffer) {
	D3D11_VIEWPORT viewport = {};
	viewport.Width = (float)buffer->width;
	viewport.Height = (float)buffer->height;
	viewport.MaxDepth = 1.0f;
	graphics_context->context->RSSetViewports(1, &viewport);
}

void graphics::set_viewport(Viewport *target_viewport) {
	D3D11_VIEWPORT viewport = {};

	viewport.Width    = target_viewport->width;
	viewport.Height   = target_viewport->height;
	viewport.TopLeftX = target_viewport->x;
	viewport.TopLeftY = target_viewport->y;
	viewport.MaxDepth = 1.0f;

	graphics_context->context->RSSetViewports(1, &viewport);
}

void graphics::set_render_targets(DepthBuffer *buffer) {
	ID3D11RenderTargetView **null = { NULL };
	graphics_context->context->OMSetRenderTargets(0, null, buffer->ds_view);
}

void graphics::set_render_targets(RenderTarget *buffer) {
	graphics_context->context->OMSetRenderTargets(1, &buffer->rt_view, NULL);
}

void graphics::set_render_targets(RenderTarget *buffer, DepthBuffer *depth_buffer) {
	graphics_context->context->OMSetRenderTargets(1, &buffer->rt_view, depth_buffer->ds_view);
}

void graphics::set_render_targets_viewport(RenderTarget *buffers, uint32_t buffer_count, DepthBuffer *depth_buffer) {
	char *original_mem_pool_top = mem_pool_top;
	D3D11_VIEWPORT *viewports = (D3D11_VIEWPORT *)mem_pool_top;
	mem_pool_top += sizeof(D3D11_VIEWPORT) * buffer_count;

	for (uint32_t i = 0; i < buffer_count; ++i) {
		viewports[i] = {};
		viewports[i].Width = (float)buffers[i].width;
		viewports[i].Height = (float)buffers[i].height;
		viewports[i].MaxDepth = 1.0f;
	}

	ID3D11RenderTargetView **rt_views = (ID3D11RenderTargetView **)mem_pool_top;
	mem_pool_top += sizeof(ID3D11RenderTargetView *) * buffer_count;

	for (uint32_t i = 0; i < buffer_count; ++i) {
		rt_views[i] = buffers[i].rt_view;
	}

	graphics_context->context->RSSetViewports(buffer_count, viewports);
	graphics_context->context->OMSetRenderTargets(buffer_count, rt_views, depth_buffer->ds_view);

	// "Free" memory.
	mem_pool_top = original_mem_pool_top;
}

void graphics::set_render_targets_viewport(RenderTarget *buffer, DepthBuffer *depth_buffer) {
	set_viewport(buffer);
	set_render_targets(buffer, depth_buffer);
}

void graphics::set_render_targets_viewport(RenderTarget *buffer) {
	set_viewport(buffer);
	set_render_targets(buffer);
}

Texture2D graphics::get_texture2D(void *data, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t pixel_byte_count, bool staging) {
	Texture2D texture;

	D3D11_TEXTURE2D_DESC texture_desc = {};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = format;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.SampleDesc.Quality = 0;
	texture_desc.Usage = staging ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = staging ? 0 : D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	texture_desc.CPUAccessFlags = staging ? D3D11_CPU_ACCESS_READ : 0;

	D3D11_SUBRESOURCE_DATA texture_data = {};
	texture_data.pSysMem = data;
	texture_data.SysMemPitch = width * pixel_byte_count;

	D3D11_SUBRESOURCE_DATA *texture_data_ptr = data ? &texture_data : NULL;
	HRESULT hr = graphics_context->device->CreateTexture2D(&texture_desc, texture_data_ptr, &texture.texture);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create 2D texture.");
		return Texture2D{};
	}

	if (!staging) {
		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc = {};
		shader_resource_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shader_resource_desc.Format = format;
		shader_resource_desc.Texture2D.MipLevels = 1;
		shader_resource_desc.Texture2D.MostDetailedMip = 0;

		hr = graphics_context->device->CreateShaderResourceView(texture.texture, &shader_resource_desc, &texture.sr_view);
		if (FAILED(hr)) {
			// Cleanup.
			texture.texture->Release();

			PRINT_DEBUG("Failed to create shader resource view.");
			return Texture2D{};
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_desc = {};
		unordered_access_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		unordered_access_desc.Format = format;
		unordered_access_desc.Texture2D.MipSlice = 0;

		hr = graphics_context->device->CreateUnorderedAccessView(texture.texture, &unordered_access_desc, &texture.ua_view);
		if (FAILED(hr)) {
			// Cleanup.
			texture.texture->Release();
			texture.sr_view->Release();

			PRINT_DEBUG("Failed to create unordered access view.");
			return Texture2D{};
		}
	}

	texture.width = width;
	texture.height = height;

	return texture;
}

void graphics::clear_texture(Texture2D *texture, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
	uint32_t clear_tex[4] = {r, g, b, a};
    graphics_context->context->ClearUnorderedAccessViewUint(texture->ua_view, clear_tex);
}

void graphics::clear_texture(Texture2D *texture, float r, float g, float b, float a) {
	float clear_tex[4] = {r, g, b, a};
    graphics_context->context->ClearUnorderedAccessViewFloat(texture->ua_view, clear_tex);
}

Texture3D graphics::get_texture3D(void *data, uint32_t width, uint32_t height, uint32_t depth, DXGI_FORMAT format, uint32_t pixel_byte_count, bool staging) {
	Texture3D texture;

	D3D11_TEXTURE3D_DESC texture_desc = {};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.Depth = depth;
	texture_desc.MipLevels = 1;
	texture_desc.Format = format;
	texture_desc.Usage = staging ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = staging ? 0 : D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	texture_desc.CPUAccessFlags = staging ? D3D11_CPU_ACCESS_READ : 0;

	D3D11_SUBRESOURCE_DATA texture_data = {};
	texture_data.pSysMem = data;
	texture_data.SysMemPitch = width * pixel_byte_count;
	texture_data.SysMemSlicePitch = width * height * pixel_byte_count;

	D3D11_SUBRESOURCE_DATA *texture_data_ptr = data ? &texture_data : NULL;
	HRESULT hr = graphics_context->device->CreateTexture3D(&texture_desc, texture_data_ptr, &texture.texture);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create 3D texture.");
		return Texture3D{};
	}

	if (!staging) {
		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc = {};
		shader_resource_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		shader_resource_desc.Format = format;
		shader_resource_desc.Texture3D.MipLevels = 1;
		shader_resource_desc.Texture3D.MostDetailedMip = 0;

		hr = graphics_context->device->CreateShaderResourceView(texture.texture, &shader_resource_desc, &texture.sr_view);
		if (FAILED(hr)) {
			// Cleanup.
			texture.texture->Release();

			PRINT_DEBUG("Failed to create shader resource view.");
			return Texture3D{};
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_desc = {};
		unordered_access_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		unordered_access_desc.Format = format;
		unordered_access_desc.Texture3D.MipSlice = 0;
		unordered_access_desc.Texture3D.FirstWSlice = 0;
		unordered_access_desc.Texture3D.WSize = depth;

		hr = graphics_context->device->CreateUnorderedAccessView(texture.texture, &unordered_access_desc, &texture.ua_view);
		if (FAILED(hr)) {
			// Cleanup.
			texture.texture->Release();
			texture.sr_view->Release();

			PRINT_DEBUG("Failed to create unordered access view.");
			return Texture3D{};
		}
	}

	texture.width = width;
	texture.height = height;
	texture.depth = depth;

	return texture;
}

void graphics::set_texture(RenderTarget *buffer, uint32_t slot) {
	graphics_context->context->PSSetShaderResources(slot, 1, &buffer->sr_view);
	graphics_context->context->CSSetShaderResources(slot, 1, &buffer->sr_view);
}

void graphics::set_texture(DepthBuffer *buffer, uint32_t slot) {
	graphics_context->context->PSSetShaderResources(slot, 1, &buffer->sr_view);
	graphics_context->context->CSSetShaderResources(slot, 1, &buffer->sr_view);
}

void graphics::set_texture(Texture2D *texture, uint32_t slot) {
	graphics_context->context->PSSetShaderResources(slot, 1, &texture->sr_view);
	graphics_context->context->CSSetShaderResources(slot, 1, &texture->sr_view);
}

void graphics::set_texture(Texture3D *texture, uint32_t slot) {
	graphics_context->context->PSSetShaderResources(slot, 1, &texture->sr_view);
	graphics_context->context->CSSetShaderResources(slot, 1, &texture->sr_view);
}

void graphics::set_texture(StructuredBuffer *buffer, uint32_t slot) {
	graphics_context->context->PSSetShaderResources(slot, 1, &buffer->sr_view);
	graphics_context->context->CSSetShaderResources(slot, 1, &buffer->sr_view);
	graphics_context->context->VSSetShaderResources(slot, 1, &buffer->sr_view);
}

void graphics::unset_texture(uint32_t slot) {
	ID3D11ShaderResourceView *null[] = { NULL };
	graphics_context->context->PSSetShaderResources(slot, 1, null);
	graphics_context->context->CSSetShaderResources(slot, 1, null);
}

void graphics::set_texture_compute(Texture2D *texture, uint32_t slot) {
	UINT init_counts = 0;
	graphics_context->context->CSSetUnorderedAccessViews(slot, 1, &texture->ua_view, &init_counts);
}

void graphics::set_texture_compute(Texture3D *texture, uint32_t slot) {
	UINT init_counts = 0;
	graphics_context->context->CSSetUnorderedAccessViews(slot, 1, &texture->ua_view, &init_counts);
}

void graphics::unset_texture_compute(uint32_t slot) {
	UINT init_counts = 0;
	ID3D11UnorderedAccessView *null[] = { NULL };
	graphics_context->context->CSSetUnorderedAccessViews(slot, 1, null, &init_counts);
}

void graphics::set_blend_state(BlendType type) {
	current_blend_type = type;
	float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	graphics_context->context->OMSetBlendState(blend_states[current_blend_type], blend_factor, 0xffffffff);
}

BlendType graphics::get_blend_state() {
	return current_blend_type;
}

void graphics::set_rasterizer_state(RasterType type) {
	current_raster_type = type;
	graphics_context->context->RSSetState(raster_states[current_raster_type]);
}

RasterType graphics::get_rasterizer_state() {
	return current_raster_type;
}

D3D11_TEXTURE_ADDRESS_MODE m2m[3] = {
	D3D11_TEXTURE_ADDRESS_CLAMP,
	D3D11_TEXTURE_ADDRESS_WRAP,
	D3D11_TEXTURE_ADDRESS_BORDER
};

TextureSampler graphics::get_texture_sampler(SampleMode mode, bool bilinear_filter) {
	TextureSampler sampler;

	D3D11_TEXTURE_ADDRESS_MODE address_mode = m2m[mode];
	D3D11_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter = bilinear_filter ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = address_mode;
	sampler_desc.AddressV = address_mode;
	sampler_desc.AddressW = address_mode;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampler_desc.MinLOD = -D3D11_FLOAT32_MAX;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
	HRESULT hr = graphics_context->device->CreateSamplerState(&sampler_desc, &sampler.sampler);
	if(FAILED(hr)) {
		return TextureSampler{};
	}

	return sampler;
}

void graphics::set_texture_sampler(TextureSampler *sampler, uint32_t slot) {
	graphics_context->context->PSSetSamplers(slot, 1, &sampler->sampler);
}


Mesh graphics::get_mesh(void *vertices, uint32_t vertex_count, uint32_t vertex_stride, void *indices, uint32_t index_count, uint32_t index_byte_size, D3D11_PRIMITIVE_TOPOLOGY topology) {
	Mesh mesh = {};

	D3D11_BUFFER_DESC vertex_buffer_desc = {};
	vertex_buffer_desc.ByteWidth = vertex_count * vertex_stride;
	vertex_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertex_buffer_data = {};
	vertex_buffer_data.pSysMem = vertices;
	vertex_buffer_data.SysMemPitch = vertex_stride;

	HRESULT hr = graphics_context->device->CreateBuffer(&vertex_buffer_desc, &vertex_buffer_data, &mesh.vertex_buffer);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create vertex buffer.");
		return Mesh{};
	}

	if (indices && index_count > 0) {
		D3D11_BUFFER_DESC index_buffer_desc = {};
		index_buffer_desc.ByteWidth = index_count * index_byte_size;
		index_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
		index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA index_buffer_data = {};
		index_buffer_data.pSysMem = indices;
		index_buffer_data.SysMemPitch = index_byte_size;

		hr = graphics_context->device->CreateBuffer(&index_buffer_desc, &index_buffer_data, &mesh.index_buffer);
		if (FAILED(hr)) {
			// Cleanup.
			mesh.vertex_buffer->Release();

			PRINT_DEBUG("Failed to create index buffer.");
			return Mesh{};
		}
	}

	mesh.vertex_stride = vertex_stride;
	mesh.vertex_offset = 0;
	mesh.vertex_count = vertex_count;
	mesh.index_count = index_count;
	mesh.index_format = index_byte_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	mesh.topology = topology;

	return mesh;
}

Mesh graphics::get_mesh(ByteAddressBuffer buffer, uint32_t vertex_count, uint32_t vertex_stride, D3D11_PRIMITIVE_TOPOLOGY topology) {
	Mesh mesh = {};

	mesh.vertex_buffer = buffer.buffer;
	mesh.vertex_stride = vertex_stride;
	mesh.vertex_offset = 0;
	mesh.vertex_count = vertex_count;
	mesh.topology = topology;

	return mesh;
}

void graphics::draw_mesh(Mesh *mesh) {
	graphics_context->context->IASetVertexBuffers(0, 1, &mesh->vertex_buffer, &mesh->vertex_stride, &mesh->vertex_offset);

	graphics_context->context->IASetPrimitiveTopology(mesh->topology);
	if(mesh->index_buffer) {
		graphics_context->context->IASetIndexBuffer(mesh->index_buffer, mesh->index_format, 0);
		graphics_context->context->DrawIndexed(mesh->index_count, 0, 0);
	} else {
		graphics_context->context->Draw(mesh->vertex_count, 0);
	}
}

void graphics::draw_mesh_instanced(Mesh *mesh, int instance_count) {
	graphics_context->context->IASetVertexBuffers(0, 1, &mesh->vertex_buffer, &mesh->vertex_stride, &mesh->vertex_offset);

	graphics_context->context->IASetPrimitiveTopology(mesh->topology);
	if(mesh->index_buffer) {
		graphics_context->context->IASetIndexBuffer(mesh->index_buffer, mesh->index_format, 0);
		graphics_context->context->DrawIndexedInstanced(mesh->index_count, instance_count, 0, 0, 0);
	} else {
		graphics_context->context->DrawInstanced(mesh->vertex_count, instance_count, 0, 0);
	}
}

ConstantBuffer graphics::get_constant_buffer(uint32_t size) {
	// Make sure that size is always multiple of 16.
	size = size % 16 == 0 ? size : (size / 16 + 1) * 16;

	ConstantBuffer buffer = {};
	buffer.size = size;

	D3D11_BUFFER_DESC constant_buffer_desc = {};
	constant_buffer_desc.ByteWidth = size;
	constant_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	graphics_context->device->CreateBuffer(&constant_buffer_desc, NULL, &buffer.buffer);

	return buffer;
}

ByteAddressBuffer graphics::get_byte_address_buffer(int size) {
	ByteAddressBuffer buffer = {};
	buffer.size = size;

	D3D11_BUFFER_DESC byte_address_buffer_desc = {};
	byte_address_buffer_desc.ByteWidth = 4 * size;
	byte_address_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	byte_address_buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_VERTEX_BUFFER ;
	byte_address_buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	HRESULT hr = graphics_context->device->CreateBuffer(&byte_address_buffer_desc, NULL, &buffer.buffer);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create buffer.");
		return ByteAddressBuffer{};
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_desc = {};
	unordered_access_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	unordered_access_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	unordered_access_desc.Buffer.FirstElement = 0;
	unordered_access_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	unordered_access_desc.Buffer.NumElements = size;

	hr = graphics_context->device->CreateUnorderedAccessView(buffer.buffer, &unordered_access_desc, &buffer.ua_view);
	if (FAILED(hr)) {
		// Cleanup.
		buffer.buffer->Release();

		PRINT_DEBUG("Failed to create unordered access view.");
		return ByteAddressBuffer{};
	}

	return buffer;
}

StructuredBuffer graphics::get_structured_buffer(int element_stride, int num_elements, bool staging) {
	StructuredBuffer buffer = {};
	buffer.size = element_stride * num_elements;

	D3D11_BUFFER_DESC structured_buffer_desc = {};
	structured_buffer_desc.ByteWidth = element_stride * num_elements;
	structured_buffer_desc.Usage = staging ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
	structured_buffer_desc.BindFlags = staging ? 0 : D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	structured_buffer_desc.CPUAccessFlags = staging ? D3D11_CPU_ACCESS_READ : D3D11_CPU_ACCESS_WRITE;
	structured_buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	structured_buffer_desc.StructureByteStride = element_stride;

	HRESULT hr = graphics_context->device->CreateBuffer(&structured_buffer_desc, NULL, &buffer.buffer);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create buffer.");
		return StructuredBuffer{};
	}

	if (!staging) {
		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc = {};
		shader_resource_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		shader_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		shader_resource_desc.Buffer.NumElements = num_elements;
		shader_resource_desc.Buffer.FirstElement = 0;

		hr = graphics_context->device->CreateShaderResourceView(buffer.buffer, &shader_resource_desc, &buffer.sr_view);
		if (FAILED(hr)) {
			// Cleanup.
			buffer.buffer->Release();

			PRINT_DEBUG("Failed to create shader resource view.");
			return StructuredBuffer{};
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_desc = {};
		unordered_access_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		unordered_access_desc.Format = DXGI_FORMAT_UNKNOWN;
		unordered_access_desc.Buffer.FirstElement = 0;
		unordered_access_desc.Buffer.Flags = 0;
		unordered_access_desc.Buffer.NumElements = num_elements;

		hr = graphics_context->device->CreateUnorderedAccessView(buffer.buffer, &unordered_access_desc, &buffer.ua_view);
		if (FAILED(hr)) {
			// Cleanup.
			buffer.buffer->Release();
			buffer.sr_view->Release();

			PRINT_DEBUG("Failed to create unordered access view.");
			return StructuredBuffer{};
		}
	}


	return buffer;
}

void graphics::update_constant_buffer(ConstantBuffer *buffer, void *data) {
	D3D11_MAPPED_SUBRESOURCE mapped_buffer;
	graphics_context->context->Map(buffer->buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_buffer);
	memcpy(mapped_buffer.pData, data, buffer->size);
	graphics_context->context->Unmap(buffer->buffer, 0);
}

void graphics::update_structured_buffer(StructuredBuffer *buffer, void *data) {
	graphics_context->context->UpdateSubresource(buffer->buffer, 0, NULL, data, 0, 0);
}

void graphics::set_constant_buffer(ConstantBuffer *buffer, uint32_t slot) {
	graphics_context->context->PSSetConstantBuffers(slot, 1, &buffer->buffer);
	graphics_context->context->GSSetConstantBuffers(slot, 1, &buffer->buffer);
	graphics_context->context->VSSetConstantBuffers(slot, 1, &buffer->buffer);
	graphics_context->context->CSSetConstantBuffers(slot, 1, &buffer->buffer);
}

void graphics::set_structured_buffer(StructuredBuffer *buffer, uint32_t slot) {
	UINT init_counts = 0;
	graphics_context->context->CSSetUnorderedAccessViews(slot, 1, &buffer->ua_view, &init_counts);
}

void graphics::unset_structured_buffer(uint32_t slot) {
	UINT init_counts = 0;
	ID3D11UnorderedAccessView *null[] = { NULL };
	graphics_context->context->CSSetUnorderedAccessViews(slot, 1, null, &init_counts);
}

void graphics::set_byte_address_buffer(ByteAddressBuffer *buffer, uint32_t slot) {
	UINT init_counts = 0;
	graphics_context->context->CSSetUnorderedAccessViews(slot, 1, &buffer->ua_view, &init_counts);
}

void graphics::copy_resource(StructuredBuffer *src, StructuredBuffer *dst) {
    graphics_context->context->CopyResource(dst->buffer, src->buffer);
}

void graphics::resolve_render_targets(RenderTarget *src, RenderTarget *dst) {
	graphics_context->context->ResolveSubresource(dst->texture, 0, src->texture, 0, dst->format);
}

void *graphics::read_resource(StructuredBuffer *buffer, void *data) {
	// Map the subresource.
	D3D11_MAPPED_SUBRESOURCE subresource;
    graphics_context->context->Map(buffer->buffer, 0, D3D11_MAP_READ, NULL, &subresource);

	// Allocate memory for the resource data (if not provided).
    if(!data) {
		data = malloc(buffer->size);
	}

	// Copy the resource data.
    memcpy(data, subresource.pData, buffer->size);

	// Unmap the subresource.
    graphics_context->context->Unmap(buffer->buffer, 0);

	return data;
}

CompiledShader compile_shader(void *source, uint32_t source_size, char *target, char **macro_defines = NULL, uint32_t macro_defines_count = 0) {
	CompiledShader compiled_shader;

	char *original_mem_pool_top = mem_pool_top;

	char **defines = NULL;
	if(macro_defines) {
		// Create NULL-terminated macro defines list.
		defines = (char **)mem_pool_top;
		mem_pool_top += sizeof(char *) * macro_defines_count + 2; // We need two additional NULL items.

		memcpy(defines, macro_defines, sizeof(char *) * macro_defines_count);
		defines[macro_defines_count] = NULL;
		defines[macro_defines_count + 1] = NULL;
	}

	uint32_t flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#ifdef DEBUG
	flags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob *error_msg;
	HRESULT hr = D3DCompile(source, source_size, NULL, (D3D_SHADER_MACRO *)defines, NULL, "main", target, flags, NULL, &compiled_shader.blob, &error_msg);

	// "Free" memory.
	mem_pool_top = original_mem_pool_top;

	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to compile shader!");
		if (error_msg) {
			PRINT_DEBUG((char *)error_msg->GetBufferPointer());
			error_msg->Release();
		}

		return CompiledShader{};
	}

	return compiled_shader;
}

CompiledShader graphics::compile_vertex_shader(void *source, uint32_t source_size, char **macro_defines, uint32_t macro_defines_count) {
	CompiledShader vertex_shader = compile_shader(source, source_size, "vs_5_0", macro_defines, macro_defines_count);
	return vertex_shader;
}

CompiledShader graphics::compile_pixel_shader(void *source, uint32_t source_size, char **macro_defines, uint32_t macro_defines_count) {
	CompiledShader pixel_shader = compile_shader(source, source_size, "ps_5_0", macro_defines, macro_defines_count);
	return pixel_shader;
}

CompiledShader graphics::compile_geometry_shader(void *source, uint32_t source_size, char **macro_defines, uint32_t macro_defines_count) {
	CompiledShader geometry_shader = compile_shader(source, source_size, "gs_5_0", macro_defines, macro_defines_count);
	return geometry_shader;
}

CompiledShader graphics::compile_compute_shader(void *source, uint32_t source_size, char **macro_defines, uint32_t macro_defines_count) {
	CompiledShader compute_shader = compile_shader(source, source_size, "cs_5_0", macro_defines, macro_defines_count);
	return compute_shader;
}

VertexShader graphics::get_vertex_shader(CompiledShader *compiled_shader, VertexInputDesc *vertex_input_descs, uint32_t vertex_input_count) {
	VertexShader vertex_shader = graphics::get_vertex_shader(compiled_shader->blob->GetBufferPointer(),
															 (uint32_t)compiled_shader->blob->GetBufferSize(),
															 vertex_input_descs, vertex_input_count);
	return vertex_shader;
}

VertexShader graphics::get_vertex_shader(void *shader_byte_code, uint32_t shader_size, VertexInputDesc *vertex_input_descs, uint32_t vertex_input_count) {

	VertexShader shader = {};
	HRESULT hr = graphics_context->device->CreateVertexShader(shader_byte_code, shader_size, NULL, &shader.vertex_shader);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create vertex shader.");
		return VertexShader{};
	}

	char *original_mem_pool_top = mem_pool_top;
	D3D11_INPUT_ELEMENT_DESC *input_layout_desc = (D3D11_INPUT_ELEMENT_DESC *) mem_pool_top;
	mem_pool_top += sizeof(D3D11_INPUT_ELEMENT_DESC) * vertex_input_count;

	for (uint32_t i = 0; i < vertex_input_count; ++i) {
		input_layout_desc[i] = {};
		input_layout_desc[i].SemanticName = vertex_input_descs[i].semantic_name;
		input_layout_desc[i].Format = vertex_input_descs[i].format;
		input_layout_desc[i].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	}

	hr = graphics_context->device->CreateInputLayout(input_layout_desc, vertex_input_count, shader_byte_code, shader_size, &shader.input_layout);

	// "Free" memory.
	mem_pool_top = original_mem_pool_top;

	if (FAILED(hr)) {
		// Cleanup.
		shader.vertex_shader->Release();

		PRINT_DEBUG("Failed to create input layout.");
		return VertexShader{};
	}

	return shader;
}

void graphics::set_vertex_shader(VertexShader *shader) {
	graphics_context->context->IASetInputLayout(shader->input_layout);
	graphics_context->context->VSSetShader(shader->vertex_shader, NULL, 0);
}

PixelShader graphics::get_pixel_shader(CompiledShader *compiled_shader) {
	PixelShader pixel_shader = graphics::get_pixel_shader(compiled_shader->blob->GetBufferPointer(),
														  (uint32_t)compiled_shader->blob->GetBufferSize());
	return pixel_shader;
}

PixelShader graphics::get_pixel_shader(void *shader_byte_code, uint32_t shader_size) {
	PixelShader shader = {};

	HRESULT hr = graphics_context->device->CreatePixelShader(shader_byte_code, shader_size, NULL, &shader.pixel_shader);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create pixel shader.");
		return PixelShader{};
	}

	return shader;
}

void graphics::set_pixel_shader() {
	graphics_context->context->PSSetShader(NULL, NULL, 0);
}

void graphics::set_pixel_shader(PixelShader *shader) {
	graphics_context->context->PSSetShader(shader->pixel_shader, NULL, 0);
}

GeometryShader graphics::get_geometry_shader(CompiledShader *compiled_shader) {
	GeometryShader geometry_shader = graphics::get_geometry_shader(
		compiled_shader->blob->GetBufferPointer(), (uint32_t) compiled_shader->blob->GetBufferSize()
	);
	return geometry_shader;
}

GeometryShader graphics::get_geometry_shader(void *shader_byte_code, uint32_t shader_size) {
	GeometryShader shader = {};

	HRESULT hr = graphics_context->device->CreateGeometryShader(shader_byte_code, shader_size, NULL, &shader.geometry_shader);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create geometry shader.");
		return GeometryShader{};
	}

	return shader;
}

void graphics::set_geometry_shader() {
	graphics_context->context->GSSetShader(NULL, NULL, 0);
}

void graphics::set_geometry_shader(GeometryShader *shader) {
	graphics_context->context->GSSetShader(shader->geometry_shader, NULL, 0);
}

ComputeShader graphics::get_compute_shader(CompiledShader *compiled_shader) {
	ComputeShader compute_shader = graphics::get_compute_shader(
		compiled_shader->blob->GetBufferPointer(), (uint32_t) compiled_shader->blob->GetBufferSize()
	);
	return compute_shader;
}

ComputeShader graphics::get_compute_shader(void *shader_byte_code, uint32_t shader_size) {
	ComputeShader shader = {};

	HRESULT hr = graphics_context->device->CreateComputeShader(shader_byte_code, shader_size, NULL, &shader.compute_shader);
	if (FAILED(hr)) {
		PRINT_DEBUG("Failed to create compute shader.");
		return ComputeShader{};
	}

	return shader;
}

void graphics::set_compute_shader() {
	graphics_context->context->CSSetShader(NULL, NULL, 0);
}

void graphics::set_compute_shader(ComputeShader *shader) {
	graphics_context->context->CSSetShader(shader->compute_shader, NULL, 0);
}

void graphics::run_compute(int group_x, int group_y, int group_z) {
	graphics_context->context->Dispatch(group_x, group_y, group_z);
}

void graphics::swap_frames() {
	swap_chain->swap_chain->Present(1, 0);
}

void graphics::show_live_objects() {
	ID3D11Debug *debug_device;
	graphics_context->device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&debug_device));
	debug_device->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
}

// is_ready functions

bool graphics::is_ready(Texture2D *texture) {
	return texture->texture && texture->sr_view;
}

bool graphics::is_ready(Texture3D *texture) {
	return texture->texture && texture->sr_view;
}

bool graphics::is_ready(RenderTarget *render_target) {
	return render_target->rt_view && render_target->texture;
}

bool graphics::is_ready(DepthBuffer *depth_buffer) {
	return depth_buffer->ds_view && depth_buffer->sr_view && depth_buffer->texture;
}

bool graphics::is_ready(Mesh *mesh) {
	return mesh->vertex_buffer && (!mesh->index_count || mesh->index_buffer);
}

bool graphics::is_ready(ConstantBuffer *buffer) {
	return buffer->buffer;
}

bool graphics::is_ready(StructuredBuffer *buffer) {
	return buffer->buffer && buffer->sr_view && buffer->ua_view;
}

bool graphics::is_ready(TextureSampler *sampler) {
	return sampler->sampler;
}

bool graphics::is_ready(VertexShader *shader) {
	return shader->vertex_shader && shader->input_layout;
}

bool graphics::is_ready(PixelShader *shader) {
	return shader->pixel_shader;
}

bool graphics::is_ready(ComputeShader *shader) {
	return shader->compute_shader;
}

bool graphics::is_ready(CompiledShader *shader) {
	return shader->blob;
}

// release functions

#define RELEASE_DX_RESOURCE(resource) if(resource) resource->Release(); resource = NULL;

void graphics::release() {
	RELEASE_DX_RESOURCE(swap_chain->swap_chain);
	RELEASE_DX_RESOURCE(blend_states[BlendType::OPAQUE]);
	RELEASE_DX_RESOURCE(blend_states[BlendType::ALPHA]);
	RELEASE_DX_RESOURCE(raster_states[RasterType::SOLID]);
	RELEASE_DX_RESOURCE(raster_states[RasterType::WIREFRAME]);

	// This is required so the swap chain is actually released.
	// https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-flush#deferred-destruction-issues-with-flip-presentation-swap-chains
	graphics_context->context->ClearState();
	graphics_context->context->Flush();

	RELEASE_DX_RESOURCE(graphics_context->context);
	RELEASE_DX_RESOURCE(graphics_context->device);
	free(mem_pool);
}

void graphics::release(RenderTarget *buffer) {
	RELEASE_DX_RESOURCE(buffer->rt_view);
	RELEASE_DX_RESOURCE(buffer->sr_view);
	RELEASE_DX_RESOURCE(buffer->texture);
}

void graphics::release(DepthBuffer *buffer) {
	RELEASE_DX_RESOURCE(buffer->ds_view);
	RELEASE_DX_RESOURCE(buffer->sr_view);
	RELEASE_DX_RESOURCE(buffer->texture);
}

void graphics::release(Texture2D *texture) {
	RELEASE_DX_RESOURCE(texture->sr_view);
	RELEASE_DX_RESOURCE(texture->ua_view);
	RELEASE_DX_RESOURCE(texture->texture);
}

void graphics::release(Texture3D *texture) {
	RELEASE_DX_RESOURCE(texture->sr_view);
	RELEASE_DX_RESOURCE(texture->ua_view);
	RELEASE_DX_RESOURCE(texture->texture);
}

void graphics::release(Mesh *mesh) {
	RELEASE_DX_RESOURCE(mesh->vertex_buffer);
	RELEASE_DX_RESOURCE(mesh->index_buffer);
}

void graphics::release(VertexShader *shader) {
	RELEASE_DX_RESOURCE(shader->vertex_shader);
	RELEASE_DX_RESOURCE(shader->input_layout);
}

void graphics::release(GeometryShader *shader) {
	RELEASE_DX_RESOURCE(shader->geometry_shader);
}

void graphics::release(PixelShader *shader) {
	RELEASE_DX_RESOURCE(shader->pixel_shader);
}

void graphics::release(ComputeShader *shader) {
	RELEASE_DX_RESOURCE(shader->compute_shader);
}

void graphics::release(ConstantBuffer *buffer) {
	RELEASE_DX_RESOURCE(buffer->buffer);
}

void graphics::release(StructuredBuffer *buffer) {
	RELEASE_DX_RESOURCE(buffer->buffer);
	RELEASE_DX_RESOURCE(buffer->ua_view);
	RELEASE_DX_RESOURCE(buffer->sr_view);
}

void graphics::release(ByteAddressBuffer *buffer) {
	RELEASE_DX_RESOURCE(buffer->buffer);
	RELEASE_DX_RESOURCE(buffer->ua_view);
}

void graphics::release(TextureSampler *sampler) {
	RELEASE_DX_RESOURCE(sampler->sampler);
}

void graphics::release(CompiledShader *shader) {
	RELEASE_DX_RESOURCE(shader->blob);
}


////////////////////////////////////////////////
/// PROFILING API
////////////////////////////////////////////////

ProfilingBlock graphics::get_profiling_block() {
	ProfilingBlock result = {};
    
	// Create disjoint timestamp query.
	D3D11_QUERY_DESC desc = {};
    desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    graphics_context->device->CreateQuery(&desc, &result.disjoint);

	// Create start and end timestamp queries.
    desc.Query = D3D11_QUERY_TIMESTAMP;
    graphics_context->device->CreateQuery(&desc, &result.start);
    graphics_context->device->CreateQuery(&desc, &result.end);

	return result;
}

void graphics::start_profiling_block(ProfilingBlock *block) {
	graphics_context->context->Begin(block->disjoint);
    graphics_context->context->End(block->start);
}

void graphics::end_profiling_block(ProfilingBlock *block) {
    graphics_context->context->End(block->end);
	graphics_context->context->End(block->disjoint);
}

float graphics::get_latest_profiling_time(ProfilingBlock *block) {
	// Wait for data to be available.
    while (graphics_context->context->GetData(block->disjoint, NULL, 0, 0) == S_FALSE) {
        Sleep(1);
    }

	// Get disjoint query data.
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_query_data;
    graphics_context->context->GetData(block->disjoint, &disjoint_query_data, sizeof(disjoint_query_data), 0);
	if (disjoint_query_data.Disjoint) {
		return -1.0f;
	}

	// Get start and end timestamps.
	uint64_t start, end;
    graphics_context->context->GetData(block->start, &start, sizeof(start), 0);
    graphics_context->context->GetData(block->end, &end, sizeof(end), 0);
    
	// Compute difference between start and end in seconds.
	uint64_t diff = end - start;
    float result = float(double(diff) / double(disjoint_query_data.Frequency));

	return result;
}

////////////////////////////////////////////////
/// HIGHER LEVEL API
////////////////////////////////////////////////

int32_t graphics::get_vertex_input_desc_from_shader(char *vertex_string, uint32_t size, VertexInputDesc * vertex_input_descs) {
	const char *struct_name = "VertexInput";
	char *c = vertex_string;
	enum State {
		SEARCHING,
		PARSING_TYPE,
		SKIPPING_NAME,
		PARSING_SEMANTIC_NAME
	};

	State state = SEARCHING;
	uint32_t type_length = 0;
	uint32_t semantic_name_length = 0;
	uint32_t type = 0;

#define SHADER_TYPE_FLOAT4 0
#define SHADER_TYPE_FLOAT2 1
#define SHADER_TYPE_FLOAT3 2

	char *types[] = {
		"float4", "float2", "float3", "int4", "uint"
	};

	DXGI_FORMAT formats[] {
		DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_SINT, DXGI_FORMAT_R32_UINT
	};

	uint32_t i = 0;
	int32_t vertex_input_count = 0;

	while (i < size) {
		switch (state) {
		case SEARCHING:
		{
			if (strncmp(c, struct_name, strlen(struct_name)) == 0) {
				state = PARSING_TYPE;
				i += (uint32_t)strlen(struct_name);
				c += (uint32_t)strlen(struct_name);
			}
		}
		break;
		case PARSING_TYPE:
		{
			if (*c == ' ' && type_length > 0) {
				for (uint32_t j = 0; j < ARRAYSIZE(types); ++j) {
					if (strncmp(c - type_length, types[j], type_length) == 0) {
						type = j;

						state = SKIPPING_NAME;
						type_length = 0;

						break;
					}
				}
			} else if (isalnum(*c)) {
				type_length++;
			}
		}
		break;
		case SKIPPING_NAME:
		{
			if (*c == ':') {
				state = PARSING_SEMANTIC_NAME;
			}
		}
		break;
		case PARSING_SEMANTIC_NAME:
		{
			if ((isspace(*c) || *c == ';') && semantic_name_length > 0) {
				if(semantic_name_length >= MAX_SEMANTIC_NAME_LENGTH) {
					return -1;
				}
				if (vertex_input_descs && strncmp(c - semantic_name_length, "SV_InstanceID", semantic_name_length) != 0) {
					vertex_input_descs[vertex_input_count].format = formats[type];
					memcpy(vertex_input_descs[vertex_input_count].semantic_name, c - semantic_name_length, semantic_name_length);
					vertex_input_descs[vertex_input_count].semantic_name[semantic_name_length] = 0;
				}
				vertex_input_count++;

				state = PARSING_TYPE;
				semantic_name_length = 0;
			} else if (isalnum(*c) || *c == '_') {
				semantic_name_length++;
			}
		}
		}
		++c; ++i;
	}

	return vertex_input_count;
}

VertexShader graphics::get_vertex_shader_from_code(char *code, uint32_t code_length, char **macro_defines, uint32_t macro_defines_count) {
	// Compile shader
    CompiledShader vertex_shader_compiled = graphics::compile_vertex_shader(code, code_length, macro_defines, macro_defines_count);
	if(!graphics::is_ready(&vertex_shader_compiled)) {
		return VertexShader{};
	}

	char *original_mem_pool_top = mem_pool_top;
	// Get VertexInpuDescs
    uint32_t vertex_input_count = graphics::get_vertex_input_desc_from_shader(code, code_length, NULL);
	VertexInputDesc *vertex_input_descs = (VertexInputDesc *) mem_pool_top;
	mem_pool_top += sizeof(VertexInputDesc) * vertex_input_count;

	graphics::get_vertex_input_desc_from_shader(code, code_length, vertex_input_descs);

	// Get VertexShader object
    VertexShader vertex_shader = graphics::get_vertex_shader(&vertex_shader_compiled, vertex_input_descs, vertex_input_count);
    graphics::release(&vertex_shader_compiled);

	// "Free" memory.
	mem_pool_top = original_mem_pool_top;

	return vertex_shader;
}


PixelShader graphics::get_pixel_shader_from_code(char *code, uint32_t code_length, char **macro_defines, uint32_t macro_defines_count) {
	CompiledShader pixel_shader_compiled = graphics::compile_pixel_shader(code, code_length, macro_defines, macro_defines_count);
	if(!graphics::is_ready(&pixel_shader_compiled)) {
		return PixelShader{};
	}

	PixelShader pixel_shader = graphics::get_pixel_shader(&pixel_shader_compiled);
	graphics::release(&pixel_shader_compiled);

	return pixel_shader;
}

ComputeShader graphics::get_compute_shader_from_code(char *code, uint32_t code_length, char **macro_defines, uint32_t macro_defines_count) {
	CompiledShader compute_shader_compiled = graphics::compile_compute_shader(code, code_length, macro_defines, macro_defines_count);
	if(!graphics::is_ready(&compute_shader_compiled)) {
		return ComputeShader{};
	}

    ComputeShader compute_shader = graphics::get_compute_shader(&compute_shader_compiled);
    graphics::release(&compute_shader_compiled);

	return compute_shader;
}

Mesh graphics::get_quad_mesh() {
	float quad_vertices[] = {
		-1.0f, -1.0f, 0.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f,

		-1.0f, -1.0f, 0.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 0.0f,
	};

	const uint32_t quad_vertices_stride = sizeof(float) * 6;
	const uint32_t quad_vertices_count = 6;

	Mesh mesh = graphics::get_mesh(quad_vertices, quad_vertices_count, quad_vertices_stride, NULL, 0, 0);
	return mesh;
}