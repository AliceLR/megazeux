/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

/* Palette editor */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../helpsys.h"
#include "../window.h"
#include "../data.h"
#include "../graphics.h"
#include "../event.h"

#include "pal_ed.h"


//----------------------------------/ /-----------------------------------------/
// ##..##..##..##..##..##..##..##.. / / Color # 00-             RGB  HSL  Lab   /
// ##..##..##..##..##..#1.1#1.1#1.1 / / R    0 [----|----|----|----|----|----|] /
// #0.1#2.3#4.5#6.7#8.9#0.1#2.3#4.5 / / G    0 [----|----|----|----|----|----|] /
// ##..##..##..##..##..##..##..##.. / / B    0 [----|----|----|----|----|----|] /
//-^^-------------------------------/ /-----------------------------------------/

//----------------------------------------------------------------/
//
// %- Select color    Alt+D- Default pal.   PgDn- Prev. mode
// R- Increase Red    Alt+R- Decrease Red   PgUp- Next mode
// G- Increase Green  Alt+G- Decrease Green   F2- Store color
// B- Increase Blue   Alt+B- Decrease Blue    F3- Retrieve color
// A- Increase All    Alt+A- Decrease All
// 0- Blacken color   Alt+H- Hide help         Q- Quit editing
//
//----------------------------------------------------------------/

// Note: the help menu mouse functionality has been broken since 2.80.

#define PAL_ED_COL_X1         36
#define PAL_ED_COL_X2         79
#define PAL_ED_COL_Y1         0
#define PAL_ED_COL_Y2         5

#define PAL_ED_16_X1          0
#define PAL_ED_16_X2          35
#define PAL_ED_16_Y1          0
#define PAL_ED_16_Y2          5

#define PAL_ED_16_HELP_X1     7
#define PAL_ED_16_HELP_Y1     7
#define PAL_ED_16_HELP_X2     73
#define PAL_ED_16_HELP_Y2     16


struct color_status {
  unsigned char r;
  unsigned char g;
  unsigned char b;

  unsigned int h;
  unsigned int s;
  unsigned int l;

  unsigned int CL;
  int Ca;
  int Cb;
};

bool saved_color_active = false;
static struct color_status saved_color = {
  0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int content_x;
static int content_y;
static int minimal_help = -1;

static unsigned int current_id = 0;
static int current_mode_id = 0;


// -----------------------------------------------------------------------------


static void rgb_to_hsl(struct color_status *current)
{
  float r = (float)(current->r) / 63.0;
  float g = (float)(current->g) / 63.0;
  float b = (float)(current->b) / 63.0;

  float M = MAX(r, MAX(g, b));
  float m = MIN(r, MIN(g, b));
  float c = M - m;

  float h =
   (c == 0) ? 0 :
   (M == r) ? fmod((g - b)/c + 6, 6.0)  : // Add 6 to bypass C's shit modulo
   (M == g) ? ((b - r)/c) + 2           :
   (M == b) ? ((r - g)/c) + 4           : 0;

  float l = (M+m)/2.0;

  current->h = (unsigned int) round( fmod(h * 60, 360.0) );

  current->s = (unsigned int) round( l < 1  ?  c/(1 - fabs(M+m-1)) * 100 : 0 );

  current->l = (unsigned int) round( l * 100 );
}

static void hsl_to_rgb(struct color_status *current)
{
  float l = (float)( current->l ) / 100.0;
  float s = (float)( current->s ) / 100.0;
  float h = (float)( current->h ) / 60.0;

  float c = (1 - fabs(2*l - 1)) * s;

  float m = l - c/2.0;

  float x = c * (1 - fabs( fmod(h, 2.0) - 1 ));

  int fh = floor(h);

  float r =
   (fh == 0) || (fh == 5) ? c+m :
   (fh == 1) || (fh == 4) ? x+m : m;

  float g =
   (fh == 1) || (fh == 2) ? c+m :
   (fh == 0) || (fh == 3) ? x+m : m;

  float b =
   (fh == 3) || (fh == 4) ? c+m :
   (fh == 2) || (fh == 5) ? x+m : m;

  current->r = (unsigned char) round(r * 63.0);
  current->g = (unsigned char) round(g * 63.0);
  current->b = (unsigned char) round(b * 63.0);
}

static const double Xn = 0.9505;
static const double Yn = 1.0000;
static const double Zn = 1.0890;

static inline double srgb_comp_to_linear(double c)
{
  return (c <= 0.04045)  ?  c / 12.92  :  pow((c + 0.055)/(1.055), 2.4);
}

static inline double srgb_linear_to_comp(double l)
{
  return (l <= 0.0031308)  ?  l * 12.92  :  (1.055)*pow(l, 1/2.4) - 0.055;
}

static inline double xyz_to_lab_comp_transform(double c)
{
  return (c > 0.008856)  ?  cbrt(c)  :  (903.3 * c + 16)/116.0;
}

static inline double lab_to_xyz_comp_transform(double c)
{
  return (c > 0.205893)  ?  pow(c, 3.0)  :  0.128414 * (c - 0.137931);
}

static void rgb_to_lab(struct color_status *current)
{
  double r = srgb_comp_to_linear( (double)(current->r) / 63.0 );
  double g = srgb_comp_to_linear( (double)(current->g) / 63.0 );
  double b = srgb_comp_to_linear( (double)(current->b) / 63.0 );

  double x = 0.4124564*r + 0.3575761*g + 0.1804375*b;
  double y = 0.2126729*r + 0.7151522*g + 0.0721750*b;
  double z = 0.0193339*r + 0.1191920*g + 0.9503041*b;

  double fx = xyz_to_lab_comp_transform( x / Xn );
  double fy = xyz_to_lab_comp_transform( y / Yn );
  double fz = xyz_to_lab_comp_transform( z / Zn );

  current->CL = (unsigned int) round( 116 * fy - 16 );
  current->Ca = (int) round( 500 * (fx - fy) );
  current->Cb = (int) round( 200 * (fy - fz) );
}

static void lab_to_rgb(struct color_status *current)
{
  double CL = (double)(current->CL);
  double Ca = current->Ca;
  double Cb = current->Cb;

  double fy = (CL + 16) / 116.0;
  double fx = fy + Ca / 500.0;
  double fz = fy - Cb / 200.0;

  double x = Xn * lab_to_xyz_comp_transform( fx );
  double y = Yn * lab_to_xyz_comp_transform( fy );
  double z = Zn * lab_to_xyz_comp_transform( fz );

  double r =  3.2404542*x + -1.5371385*y + -0.4985314*z;
  double g = -0.9692660*x +  1.8760108*y +  0.0415560*z;
  double b =  0.0556434*x + -0.2040259*y +  1.0572252*z;

  r = srgb_linear_to_comp( r );
  g = srgb_linear_to_comp( g );
  b = srgb_linear_to_comp( b );

  current->r = (unsigned int) round(CLAMP(r, 0.0, 1.0) * 63.0);
  current->g = (unsigned int) round(CLAMP(g, 0.0, 1.0) * 63.0);
  current->b = (unsigned int) round(CLAMP(b, 0.0, 1.0) * 63.0);
}

static void update_color(struct color_status *current, int id)
{
  get_rgb(id, &(current->r), &(current->g), &(current->b));
  rgb_to_hsl(current);
  rgb_to_lab(current);
}

static void set_color_rgb(struct color_status *current)
{
  rgb_to_hsl(current);
  rgb_to_lab(current);
  set_rgb(current_id, current->r, current->g, current->b);
  update_palette();
}


static void set_color_hsl(struct color_status *current)
{
  hsl_to_rgb(current);
  rgb_to_lab(current);
  set_rgb(current_id, current->r, current->g, current->b);
  update_palette();
}


static void set_color_lab(struct color_status *current)
{
  lab_to_rgb(current);
  rgb_to_hsl(current);
  set_rgb(current_id, current->r, current->g, current->b);
  update_palette();
}

static int get_color_rgb(struct color_status *current, int component)
{
  switch(component)
  {
    case 0: return (int)current->r;
    case 1: return (int)current->g;
    case 2: return (int)current->b;
  }
  return -1;
}

static int get_color_hsl(struct color_status *current, int component)
{
  switch(component)
  {
    case 0: return (int)current->h;
    case 1: return (int)current->s;
    case 2: return (int)current->l;
  }
  return -1;
}

static int get_color_lab(struct color_status *current, int component)
{
  switch(component)
  {
    case 0: return (int)current->CL;
    case 1: return (int)current->Ca;
    case 2: return (int)current->Cb;
  }
  return -1;
}

static void set_color_rgb_bar(struct color_status *current, int component,
 int value)
{
  switch(component)
  {
    case 0:
      current->r = value;
      break;

    case 1:
      current->g = value;
      break;

    case 2:
      current->b = value;
      break;
  }
  set_color_rgb(current);
}

static void set_color_hsl_bar(struct color_status *current, int component,
 int value)
{
  switch(component)
  {
    case 0:
      current->h = value;
      break;

    case 1:
      current->s = value;
      break;

    case 2:
      current->l = value;
      break;
  }
  set_color_hsl(current);
}

static void set_color_lab_bar(struct color_status *current, int component,
 int value)
{
  switch(component)
  {
    case 0:
      current->CL = value;
      break;

    case 1:
      current->Ca = value;
      break;

    case 2:
      current->Cb = value;
      break;
  }
  set_color_lab(current);
}

static int key_color_rgb(struct color_status *current, int key)
{
  switch(key)
  {
    case IKEY_r:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->r)
        {
          current->r--;
        }
      }
      else
      {
        if(current->r < 63)
        {
          current->r++;
        }
      }

      set_color_rgb(current);
      return -1;
    }

    case IKEY_g:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->g)
        {
          current->g--;
        }
      }
      else
      {
        if(current->g < 63)
        {
          current->g++;
        }
      }

      set_color_rgb(current);
      return -1;
    }

    case IKEY_b:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->b)
        {
          current->b--;
        }
      }
      else
      {
        if(current->b < 63)
        {
          current->b++;
        }
      }

      set_color_rgb(current);
      return -1;
    }

    case IKEY_a:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->r)
          current->r--;
        if(current->g)
          current->g--;
        if(current->b)
          current->b--;
      }
      else
      {
        if(current->r < 63)
          current->r++;
        if(current->g < 63)
          current->g++;
        if(current->b < 63)
          current->b++;
      }

      set_color_rgb(current);
      return -1;
    }
  }

  return key;
}

static int key_color_hsl(struct color_status *current, int key)
{
  switch(key)
  {
    case IKEY_h:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->h)
        {
          current->h--;
        }
        else
        {
          current->h = 359;
        }
      }
      else
      {
        if(current->h < 359)
        {
          current->h++;
        }
        else
        {
          current->h = 0;
        }
      }

      set_color_hsl(current);
      return -1;
    }

    case IKEY_s:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->s)
        {
          current->s--;
        }
      }
      else
      {
        if(current->s < 100)
        {
          current->s++;
        }
      }

      set_color_hsl(current);
      return -1;
    }

    case IKEY_v:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->l)
        {
          current->l--;
        }
      }
      else
      {
        if(current->l < 100)
        {
          current->l++;
        }
      }

      set_color_hsl(current);
      return -1;
    }
  }

  return key;
}

static int key_color_lab(struct color_status *current, int key)
{
  switch(key)
  {
    case IKEY_a:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->Ca > -128)
        {
          current->Ca--;
        }
      }
      else
      {
        if(current->Ca < 128)
        {
          current->Ca++;
        }
      }

      set_color_lab(current);
      return -1;
    }

    case IKEY_b:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->Cb > -128)
        {
          current->Cb--;
        }
      }
      else
      {
        if(current->Cb < 128)
        {
          current->Cb++;
        }
      }

      set_color_lab(current);
      return -1;
    }

    case IKEY_v:
    {
      if(get_alt_status(keycode_internal))
      {
        if(current->CL)
        {
          current->CL--;
        }
      }
      else
      {
        if(current->CL < 100)
        {
          current->CL++;
        }
      }

      set_color_lab(current);
      return -1;
    }
  }

  return key;
}

struct color_mode_component {
  const char *name_long;
  const char *name_short;
  const char *key;
  char left_col;
  char right_col;
  int min_val;
  int max_val;
};

struct color_mode {
  const char *name;
  struct color_mode_component components[3];
  void (*set_function)(struct color_status *);
  int (*get_function)(struct color_status *, int);
  int (*input_function)(struct color_status *, int);
  void (*input_bar_function)(struct color_status *, int, int);
  bool allow_all;
};

static const struct color_mode mode_list[] =
{
  { "RGB",
    {
      { "Red",   "Red ", "R", 12,  0,    0,  63 },
      { "Green", "Grn ", "G", 10,  0,    0,  63 },
      { "Blue",  "Blu ", "B",  9,  0,    0,  63 },
    },
    set_color_rgb,
    get_color_rgb,
    key_color_rgb,
    set_color_rgb_bar,
    true,
  },

  { "HSL",
    {
      { "Hue",   "Hue",  "H",  4,  4,    0, 359 },
      { "Sat.",  "Sat",  "S",  7,  0,    0, 100 },
      { "Light", "Lgt",  "V", 15,  0,    0, 100 },
    },
    set_color_hsl,
    get_color_hsl,
    key_color_hsl,
    set_color_hsl_bar,
    false,
  },

  { "CIELAB",
    {
      { "L*",    "L*",   "V", 15,  0,    0, 100 },
      { "a*",    "a*",   "A",  4,  2, -128, 128 },
      { "b*",    "b*",   "B",  6,  1, -128, 128 },
    },
    set_color_lab,
    get_color_lab,
    key_color_lab,
    set_color_lab_bar,
    false,
  },
};


// -----------------------------------------------------------------------------


static char hue_chars[32] = {
  0xDB, 0xB0, 0xB0, 0xB1, 0xB2,  0xDB, 0xDB, 0xB0, 0xB1, 0xB2,
  0xDB, 0xDB, 0xB0, 0xB1, 0xB2,  0xDB, 0xB0, 0xB1, 0xB1, 0xB2,
  0xB2, 0xDB, 0xB0, 0xB1, 0xB2,  0xDB, 0xDB, 0xB0, 0xB1, 0xB2,
  0xDB, 0xDB
};

static char hue_colors[32] = {
  0xCC, 0xC6, 0xCE, 0xCE, 0xCE,  0xEE, 0xEE, 0xEA, 0xEA, 0xEA,
  0xAA, 0xAA, 0xAB, 0xAB, 0xAB,  0xBB, 0xB3, 0xB3, 0xB9, 0xB9,
  0x39, 0x99, 0x9D, 0x9D, 0x9D,  0xDD, 0xDD, 0xDC, 0xDC, 0xDC,
  0xCC, 0xCC
};

static void draw_hue_bar(int value, unsigned int x, unsigned int y,
 int min_val, int max_val)
{
  int i;

  value = ((value - min_val)*31 + (max_val - min_val)/2) / (max_val - min_val);

  for(i = 0; i < 32; i++)
  {
    draw_char(hue_chars[i], hue_colors[i], x+i, y);
  }

  draw_char(0xDB, 0xFF, x+value, y);
}

static void draw_color_bar(int value, unsigned int x, unsigned int y,
 int min_val, int max_val, char left_col, char right_col)
{
  char bg_col;
  char fg_col;
  int i;
  int j;

  value = ((value - min_val)*63 + (max_val - min_val)/2) / (max_val - min_val);

  for(i = 0, j = 0; i < 32; i++)
  {
    bg_col =
     (value > j) ? left_col  :
     (value < j) ? right_col : 15;
    j++;

    fg_col =
     (value > j) ? left_col  :
     (value < j) ? right_col : 15;
    j++;

    draw_char('\xDE', (bg_col << 4) + fg_col, x+i, y);
  }
}

static void palette_editor_redraw_color_window(struct color_status *current,
 const struct color_mode *mode)
{
  char color;
  int x;
  int i;

  draw_window_box(PAL_ED_COL_X1, PAL_ED_COL_Y1, PAL_ED_COL_X2, PAL_ED_COL_Y2,
   DI_GREY, DI_GREY_DARK, DI_GREY_CORNER, 1, 1);

  // Write Color #
  write_string(
   "Color #",
   PAL_ED_COL_X1 + 2,
   PAL_ED_COL_Y1 + 1,
   DI_GREY_TEXT,
   0
  );

  // Modes
  x = 42;
  for(i = ARRAY_SIZE(mode_list) - 1; i >= 0; i--)
  {
    color = (i == current_mode_id) ? DI_GREY : DI_GREY_DARK;

    x -= strlen( mode_list[i].name ) + 2;
    write_string(
     mode_list[i].name,
     PAL_ED_COL_X1 + x,
     PAL_ED_COL_Y1 + 1,
     color,
     0
    );
  }
}

static void palette_editor_update_color_window(struct color_status *current,
 const struct color_mode *mode)
{
  const struct color_mode_component *c;
  char buffer[5];
  int value;
  int i;

  // Color #
  write_number(current_id, DI_GREY_TEXT,
   PAL_ED_COL_X1 + 12, PAL_ED_COL_Y1 + 1, 3, 1, 10);

  for(i = 0; i < 3; i++)
  {
    value = mode->get_function(current, i);
    c = &(mode->components[i]);

    sprintf(buffer, "%4d", value);

    write_string(
     buffer,
     PAL_ED_COL_X1 + 5,
     PAL_ED_COL_Y1 + 2 + i,
     DI_GREY_TEXT,
     0
    );

    write_string(
     c->name_short,
     PAL_ED_COL_X1 + 2,
     PAL_ED_COL_Y1 + 2 + i,
     DI_GREY_TEXT,
     0
    );

    if((current_mode_id == 1) && (i == 0))
    {
      // Special case - the hue bar
      draw_hue_bar(value, PAL_ED_COL_X1 + 10, PAL_ED_COL_Y1 + 2 + i,
       c->min_val, c->max_val);
    }

    else
    {
      // Otherwise, draw a regular bar
      draw_color_bar(value, PAL_ED_COL_X1 + 10, PAL_ED_COL_Y1 + 2 + i,
       c->min_val, c->max_val, c->left_col, c->right_col);
    }
  }
}

static int palette_editor_input_color_window(struct color_status *current,
 const struct color_mode *mode, int key)
{
  if(get_mouse_press())
  {
    int mouse_x;
    int mouse_y;
    int mouse_px;
    int mouse_py;

    get_mouse_position(&mouse_x, &mouse_y);
    get_real_mouse_position(&mouse_px, &mouse_py);

    // Component bars

    // -1 and +1 to allow going past the actual bounds slightly to make setting
    // to the minimum and maximum values easier.

    if(
     (mouse_x >= PAL_ED_COL_X1 + 10 - 1) &&
     (mouse_x <  PAL_ED_COL_X1 + 10 + 32 + 1) &&
     (mouse_y >= PAL_ED_COL_Y1 + 2) &&
     (mouse_y <  PAL_ED_COL_Y1 + 5))
    {
      const struct color_mode_component *c;
      int component = mouse_y - PAL_ED_COL_Y1 - 2;

      float value =
       CLAMP((mouse_x - PAL_ED_COL_X1 - 10) + ((mouse_px % 8) / 8.0), 0, 32);

      c = &(mode->components[component]);

      value = (value * (c->max_val - c->min_val) + 16) / 32.0 + c->min_val;

      mode->input_bar_function(current, component, (int)value);

      // FIXME warp_real_mouse_y(), which is intended for setting MOUSEPY, will
      // actually warp the real X to the nearest MZX pixel position as well.
      // This causes major issues with upscaled and non-integer scaling ratios.

      // Snap the mouse to the center of the bar.
      warp_real_mouse_y(mouse_y * 14 + 7);
      return -1;
    }
    else

    // Mode select

    if(0)
    {
      // FIXME implement this if there's demand for it. It's close to the slider
      // bars and may be easy to accidentally click, which might be unpleasant.
      //return -2;
    }
  }

  key = mode->input_function(current, key);
  return key;
}


// -----------------------------------------------------------------------------


static void palette_editor_redraw_window_16(struct color_status *current,
 const struct color_mode *mode)
{
  const struct color_mode_component *c;
  int i;
  int x;
  int y;

  content_x = PAL_ED_16_X1 + 2;
  content_y = PAL_ED_16_Y1 + 1;

  // Draw window
  draw_window_box(PAL_ED_16_X1, PAL_ED_16_Y1, PAL_ED_16_X2, PAL_ED_16_Y2,
   DI_GREY_DARK, DI_GREY, DI_GREY_CORNER, 1, 1);

  // Erase the spot where the palette will go and the line below it.
  for(i = 0; i < 32*4; i++)
    erase_char(i % 32 + content_x, i / 32 + content_y);

  if(!minimal_help)
  {
    draw_window_box(PAL_ED_16_HELP_X1, PAL_ED_16_HELP_Y1, PAL_ED_16_HELP_X2,
     PAL_ED_16_HELP_Y2, DI_GREY_DARK, DI_GREY, DI_GREY_CORNER, 1, 1);

    // Write menu
    write_string
    (
      "\x1d- Select color    Alt+D- Default pal.   PgUp- Prev. mode\n"
      " - Increase        Alt+ - Decrease       PgDn- Next mode\n"
      " - Increase        Alt+ - Decrease         F2- Store color\n"
      " - Increase        Alt+ - Decrease         F3- Retrieve color\n"
      "\n"
      "0- Blacken color   Alt+H- Hide help         Q- Quit editing\n",
      PAL_ED_16_HELP_X1 + 3,
      PAL_ED_16_HELP_Y1 + 2,
      DI_GREY_TEXT,
      1
    );

    // Component instructions
    y = PAL_ED_16_HELP_Y1 + 3;
    for(i = 0; i < 3; i++, y++)
    {
      c = &(mode->components[i]);
      x = PAL_ED_16_HELP_X1 + 3;

      write_string(c->key, x, y, DI_GREY_TEXT, 1);
      x += 12;

      write_string(c->name_long, x, y, DI_GREY_TEXT, 1);
      x += 11;

      write_string(c->key, x, y, DI_GREY_TEXT, 1);
      x += 12;

      write_string(c->name_long, x, y, DI_GREY_TEXT, 1);
    }

    // All
    if(mode->allow_all)
    {
      write_string
      (
       "A- Increase All    Alt+A- Decrease All",
       PAL_ED_16_HELP_X1 + 3,
       PAL_ED_16_HELP_Y1 + 6,
       DI_GREY_TEXT,
       1
      );
    }
  }
}

static void palette_editor_update_window_16(struct color_status *current,
 const struct color_mode *mode)
{
  int screen_mode = get_screen_mode();
  unsigned int bg_color;
  unsigned int fg_color;
  unsigned int chr;
  unsigned char r;
  unsigned char g;
  unsigned char b;
  int i;
  int x;
  int y;

  // Draw palette bars
  for(bg_color = 0; bg_color < 16; bg_color++)
  {
    x = bg_color * 2 + content_x;
    y = content_y;

    get_rgb(bg_color, &r, &g, &b);

    // The foreground color is white or black in the protected palette.

    fg_color = ((r * .3) + (g * .59) + (b * .11) < 32) ? 31 : 16;

    // FIXME: in SMZX modes 2 and 3, fg_color alters the color here
    // eventually, they should get their own palette editor.
    if(screen_mode >= 2)
      fg_color = 0;

    select_layer(OVERLAY_LAYER);

    // Draw the palette colors
    for(i = 0; i < 8; i++)
    {
      chr = ' ';

      // FIXME: These will look ugly in SMZX mode, so only draw them in regular mode.
      if(!screen_mode)
      {
        if((i == 5) && (bg_color >= 10))
          chr = '1';

        if(i == 6)
          chr = '0' + (bg_color % 10);
      }

      draw_char_mixed_pal_ext(chr, bg_color, fg_color,
       x + (i/4), y + (i%4), PRO_CH);
    }

    select_layer(UI_LAYER);

    // Clear the bottom
    draw_char('\xC4', DI_GREY, x, y + 4);
    draw_char('\xC4', DI_GREY, x+1, y + 4);

    if(bg_color == current_id)
    {
      // Draw '^^'
      write_string("\x1e\x1e",
       x, y + 4, DI_GREY_TEXT, 0);
    }
  }
}

static int palette_editor_input_16(struct color_status *current,
 const struct color_mode *mode, int key)
{
  if(get_mouse_press())
  {
    int mouse_x, mouse_y;
    get_mouse_position(&mouse_x, &mouse_y);

    // A position in the palette: select the color

    if((mouse_x >= content_x) && (mouse_x < content_x + 32) &&
     (mouse_y >= content_y) && (mouse_y < content_y + 4))
    {
      current_id = (mouse_x - content_x) / 2;
      return -1;
    }
  }

  switch(key)
  {
    case IKEY_LEFT:
    case IKEY_MINUS:
    case IKEY_KP_MINUS:
    {
      if(current_id > 0)
        current_id--;
      break;
    }

    case IKEY_RIGHT:
    case IKEY_EQUALS:
    case IKEY_KP_PLUS:
    {
      if(current_id < 15)
        current_id++;
      break;
    }

    default:
    {
      // Pass the key through to the common handler.
      return key;
    }
  }

  // Update the display.
  return -1;
}


// -----------------------------------------------------------------------------


static void palette_editor_redraw_window(struct color_status *current_color,
 const struct color_mode *current_mode)
{
  restore_screen();
  save_screen();
  palette_editor_redraw_window_16(current_color, current_mode);
  palette_editor_redraw_color_window(current_color, current_mode);
}

static void palette_editor_update_window(struct color_status *current_color,
 const struct color_mode *current_mode)
{
  palette_editor_update_window_16(current_color, current_mode);
  palette_editor_update_color_window(current_color, current_mode);
}

static int palette_editor_input(struct color_status *current_color,
 const struct color_mode *current_mode, int key)
{
  key = palette_editor_input_16(current_color, current_mode, key);
  key = palette_editor_input_color_window(current_color, current_mode, key);
  return key;
}

static struct color_status *rebuild_palette(struct color_status *palette)
{
  int palette_size = (get_screen_mode() >= 2) ? 256 : 16;
  int i;

  if(!palette)
  {
    palette = cmalloc(palette_size * sizeof(struct color_status));
  }

  for(i = 0; i < palette_size; i++)
  {
    update_color(&(palette[i]), i);
  }

  current_id %= palette_size;

  return palette;
}

void palette_editor(struct world *mzx_world)
{
  const struct color_mode *current_mode;
  struct color_status *current_color;
  struct color_status *palette;
  int refresh_window = 1;
  int refresh_elements = 1;
  int key;

  if(minimal_help == -1)
  {
    minimal_help = mzx_world->editor_conf.pedit_hhelp;
  }

  palette = rebuild_palette(NULL);

  cursor_off();
  set_context(CTX_PALETTE_EDITOR);
  save_screen();

  do
  {
    current_mode = &(mode_list[current_mode_id]);
    current_color = &(palette[current_id]);

    if(refresh_window)
    {
      // Draw the palette editor window.
      palette_editor_redraw_window(current_color, current_mode);
      refresh_window = 0;
    }

    if(refresh_elements)
    {
      // Update the palette editor window.
      palette_editor_update_window(current_color, current_mode);
    }
    refresh_elements = 1;

    update_screen();
    update_event_status_delay();

    key = get_key(keycode_internal_wrt_numlock);

    // Exit event -- mimic Escape
    if(get_exit_status())
      key = IKEY_ESCAPE;

    // Run it through the editor-specific handling.
    key = palette_editor_input(current_color, current_mode, key);

    switch(key)
    {
      case -2:
      {
        // Redraw entire window.
        refresh_window = 1;
        break;
      }

      case -1:
      {
        // Redraw window elements.
        break;
      }

      case IKEY_q:
      {
        key = IKEY_ESCAPE;
        break;
      }

      case IKEY_h:
      {
        if(get_alt_status(keycode_internal))
        {
          minimal_help ^= 1;
          refresh_window = 1;
        }
        break;
      }

      case IKEY_PAGEUP:
      {
        if(current_mode_id == 0)
          current_mode_id = ARRAY_SIZE(mode_list);

        current_mode_id--;
        refresh_window = 1;
        break;
      }

      case IKEY_PAGEDOWN:
      {
        current_mode_id++;

        if(current_mode_id == ARRAY_SIZE(mode_list))
          current_mode_id = 0;

        refresh_window = 1;
        break;
      }

      case IKEY_0:
      {
        current_color->r = 0;
        current_color->g = 0;
        current_color->b = 0;
        set_color_rgb(current_color);
        break;
      }

      case IKEY_d:
      {
        if(get_alt_status(keycode_internal))
        {
          default_palette();
          rebuild_palette(palette);
        }
        break;
      }

      case IKEY_F2:
      {
        memcpy(&saved_color, current_color, sizeof(struct color_status));
        saved_color_active = true;
        break;
      }

      case IKEY_F3:
      {
        if(saved_color_active)
        {
          memcpy(current_color, &saved_color, sizeof(struct color_status));

          set_color_rgb(current_color);
        }
        break;
      }

#ifdef CONFIG_HELPSYS
      case IKEY_F1:
      {
        m_show();
        help_system(mzx_world);
        break;
      }
#endif

      default:
      {
        refresh_elements = 0;
        break;
      }
    }
  } while(key != IKEY_ESCAPE);

  restore_screen();
  pop_context();

  free(palette);
}
