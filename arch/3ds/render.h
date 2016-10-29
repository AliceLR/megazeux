#ifndef __3DS_RENDER_H__
#define __3DS_RENDER_H__

#include <stdlib.h>
#include <3ds.h>
#include <citro3d.h>

struct ctr_shader_data
{
  DVLB_s* dvlb;
  shaderProgram_s program;
  int proj_loc;
  C3D_AttrInfo attr;
};

struct ctr_render_data
{
  u32 *buffer;
  u8 *chartex_buffer;
  struct v_char *map;
  struct ctr_shader_data shader, shader_accel;
  C3D_Mtx projection;
  C3D_Tex texture, chartex;
  u8 chartex_dirty, cursor_on, mouse_on;
  C3D_RenderTarget *target, *target_bottom;
  u32 focus_x, focus_y;
};

typedef struct {
  float u, v;
} vector_2f;

typedef struct {
  float x, y, z;
} vector_3f;

struct v_char
{
  float x, y, z;
  u8 u, v;
  u16 dud;
  u32 col;
};

struct vertex
{
  vector_3f position;
  vector_2f texcoord;
};

C3D_Tex* ctr_load_png(const char *name);

void ctr_init_shader(struct ctr_shader_data *shader, const void* data, int size);
void ctr_bind_shader(struct ctr_shader_data *shader);

void* ctr_draw_2d_texture(struct ctr_render_data *render_data, const C3D_Tex* texture,
  int tx, int ty, int tw, int th,
  float x, float y, float w, float h, float z);

#endif /* __3DS_RENDER_H__ */
