/* MegaZeux
 *
 * Copyright (C) 2018 Joel Bouchard Lamontagne <logicow@gmail.com>
 * Copyright (C) 2006-2007 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
 
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "platform.h"
#include "render.h"
#include "render_layer.h"
#include "renderers.h"
#include "util.h"
#include "data.h"

#ifdef CONFIG_SDL
#include "render_sdl.h"
#endif

#include "render_d3d11.h"

#define D3DCALL(x) if(x != S_OK) { debug("D3D call fail at line %d in file %s \n", __LINE__, __FILE__); }

#define LAYER_DATA_TEXTURE_WIDTH 2112

extern CORE_LIBSPEC Uint32 sdl_window_id;
static void d3d11_render_layer(struct graphics_data *g, struct video_layer *layer);

/*

D3D11 renderer by Joel Bouchard-Lamontagne

The main idea is to have direct copies from game data into our textures and buffers without any conversion.

depth_state: render front to back, only keep if z is less (depth is 0 to 1, we use a 16-bit variable and divide by 2^16 - 1)
depth_buffer: 16-bit unorm

palette_hw: ARGB 8-bit unorm, we copy the UI palette to position 256 so 
            it's always in the same spot regardless of if we're using SMZX mode or not
            first line is colors, second line is SMZX indices
            retrieve individual SMZX index by reading the ARGB color and keeping only the component we want

tileset_hw: 8-bit unsigned, X coordinate is 64 characters wide for line alignment purposes so we don't need padding
            as a side effect, any character modification requires changing the entire 64 wide row
            pixel shader does the work of reading the 8-bit row and using bitwise ops to keep the pixel bit we want

layer_data_hw: direct copy of char_element array. Texture is 81*26 wide + some padding, and 512 high to use 1 row per layer
               technically means an 1x1 sprite requires a copy of the entire row so this could be improved.
               Maybe split up the CopySubresourceRegion calls between large layers and small layers or something.

layers_constant_buffer_staging: constant buffer using d3d11_const_layer struct
                                I think it needs to be a multiple of 16 bytes
                                transparent_color actually uses 10 bits and use_smzx uses 6 because we need a 257th transparency color

Mouse pointer is an 1x1 layer using a copy of the topmost layer with colors inverted
Cursor is a bunch of 1x1 layers using the underscore character at the right position
*/

void d3d11_init_screen_create_window(struct d3d11_render_data *s, struct config_info *conf)
{
  int desktopX = GetSystemMetrics(SM_CXSCREEN);
  int desktopY = GetSystemMetrics(SM_CYSCREEN);
  int scaleX = (desktopX - 16) / 640;
  int scaleY = (desktopY - 64) / 350;
  int scale = scaleX < scaleY ? scaleX : scaleY;
  s->screen_width = 640 * scale;
  s->screen_height = 350 * scale;
  
  if(conf->fullscreen && conf->resolution_width > 0 && conf->resolution_height > 0)
  {
    s->screen_width = conf->resolution_width;
    s->screen_height = conf->resolution_height;
  }
  else if(conf->window_width > 0 && conf->window_height > 0)
  {
    s->screen_width = conf->window_width;
    s->screen_height = conf->window_height;
  }
  
  unsigned int flags = conf->allow_resize ? SDL_WINDOW_RESIZABLE : 0;
  if(conf->fullscreen)
  {
   flags = 0;
  }
  
  SDL_Window *window = SDL_CreateWindow("MegaZeux",
  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, s->screen_width, s->screen_height,
  flags);
  
  sdl_window_id = SDL_GetWindowID(window);
}

void d3d11_init_screen_create_device(struct d3d11_render_data *s, struct config_info *conf)
{
  DXGI_SWAP_CHAIN_DESC swapChainDesc;
  int debug_flag = 0; //D3D11_CREATE_DEVICE_DEBUG;
  D3D_FEATURE_LEVEL level;
  
  ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
  swapChainDesc.BufferCount = 2;
  swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.BufferDesc.Height = s->screen_height;
  swapChainDesc.BufferDesc.Width = s->screen_width;
  swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
  swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
  swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
  swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.Flags = 0;
  swapChainDesc.OutputWindow = GetActiveWindow();
  swapChainDesc.SampleDesc.Count = 1;
  swapChainDesc.SampleDesc.Quality = 0;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.Windowed = true;
  
  D3DCALL(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
   debug_flag, NULL, 0, D3D11_SDK_VERSION, &swapChainDesc, &s->swap_chain,
   &s->device, &level, &s->context));
  
  //Disable built-in alt-enter
  IDXGIDevice * pDXGIDevice;
  D3DCALL( s->device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice) );

  IDXGIAdapter * pDXGIAdapter;
  D3DCALL( pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter) );

  IDXGIFactory * pIDXGIFactory;
  pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&pIDXGIFactory);

  pIDXGIFactory->MakeWindowAssociation(GetActiveWindow(), DXGI_MWA_NO_ALT_ENTER);
}

void d3d11_init_screen_create_backbuffer(struct d3d11_render_data *s)
{
  int v_width, v_height;
  D3D11_TEXTURE2D_DESC texture_desc;
  
  s->swap_chain->GetBuffer(0, IID_PPV_ARGS(&s->render_target));
  D3DCALL(s->device->CreateRenderTargetView(s->render_target, 0, &s->rtv));
  
  fix_viewport_ratio(s->screen_width, s->screen_height, &v_width, &v_height, s->screen_ratio);
  
  s->viewport.Height = (float)v_height;
  s->viewport.Width = (float)v_width;
  s->viewport.TopLeftX = (s->screen_width - v_width)*0.5f;
  s->viewport.TopLeftY = (s->screen_height - v_height)*0.5f;
  s->viewport.MinDepth = 0.0f;
  s->viewport.MaxDepth = 1.0f;
  
  s->rect.left = (s->screen_width - v_width)*0.5f;
  s->rect.right = (s->screen_width + v_width)*0.5f;
  s->rect.top = (s->screen_height - v_height)*0.5f;
  s->rect.bottom = (s->screen_height + v_height)*0.5f;
  
  ZeroMemory(&texture_desc, sizeof(D3D11_TEXTURE2D_DESC));
  texture_desc.ArraySize = 1;
  texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  texture_desc.CPUAccessFlags = 0;
  texture_desc.Format = DXGI_FORMAT_D16_UNORM;
  texture_desc.Height = s->screen_height;
  texture_desc.Width = s->screen_width;
  texture_desc.MipLevels = 1;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.SampleDesc.Quality = 0;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.MiscFlags = 0;
  D3DCALL(s->device->CreateTexture2D(&texture_desc, NULL, &s->depth_buffer));
  
  D3DCALL(s->device->CreateDepthStencilView(s->depth_buffer, 0, &s->dsv));
}

void d3d11_init_screen_create_palette(struct d3d11_render_data *s)
{
  D3D11_TEXTURE2D_DESC texture_desc;
  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
  
  ZeroMemory(&texture_desc, sizeof(D3D11_TEXTURE2D_DESC));
  texture_desc.ArraySize = 1;
  texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  texture_desc.CPUAccessFlags = 0;
  texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texture_desc.Height = 2;
  texture_desc.Width = FULL_PAL_SIZE;
  texture_desc.MipLevels = 1;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.SampleDesc.Quality = 0;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.MiscFlags = 0;
  D3DCALL(s->device->CreateTexture2D(&texture_desc, NULL, &s->palette_hw));
  
  texture_desc.BindFlags = 0;
  texture_desc.Usage = D3D11_USAGE_STAGING;
  texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  D3DCALL(s->device->CreateTexture2D(&texture_desc, NULL, &s->palette_staging));
  
  ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
  srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MipLevels = 1;
  srv_desc.Texture2D.MostDetailedMip = 0;

  D3DCALL(s->device->CreateShaderResourceView(s->palette_hw, &srv_desc, &s->palette_srv));
}

void d3d11_init_screen_create_tileset(struct d3d11_render_data *s)
{
  D3D11_TEXTURE2D_DESC texture_desc;
  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
  
  ZeroMemory(&texture_desc, sizeof(D3D11_TEXTURE2D_DESC));
  texture_desc.ArraySize = 1;
  texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  texture_desc.CPUAccessFlags = 0;
  texture_desc.Format = DXGI_FORMAT_R8_UINT;
  texture_desc.Height = FULL_CHARSET_SIZE / 64;
  texture_desc.Width = 64 * 14;
  texture_desc.MipLevels = 1;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.SampleDesc.Quality = 0;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.MiscFlags = 0;
  D3DCALL(s->device->CreateTexture2D(&texture_desc, NULL, &s->tileset_hw));
  
  texture_desc.BindFlags = 0;
  texture_desc.Usage = D3D11_USAGE_STAGING;
  texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  D3DCALL(s->device->CreateTexture2D(&texture_desc, NULL, &s->tileset_staging));
  
  ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
  srv_desc.Format = DXGI_FORMAT_R8_UINT;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MipLevels = 1;
  srv_desc.Texture2D.MostDetailedMip = 0;

  D3DCALL(s->device->CreateShaderResourceView(s->tileset_hw, &srv_desc, &s->tileset_srv));
}

void d3d11_init_screen_create_layer_data(struct d3d11_render_data *s)
{
  D3D11_TEXTURE2D_DESC texture_desc;
  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
  D3D11_BUFFER_DESC bufferDescription;
  
  ZeroMemory(&texture_desc, sizeof(D3D11_TEXTURE2D_DESC));
  texture_desc.ArraySize = 1;
  texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  texture_desc.CPUAccessFlags = 0;
  texture_desc.Format = DXGI_FORMAT_R16G16_UINT;
  texture_desc.Height = TEXTVIDEO_LAYERS;
  texture_desc.Width = LAYER_DATA_TEXTURE_WIDTH;
  texture_desc.MipLevels = 1;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.SampleDesc.Quality = 0;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.MiscFlags = 0;
  D3DCALL(s->device->CreateTexture2D(&texture_desc, NULL, &s->layer_data_hw));
  
  texture_desc.BindFlags = 0;
  texture_desc.Usage = D3D11_USAGE_STAGING;
  texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  D3DCALL(s->device->CreateTexture2D(&texture_desc, NULL, &s->layer_data_staging));
  
  
  ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
  srv_desc.Format = DXGI_FORMAT_R16G16_UINT;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MipLevels = 1;
  srv_desc.Texture2D.MostDetailedMip = 0;

  D3DCALL(s->device->CreateShaderResourceView(s->layer_data_hw, &srv_desc, &s->layer_data_srv));
  
  ZeroMemory(&bufferDescription, sizeof(bufferDescription));
  bufferDescription.ByteWidth = TEXTVIDEO_LAYERS * sizeof(d3d11_const_layer);
  bufferDescription.Usage = D3D11_USAGE_DEFAULT;
  bufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bufferDescription.CPUAccessFlags = 0;
  bufferDescription.MiscFlags = 0;
  bufferDescription.StructureByteStride = sizeof(d3d11_const_layer);
  
  D3DCALL(s->device->CreateBuffer(&bufferDescription, NULL, &s->layers_constant_buffer_hw));
  
  bufferDescription.Usage = D3D11_USAGE_STAGING;
  bufferDescription.BindFlags = 0;
  bufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  
  D3DCALL(s->device->CreateBuffer(&bufferDescription, NULL, &s->layers_constant_buffer_staging));
}

static char *d3d11_load_string(const char *filename, unsigned long &size)
{
  char *buffer = NULL;
  FILE *f;

  f = fopen_unsafe(filename, "rb");
  if(!f)
    goto err_out;

  size = ftell_and_rewind(f);
  if(!size)
    goto err_close;

  buffer = (char*)cmalloc(size + 1);
  buffer[size] = '\0';

  if(fread(buffer, size, 1, f) != 1)
  {
    free(buffer);
    buffer = NULL;
  }

err_close:
  fclose(f);
err_out:
  return buffer;
}

void d3d11_init_screen_create_shader(struct d3d11_render_data *s)
{
  unsigned long size_vertex_cso;
  unsigned long size_pixel_cso;
  char *path;
  char *vertex_cso;
  char *pixel_cso;
  D3D11_BLEND_DESC blend_desc;
  D3D11_RASTERIZER_DESC raster_desc;
  D3D11_INPUT_ELEMENT_DESC inputElementDescs[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
  };
  D3D11_DEPTH_STENCIL_DESC depth_desc;
  
  path = (char*)cmalloc(MAX_PATH);
  strcpy(path, mzx_res_get_by_id(SHADERS_D3D11_VERT));
  vertex_cso = d3d11_load_string(path, size_vertex_cso);
  
  strcpy(path, mzx_res_get_by_id(SHADERS_D3D11_FRAG));
  pixel_cso = d3d11_load_string(path, size_pixel_cso);
  
  D3DCALL(s->device->CreatePixelShader(pixel_cso, size_pixel_cso, NULL, &s->pixel_shader));
  D3DCALL(s->device->CreateVertexShader(vertex_cso, size_vertex_cso, NULL, &s->vertex_shader));
  
  ZeroMemory(&blend_desc, sizeof(D3D11_BLEND_DESC));
  blend_desc.AlphaToCoverageEnable = false;
  blend_desc.IndependentBlendEnable = true;
  blend_desc.RenderTarget[0].BlendEnable = false;
  blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

  ZeroMemory(&raster_desc, sizeof(D3D11_RASTERIZER_DESC));
  raster_desc.FillMode = D3D11_FILL_SOLID;
  raster_desc.CullMode = D3D11_CULL_NONE;
  raster_desc.FrontCounterClockwise = false;
  raster_desc.DepthBias = 0;
  raster_desc.DepthBiasClamp = 0.0f;
  raster_desc.SlopeScaledDepthBias = 0.0f;
  raster_desc.DepthClipEnable = false;
  raster_desc.MultisampleEnable = false;
  raster_desc.AntialiasedLineEnable = false;
  raster_desc.ScissorEnable = false;

  D3DCALL(s->device->CreateInputLayout(inputElementDescs, 1, vertex_cso, size_vertex_cso, &s->input_layout));

  D3DCALL(s->device->CreateBlendState(&blend_desc, &s->blend_state));
  D3DCALL(s->device->CreateRasterizerState(&raster_desc, &s->rasterizer_state));
  
  ZeroMemory(&depth_desc, sizeof(D3D11_DEPTH_STENCIL_DESC));
  depth_desc.DepthEnable = true;
  depth_desc.StencilEnable = false;
  depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depth_desc.DepthFunc = D3D11_COMPARISON_LESS;
  depth_desc.StencilReadMask = 0;
  depth_desc.StencilWriteMask = 0;

  D3DCALL(s->device->CreateDepthStencilState(&depth_desc, &s->depth_state));
  
  free(vertex_cso);
  free(pixel_cso);
}

void draw_set_data(struct graphics_data *g, struct d3d11_render_data *render_data)
{
  D3D11_MAPPED_SUBRESOURCE mappedResourceTex;
  D3D11_MAPPED_SUBRESOURCE mappedResourceCB;
  char_element* layer_data_tex;
  d3d11_const_layer* const_layer;
  char_element single_element;
  int last_layer;
  D3D11_BOX copyRegion;
  
  render_data->layer_count = 0;
  
  D3DCALL(render_data->context->Map(
   render_data->layer_data_staging, 0, D3D11_MAP_WRITE, 0, &mappedResourceTex));
  layer_data_tex = (char_element*)mappedResourceTex.pData;
  
  D3DCALL(render_data->context->Map(
   render_data->layers_constant_buffer_staging, 0, D3D11_MAP_WRITE, 0, &mappedResourceCB));
  const_layer = (d3d11_const_layer*)mappedResourceCB.pData;
  
  if(render_data->render_mouse)
  {
    last_layer = g->layer_count - 1;
    while(last_layer > 0 && (
     (int)g->sorted_video_layers[last_layer]->x / 8 > render_data->render_mouse_x / 8 ||
     (int)g->sorted_video_layers[last_layer]->y / 14 > render_data->render_mouse_y / 14 ||
     ((int)g->sorted_video_layers[last_layer]->x / 8 + (int)g->sorted_video_layers[last_layer]->w) <= 
      render_data->render_mouse_x / 8 ||
     ((int)g->sorted_video_layers[last_layer]->y / 14 + (int)g->sorted_video_layers[last_layer]->h) <= 
      render_data->render_mouse_y / 14))
    {
      last_layer -= 1;
    }
    single_element = g->sorted_video_layers[last_layer]->data[
     (render_data->render_mouse_y / 14 - g->sorted_video_layers[last_layer]->y / 14) * 80 +
     render_data->render_mouse_x / 8 - g->sorted_video_layers[last_layer]->x / 8];
    
    single_element.bg_color = (int)((single_element.bg_color & 0xF0) | (0xF - (single_element.bg_color & 0xF)));
    single_element.fg_color = (int)((single_element.fg_color & 0xF0) | (0xF - (single_element.fg_color & 0xF)));
    
    layer_data_tex[render_data->layer_count * LAYER_DATA_TEXTURE_WIDTH] = single_element;
    
    const_layer[render_data->layer_count].x = render_data->render_mouse_x;
    const_layer[render_data->layer_count].y = render_data->render_mouse_y;
    const_layer[render_data->layer_count].w = 1;
    const_layer[render_data->layer_count].h = 1;
    const_layer[render_data->layer_count].z = 0;
    const_layer[render_data->layer_count].char_offset = 0;
    const_layer[render_data->layer_count].transparent_color_10bit = -1;
    const_layer[render_data->layer_count].use_smzx_6bit = (g->screen_mode << 2) | 1;
    
    render_data->layer_count += 1;
  }
  render_data->render_mouse = false;
  
  if(render_data->render_cursor)
  {
    single_element.bg_color = 0;
    single_element.fg_color = render_data->render_cursor_color;
    single_element.char_value = '_';
    
    for(int i = 0; i < render_data->render_cursor_h; i++)
    {
      layer_data_tex[render_data->layer_count * LAYER_DATA_TEXTURE_WIDTH] = single_element;
      
      const_layer[render_data->layer_count].x = render_data->render_cursor_x;
      const_layer[render_data->layer_count].y = render_data->render_cursor_y + i;
      const_layer[render_data->layer_count].w = 1;
      const_layer[render_data->layer_count].h = 1;
      const_layer[render_data->layer_count].z = 0;
      const_layer[render_data->layer_count].char_offset = 0;
      const_layer[render_data->layer_count].transparent_color_10bit = 0;
      const_layer[render_data->layer_count].use_smzx_6bit = g->screen_mode << 2;
      
      render_data->layer_count += 1;
    }
  }
  render_data->render_cursor = false;
  
  for(int i = g->layer_count - 1; i >= 0; i--)
  {
    memcpy(&layer_data_tex[render_data->layer_count * LAYER_DATA_TEXTURE_WIDTH],
      g->sorted_video_layers[i]->data, 
      g->sorted_video_layers[i]->w * g->sorted_video_layers[i]->h * sizeof(char_element));
    
    const_layer[render_data->layer_count].x = g->sorted_video_layers[i]->x;
    const_layer[render_data->layer_count].y = g->sorted_video_layers[i]->y;
    const_layer[render_data->layer_count].w = g->sorted_video_layers[i]->w;
    const_layer[render_data->layer_count].h = g->sorted_video_layers[i]->h;
    const_layer[render_data->layer_count].z = 1;
    const_layer[render_data->layer_count].char_offset = g->sorted_video_layers[i]->offset;
    const_layer[render_data->layer_count].transparent_color_10bit = g->sorted_video_layers[i]->transparent_col;
    const_layer[render_data->layer_count].use_smzx_6bit = g->sorted_video_layers[i]->mode << 2;
    if(g->sorted_video_layers[i]->transparent_col == -1)
    {
      const_layer[render_data->layer_count].use_smzx_6bit |= 1;
    }
    
    render_data->layer_count += 1;
  }
  
  render_data->context->Unmap(render_data->layer_data_staging, 0);
  render_data->context->Unmap(render_data->layers_constant_buffer_staging, 0);
  
  copyRegion.left = 0;
  copyRegion.right = 81*26;
  copyRegion.top = 0;
  copyRegion.bottom = render_data->layer_count;
  copyRegion.back = 1;
  copyRegion.front = 0;
  
  render_data->context->CopySubresourceRegion(
   render_data->layer_data_hw, 0, 0, 0, 0, render_data->layer_data_staging, 0, &copyRegion);
  
  copyRegion.right = sizeof(d3d11_const_layer) * render_data->layer_count;
  copyRegion.bottom = 1;
  
  render_data->context->CopySubresourceRegion(
   render_data->layers_constant_buffer_hw, 0, 0, 0, 0, render_data->layers_constant_buffer_staging, 0, &copyRegion);
  
  if(render_data->remap_tileset_start < render_data->remap_tileset_end)
  {
    D3DCALL(render_data->context->Map(render_data->tileset_staging, 0, D3D11_MAP_WRITE, 0, &mappedResourceTex));
    
    render_data->remap_tileset_start = render_data->remap_tileset_start & (~63);
    render_data->remap_tileset_end = render_data->remap_tileset_end | 63;
    memcpy(
     (UINT8*)mappedResourceTex.pData + 14*render_data->remap_tileset_start, 
     g->charset + 14*render_data->remap_tileset_start,
     sizeof(UINT8)*14*(render_data->remap_tileset_end - render_data->remap_tileset_start));
    render_data->context->Unmap(render_data->tileset_staging, 0);
  }
  
  if(render_data->remap_palette_start < render_data->remap_palette_end)
  {
    D3DCALL(render_data->context->Map(render_data->palette_staging, 0, D3D11_MAP_WRITE, 0, &mappedResourceTex));
    if(g->screen_mode)
    {
      render_data->remap_smzx = true;
      memcpy(
       (UINT32*)mappedResourceTex.pData, 
       g->flat_intensity_palette,
       sizeof(UINT32)*(FULL_PAL_SIZE));
      memcpy(
       (UINT32*)mappedResourceTex.pData + FULL_PAL_SIZE + 16, 
       g->smzx_indices,
       sizeof(UINT32)*(SMZX_PAL_SIZE));
    }
    else
    {
      render_data->remap_smzx = false;
      memcpy(
       (UINT32*)mappedResourceTex.pData + render_data->remap_palette_start, 
       g->flat_intensity_palette + render_data->remap_palette_start,
       sizeof(UINT32)*render_data->remap_palette_end - render_data->remap_palette_start);
      memcpy(
       (UINT32*)mappedResourceTex.pData + SMZX_PAL_SIZE, 
       g->protected_palette,
       sizeof(UINT32)*(PAL_SIZE));
    }
    render_data->context->Unmap(render_data->palette_staging, 0);
  }
}

void draw_no_game_data(struct d3d11_render_data *render_data)
{
  D3D11_BOX copyRegion;
  
  if(render_data->remap_tileset_start < render_data->remap_tileset_end)
  {
      copyRegion.left = 0;
      copyRegion.right = 64 * 14;
      copyRegion.top = render_data->remap_tileset_start / 64;
      copyRegion.bottom = render_data->remap_tileset_end / 64;
      copyRegion.back = 1;
      copyRegion.front = 0;
      render_data->context->CopySubresourceRegion(
       render_data->tileset_hw, 0, 0, render_data->remap_tileset_start / 64, 0, render_data->tileset_staging, 0, &copyRegion);
      
      render_data->remap_tileset_start = FULL_CHARSET_SIZE;
      render_data->remap_tileset_end = 0;
    }
    
    if(render_data->remap_palette_start < render_data->remap_palette_end)
    {
      if(render_data->remap_smzx)
      {
        render_data->context->CopySubresourceRegion(
         render_data->palette_hw, 0, 0, 0, 0, render_data->palette_staging, 0, NULL);
      }
      else
      {
        copyRegion.left = 0;
        copyRegion.right = FULL_PAL_SIZE;
        copyRegion.top = 0;
        copyRegion.bottom = 1;
        copyRegion.back = 1;
        copyRegion.front = 0;
        render_data->context->CopySubresourceRegion(render_data->palette_hw, 0, 0, 0, 0, render_data->palette_staging, 0, &copyRegion);
      }
      
      render_data->remap_palette_start = FULL_PAL_SIZE;
      render_data->remap_palette_end = 0;
    }
    
    render_data->context->RSSetViewports(1, &render_data->viewport);
    render_data->context->RSSetScissorRects(1, &render_data->rect);
    
    render_data->context->OMSetRenderTargets(1, &render_data->rtv, render_data->dsv);
    
    float clearColor[] =
    { 
      (render_data->clear_color & 0xFF) / 255.0f, 
      (render_data->clear_color & 0xFF00)/(255.0f*256.0f), 
      (render_data->clear_color & 0xFF0000)/(255.0f*256.0f*256.0f),
      1.0f
    };
    render_data->context->ClearRenderTargetView(render_data->rtv, clearColor);
    render_data->context->ClearDepthStencilView(render_data->dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
    
    render_data->context->VSSetConstantBuffers(0, 1, &render_data->layers_constant_buffer_hw);
    
    ID3D11ShaderResourceView* views[3];
    views[0] = render_data->tileset_srv;
    views[1] = render_data->layer_data_srv;
    views[2] = render_data->palette_srv;
    render_data->context->PSSetShaderResources(0, 3, views);
    
    UINT stride = 64;
    UINT offset = 0;
  
    render_data->context->IASetIndexBuffer(NULL, DXGI_FORMAT_R16_UINT, 0);
    render_data->context->IASetVertexBuffers(0, 0, NULL, &stride, &offset);
    render_data->context->IASetInputLayout(render_data->input_layout);
    render_data->context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    render_data->context->RSSetState(render_data->rasterizer_state);
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    UINT sampleMask = 0xffffffff;
    render_data->context->OMSetBlendState(render_data->blend_state, blendFactor, sampleMask);
    render_data->context->OMSetDepthStencilState(render_data->depth_state, 1);
    
    render_data->context->VSSetShader(render_data->vertex_shader, NULL, 0);
    render_data->context->PSSetShader(render_data->pixel_shader, NULL, 0);
    
    render_data->context->Draw(render_data->layer_count * 6, 0);
    
    D3DCALL(render_data->swap_chain->Present(render_data->vsync ? 1 : 0, 0));
}

#ifdef D3D11_THREADED
static void *d3d11_draw_and_sync_threaded(void* data)
{
  struct graphics_data *g = (graphics_data *)data;
  struct d3d11_render_data *render_data = (d3d11_render_data *)g->render_data;
  
  while(true)
  {
    while(render_data->game_frame == render_data->render_frame)
    {
      SDL_Delay(0);
      if(render_data->stop_thread)
      {
        return 0;
      }
    }
    platform_mutex_lock(&render_data->game_data_mutex);
    platform_mutex_lock(&render_data->context_mutex);
    
    draw_set_data(g, render_data);
    
    render_data->render_frame += 1;
    platform_mutex_unlock(&render_data->game_data_mutex);
    //Don't access graphics from this point on, d3d11_render_data is OK
    
    draw_no_game_data(render_data);
    render_data->layer_count = 0;
    
    platform_mutex_unlock(&render_data->context_mutex);
  }
  return 0;
}
#endif

static bool d3d11_init_video(struct graphics_data *g,
 struct config_info *conf)
{
  struct d3d11_render_data *render_data = (d3d11_render_data *) cmalloc(sizeof(struct d3d11_render_data));
  if(!render_data)
    return false;
  memset(render_data, 0, sizeof(struct d3d11_render_data));
  
  g->allow_resize = conf->allow_resize;
  g->ratio = conf->video_ratio;
  g->bits_per_pixel = 32;
  g->render_data = render_data;
  
  render_data->game_frame = 0;
  render_data->render_frame = 0;
  render_data->vsync = conf->gl_vsync;
  render_data->screen_ratio = g->ratio;
  
  d3d11_init_screen_create_window(render_data, conf);
  d3d11_init_screen_create_device(render_data, conf);
  d3d11_init_screen_create_backbuffer(render_data);
  d3d11_init_screen_create_palette(render_data);
  d3d11_init_screen_create_tileset(render_data);
  d3d11_init_screen_create_layer_data(render_data);
  d3d11_init_screen_create_shader(render_data);
  
  render_data->layer_count = 0;
  
  render_data->remap_tileset_start = 0;
  render_data->remap_tileset_end = FULL_CHARSET_SIZE;
  
  render_data->remap_palette_start = 0;
  render_data->remap_palette_end = SMZX_PAL_SIZE + 1;
  
  render_data->render_mouse = false;
  render_data->render_cursor = false;
  
#ifdef D3D11_THREADED
  platform_mutex_init(&render_data->game_data_mutex);
  platform_mutex_init(&render_data->context_mutex);
  platform_mutex_lock(&render_data->game_data_mutex);
  render_data->stop_thread = false;
  platform_thread_create(&render_data->render_thread, (platform_thread_fn)d3d11_draw_and_sync_threaded, g);
#endif
  return true;
}

static void d3d11_free_video(struct graphics_data *g)
{
  struct d3d11_render_data *s = (d3d11_render_data *)g->render_data;
#ifdef D3D11_THREADED
  s->stop_thread = true;
  platform_mutex_lock(&s->context_mutex);
#endif
  s->device->Release();
#ifdef D3D11_THREADED
  platform_mutex_unlock(&s->context_mutex);
  platform_thread_join(&s->render_thread);
#endif  
  free(g->render_data);
  g->render_data = NULL;
}

static void d3d11_remap_charsets(struct graphics_data *g)
{
  struct d3d11_render_data *s = (d3d11_render_data *)g->render_data;
  s->remap_tileset_start = 0;
  s->remap_tileset_end = FULL_CHARSET_SIZE;
}

static void d3d11_resize_screen(struct graphics_data *g,
 int width, int height)
{
  struct d3d11_render_data *render_data = (d3d11_render_data *)g->render_data;
  render_data->render_target->Release();
  render_data->rtv->Release();
  render_data->depth_buffer->Release();
  D3DCALL(render_data->swap_chain->ResizeBuffers(2, width, height, DXGI_FORMAT_UNKNOWN, 0));
  render_data->screen_height = height;
  render_data->screen_width = width;
  d3d11_init_screen_create_backbuffer(render_data);
}

static bool d3d11_set_video_mode(struct graphics_data *g,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  struct d3d11_render_data *render_data = (d3d11_render_data *)g->render_data;
  D3DCALL(render_data->swap_chain->SetFullscreenState(fullscreen, NULL));
  d3d11_resize_screen(g, width, height);
  return true;
}

static void d3d11_remap_char(struct graphics_data *g, Uint16 chr)
{
  struct d3d11_render_data *s = (d3d11_render_data *)g->render_data;
  if(s->remap_tileset_start > chr)
  {
    s->remap_tileset_start = chr;
  }
  if(s->remap_tileset_end < chr + 1)
  {
    s->remap_tileset_end = chr + 1;
  }
}

static void d3d11_remap_charbyte(struct graphics_data *g,
 Uint16 chr, Uint8 byte)
{
  d3d11_remap_char(g, chr);
}

static void d3d11_update_colors(struct graphics_data *g,
 struct rgb_color *palette, Uint32 count)
{
  struct d3d11_render_data *s = (d3d11_render_data *)g->render_data;
  s->clear_color = *((UINT32*)palette);
  memcpy(g->flat_intensity_palette, palette, count * sizeof(rgb_color));
  
  s->remap_palette_start = 0;
  if(s->remap_palette_end < (int)count)
  {
    s->remap_palette_end = (int)count;
  }
}

static void d3d11_render_layer(struct graphics_data *g, struct video_layer *layer)
{
  return;
}

static void d3d11_render_cursor(struct graphics_data *g,
 Uint32 x, Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
{
  struct d3d11_render_data *render_data = (struct d3d11_render_data *)g->render_data;
  render_data->render_cursor = true;
  render_data->render_cursor_x = x * 8;
  render_data->render_cursor_y = y * 14 - 10 + offset;
  render_data->render_cursor_color = color;
  render_data->render_cursor_h = lines;
}

static void d3d11_render_mouse(struct graphics_data *g,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct d3d11_render_data *render_data = (struct d3d11_render_data *)g->render_data;
  render_data->render_mouse = true;
  render_data->render_mouse_x = x;
  render_data->render_mouse_y = y;
  return;
}

#ifdef D3D11_THREADED
static void d3d11_sync_screen_async(struct graphics_data *g)
{
  struct d3d11_render_data *render_data = (struct d3d11_render_data *) g->render_data;
  render_data->game_frame += 1;
  platform_mutex_unlock(&render_data->game_data_mutex);
  
  while(render_data->game_frame != render_data->render_frame)
  {
    SDL_Delay(0);
  }
  platform_mutex_lock(&render_data->game_data_mutex);
}
#else

static void d3d11_sync_screen(struct graphics_data *g)
{
  struct d3d11_render_data *render_data = (struct d3d11_render_data *)g->render_data;
  
  draw_set_data(g, render_data);
  draw_no_game_data(render_data);
  render_data->layer_count = 0;
}
#endif

void render_d3d11_register(struct renderer *r)
{
  memset(r, 0, sizeof(struct renderer));
  r->init_video = d3d11_init_video;
  r->free_video = d3d11_free_video;
  r->check_video_mode = gl_check_video_mode;
  r->set_video_mode = d3d11_set_video_mode;
  r->update_colors = d3d11_update_colors;
  r->resize_screen = d3d11_resize_screen;
  r->remap_charsets = d3d11_remap_charsets;
  r->remap_char = d3d11_remap_char;
  r->remap_charbyte = d3d11_remap_charbyte;
  r->get_screen_coords = get_screen_coords_scaled;
  r->set_screen_coords = set_screen_coords_scaled;
  r->render_layer = d3d11_render_layer;
  r->render_cursor = d3d11_render_cursor;
  r->render_mouse = d3d11_render_mouse;
#ifdef D3D11_THREADED
  r->sync_screen = d3d11_sync_screen_async;
#else
  r->sync_screen = d3d11_sync_screen;
#endif
}
