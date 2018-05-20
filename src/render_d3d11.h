/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007,2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#ifndef __RENDER_D3D11_H
#define __RENDER_D3D11_H

#include "compat.h"

__M_BEGIN_DECLS

#include "graphics.h"
#include "util.h"

#include "d3d11.h"
#include "dxgi.h"

#define D3D11_THREADED

#ifdef D3D11_THREADED
#include <pthread.h>
#endif

struct d3d11_const_layer
{
  INT32 x;
  INT32 y;
  
  UINT8 w;
  UINT8 h;
  UINT16 z;
  
  INT16 char_offset;
  UINT8 transparent_color_10bit;
  UINT8 use_smzx_6bit;
};

struct d3d11_render_data
{
  int screen_width;
  int screen_height;
  bool vsync;
  enum ratio_type screen_ratio;
  HWND window_handle;
  IDXGISwapChain *swap_chain;
  ID3D11Device *device;
  ID3D11DeviceContext *context;
  ID3D11Resource *render_target;
  ID3D11RenderTargetView *rtv;
  
  ID3D11DepthStencilState *depth_state;
  ID3D11Texture2D *depth_buffer;
  ID3D11DepthStencilView *dsv;
  
  D3D11_VIEWPORT viewport;
  D3D11_RECT rect;
  
  ID3D11Texture2D *palette_hw;
  ID3D11Texture2D *palette_staging;
  ID3D11ShaderResourceView *palette_srv;
  
  ID3D11Texture2D *tileset_hw;
  ID3D11Texture2D *tileset_staging;
  ID3D11ShaderResourceView *tileset_srv;
  
  ID3D11Texture2D *layer_data_hw;
  ID3D11Texture2D *layer_data_staging;
  ID3D11ShaderResourceView *layer_data_srv;
  
  ID3D11VertexShader *vertex_shader;
  ID3D11PixelShader *pixel_shader;
  ID3D11InputLayout *input_layout;
  ID3D11BlendState *blend_state;
  ID3D11RasterizerState *rasterizer_state;
  
  ID3D11Buffer *layers_constant_buffer_staging;
  ID3D11Buffer *layers_constant_buffer_hw;
  
#ifdef D3D11_THREADED
  pthread_t render_thread;
  pthread_mutex_t game_data_mutex;
  pthread_mutex_t context_mutex;
#endif
  
  int game_frame;
  int render_frame;
  
  int remap_tileset_start;
  int remap_tileset_end;
  
  int remap_palette_start;
  int remap_palette_end;
  bool remap_smzx;
  
  UINT32 clear_color;
  
  int layer_count;
  
  bool render_mouse;
  int render_mouse_x;
  int render_mouse_y;
  
  bool render_cursor;
  int render_cursor_x;
  int render_cursor_y;
  int render_cursor_h;
  int render_cursor_color;
};

LRESULT CALLBACK wndproc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam);
void d3d11_init_screen_register_class(void);
void d3d11_init_screen_create_window(struct d3d11_render_data *s);
void d3d11_init_screen_create_device(struct d3d11_render_data *s);
void d3d11_init_screen_create_backbuffer(struct d3d11_render_data *s);
void d3d11_init_screen_create_palette(struct d3d11_render_data *s);
void d3d11_init_screen_create_tileset(struct d3d11_render_data *s);
void d3d11_init_screen_create_layer_data(struct d3d11_render_data *s);
void d3d11_init_screen_create_shader(struct d3d11_render_data *s);

void* d3d11_draw_and_sync_threaded(void* data);
void d3d11_get_messages(void);


__M_END_DECLS

#endif // __RENDER_D3D11_H
