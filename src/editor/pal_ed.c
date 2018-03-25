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

#include "../data.h"
#include "../event.h"
#include "../graphics.h"
#include "../helpsys.h"
#include "../util.h"
#include "../window.h"

#include "graphics.h"
#include "pal_ed.h"
#include "window.h"


//----------------------------------//------------------------------------------/
// ##..##..##..##..##..##..##..##.. // Color # 000           RGB  HSL  CIELAB   /
// ##..##..##..##..##..#1.1#1.1#1.1 // Red   0 [----|----|----|----|----|----|] /
// #0.1#2.3#4.5#6.7#8.9#0.1#2.3#4.5 // G     0 [----|----|----|----|----|----|] /
// ##..##..##..##..##..##..##..##.. // B     0 [----|----|----|----|----|----|] /
//-^^-------------------------------//------------------------------------------/

//----------------------------------------------------------------/
//
// %- Select color    Alt+D- Default pal.   PgUp- Prev. mode
// R- Increase Red    Alt+R- Decrease Red   PgDn- Next mode
// G- Increase Green  Alt+G- Decrease Green   F2- Store color
// B- Increase Blue   Alt+B- Decrease Blue    F3- Retrieve color
// A- Increase All    Alt+A- Decrease All
// 0- Blacken color   Alt+H- Hide help         Q- Quit editing
//
// Left click- edit component/activate   Right click- color (drag)
//
//----------------------------------------------------------------/

//----------------------------------/
// Palette  1:123 2:123 3:123 4:123 /
//  # 000   ##### ##### ##### ##### /
//          ##### ##### ##### ##### /
// Hex: FF  ##### ##### ##### ##### /
//----------------------------------/

//-------/----------------------------------/ 36*18, color pos + 8
//       / [][][][][][][][][][][][][][][][] /
//       / ... 16 + 2

//------------------------------------------/
// %%- Select color   Alt+D- Default pal.   /
// R- Increase Red    Alt+R- Decrease Red   /
// G- Increase Green  Alt+G- Decrease Green /
// B- Increase Blue   Alt+B- Decrease Blue  /
// A- Increase All    Alt+A- Decrease All   /
// 0- Blacken color   Alt+H- Hide help      /
//
// PgUp- Prev. mode   PgDn- Next mode       /
// F2- Store color    F3- Retrieve color    /
//
// Space- Subpalette  1-4- Current color to
// Q- Quit editing         subpalette index

// Space- Toggle subpalette index cursors
// Q- Quit editing

//
//   Left click- edit component/activate
//  Right click- change color (drag)
// Middle/wheel- change subpalette
//------------------------------------------/

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
#define PAL_ED_16_HELP_X2     73
#define PAL_ED_16_HELP_Y1     7
#define PAL_ED_16_HELP_Y2     18

#define PAL_ED_256_SEL_X1     0
#define PAL_ED_256_SEL_X2     35
#define PAL_ED_256_SEL_Y1     7
#define PAL_ED_256_SEL_Y2     24

#define PAL_ED_256_PAL_X1     0
#define PAL_ED_256_PAL_X2     35
#define PAL_ED_256_PAL_Y1     0
#define PAL_ED_256_PAL_Y2     5

#define PAL_ED_256_HELP_X1    36
#define PAL_ED_256_HELP_X2    79
#define PAL_ED_256_HELP_Y1    7
#define PAL_ED_256_HELP_Y2    24

#define CHAR_LINE_HORIZ '\xC4'
#define CHAR_CORNER_TL  '\xDA'
#define CHAR_CORNER_TR  '\xBF'
#define CHAR_CORNER_BL  '\xC0'
#define CHAR_CORNER_BR  '\xD9'

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
static int mode2_extra_cursors = 1;

static unsigned int current_id = 0;
static unsigned int current_subpalette = 0;
static int current_mode_id = 0;

static Uint32 cursor_fg_layer;
static Uint32 cursor_bg_layer;


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
    case IKEY_c:
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
  int draw_x;
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
    24
  },

  { "HSL",
    {
      { "Hue",   "Hue",  "C",  4,  4,    0, 359 },
      { "Sat.",  "Sat",  "S",  7,  0,    0, 100 },
      { "Light", "Lgt",  "V", 15,  0,    0, 100 },
    },
    set_color_hsl,
    get_color_hsl,
    key_color_hsl,
    set_color_hsl_bar,
    false,
    29
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
    34
  },
};


// -----------------------------------------------------------------------------


static int palette_editor_input_component(int value, int min_val, int max_val,
 int x, int y)
{
  int result = 0;
  int new_value = value;
  int update = true;
  char buffer[6];
  int buf_pos;
  int key;
  int i;

  save_screen();

  do
  {
    if(update)
    {
      new_value = CLAMP(new_value, min_val, max_val);
      if(snprintf(buffer, 6, "%d", new_value))
        buffer[5] = 0;

      buf_pos = strlen(buffer);
      update = false;
    }

    draw_char(221, DI_GREY_CORNER, x+4, y);

    for(i = 0; i < 4; i++)
      draw_char(0, DI_GREY_EDIT, x+i, y);

    write_string(buffer, x+4-buf_pos, y, DI_GREY_EDIT, 1);

    update_screen();
    update_event_status_delay();

    key = get_key(keycode_internal_wrt_numlock);

    // Exit event -- mimic Escape
    if(get_exit_status())
      key = IKEY_ESCAPE;

    // New mouse press exits with changes.
    if(get_mouse_press() && !get_mouse_drag())
      key = IKEY_RETURN;

    // Get old buffer value
    new_value = strtol(buffer, NULL, 10);
    update = false;

    if((key >= IKEY_0) && (key <= IKEY_9) && (buf_pos < 5))
    {
      // At exactly maximum/minimum, typing a number should wrap
      if((new_value == min_val && min_val < 0)
       || (new_value == max_val && max_val > 0))
        new_value = 0;

      else
        new_value = abs(new_value) * 10 + (key - IKEY_0);

      // Apply '-'
      if(buffer[0] == '-')
        new_value *= -1;

      update = true;
    }

    switch(key)
    {
      case IKEY_RETURN:
      case IKEY_TAB:
      {
        key = IKEY_ESCAPE;
        result = 1;
        break;
      }

      case IKEY_DELETE:
      case IKEY_PERIOD:
      {
        // Set to zero
        key = IKEY_ESCAPE;
        buffer[0] = 0;
        result = 1;
        break;
      }

      case IKEY_BACKSPACE:
      {
        if(buf_pos > 0)
        {
          buf_pos--;
          buffer[buf_pos] = 0;
        }
        break;
      }

      case IKEY_MINUS:
      {
        // Add a leading '-'
        if(buf_pos == 0 || buffer[0] == '0')
        {
          strcpy(buffer, "-");
          buf_pos = 1;
        }
        else

        // Negate the existing number
        if(-new_value >= min_val && -new_value <= max_val)
        {
          new_value *= -1;
          update = true;
        }
        break;
      }
    }
  }
  while(key != IKEY_ESCAPE);

  restore_screen();

  if(result)
  {
    value = strtol(buffer, NULL, 10);
    value = CLAMP(value, min_val, max_val);
  }

  return value;
}


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
  for(i = 0; i < (int)ARRAY_SIZE(mode_list); i++)
  {
    color = (i == current_mode_id) ? DI_GREY : DI_GREY_DARK;

    write_string(
     mode_list[i].name,
     PAL_ED_COL_X1 + mode_list[i].draw_x,
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
     DI_GREY_NUMBER,
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
  if(get_mouse_press() == MOUSE_BUTTON_LEFT)
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

      // Snap the mouse to the center of the bar.
      warp_real_mouse_y(mouse_y * 14 + 7);
      return -1;
    }
    else

    // Component numbers

    if(
     !get_mouse_drag() &&
     (mouse_x >= PAL_ED_COL_X1 + 2) &&
     (mouse_x < PAL_ED_COL_X1 + 10 - 1) &&
     (mouse_y >= PAL_ED_COL_Y1 + 2) &&
     (mouse_y < PAL_ED_COL_Y1 + 5))
    {
      const struct color_mode_component *c;
      int component = mouse_y - PAL_ED_COL_Y1 - 2;
      int value;

      c = &(mode->components[component]);
      value = mode->get_function(current, component);

      value = palette_editor_input_component(value, c->min_val, c->max_val,
       PAL_ED_COL_X1 + 5, PAL_ED_COL_Y1 + component + 2);

      mode->input_bar_function(current, component, value);
      return -1;
    }
    else

    // Mode select

    if(!get_mouse_drag() && (mouse_y == PAL_ED_COL_Y1 + 1))
    {
      int i;

      for(i = 0; i < (int)ARRAY_SIZE(mode_list); i++)
      {
        const struct color_mode *m = &(mode_list[i]);
        if((mouse_x >= PAL_ED_COL_X1 + m->draw_x) &&
         (mouse_x < (int)(PAL_ED_COL_X1 + m->draw_x + strlen(m->name))))
        {
          current_mode_id = i;
          break;
        }
      }
      return -2;
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
      "0- Blacken color   Alt+H- Hide help         Q- Quit editing\n"
      "\n"
      "Left click- edit component/activate  Right click- color (drag)\n",
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
  int i;
  int x;
  int y;

  // Draw palette bars
  for(bg_color = 0; bg_color < 16; bg_color++)
  {
    x = bg_color * 2 + content_x;
    y = content_y;

    // The foreground color is white or black in the protected palette.

    fg_color = (get_color_luma(bg_color) < 128) ? 31 : 16;

    select_layer(OVERLAY_LAYER);

    // Draw the palette colors
    for(i = 0; i < 8; i++)
    {
      chr = ' ';

      // These look ugly in SMZX mode 1, so only draw them in regular mode.
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
    draw_char(CHAR_LINE_HORIZ, DI_GREY, x, y + 4);
    draw_char(CHAR_LINE_HORIZ, DI_GREY, x+1, y + 4);

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
  if(get_mouse_press() == MOUSE_BUTTON_LEFT)
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


static void palette_editor_redraw_window_256(struct color_status *current,
 const struct color_mode *mode, int smzx_mode)
{
  const struct color_mode_component *c;
  int i;
  int x;
  int y;

  // Palette View
  draw_window_box(PAL_ED_256_PAL_X1, PAL_ED_256_PAL_Y1, PAL_ED_256_PAL_X2,
   PAL_ED_256_PAL_Y2, DI_GREY_DARK, DI_GREY, DI_GREY_CORNER, 1, 1);

  // Fill the char missed by the shadow (it looks bad w/ the color selector).
  draw_char(0, 0, PAL_ED_256_PAL_X1, PAL_ED_256_PAL_Y2 + 1);

  // Color Selector
  draw_window_box(PAL_ED_256_SEL_X1, PAL_ED_256_SEL_Y1, PAL_ED_256_SEL_X2,
   PAL_ED_256_SEL_Y2, DI_GREY, DI_GREY_DARK, DI_GREY_CORNER, 1, 1);

  content_x = PAL_ED_256_SEL_X1 + 2;
  content_y = PAL_ED_256_SEL_Y1 + 1;

  // Palette View contents
  write_string(
   "Palette\n #\n\nHex:",
   PAL_ED_256_PAL_X1 + 2,
   PAL_ED_256_PAL_Y1 + 1,
   DI_GREY_TEXT,
   0
  );

  write_string(
   "1:    2:    3:    4:",
   PAL_ED_256_PAL_X1 + 11,
   PAL_ED_256_PAL_Y1 + 1,
   DI_GREY_CORNER,
   0
  );

  // Erase spots where palette colors will go
  for(i = 0; i < 15; i++)
  {
    erase_char(i % 5 + PAL_ED_256_PAL_X1 + 11, i / 5 + PAL_ED_256_PAL_Y1 + 2);
    erase_char(i % 5 + PAL_ED_256_PAL_X1 + 17, i / 5 + PAL_ED_256_PAL_Y1 + 2);
    erase_char(i % 5 + PAL_ED_256_PAL_X1 + 23, i / 5 + PAL_ED_256_PAL_Y1 + 2);
    erase_char(i % 5 + PAL_ED_256_PAL_X1 + 29, i / 5 + PAL_ED_256_PAL_Y1 + 2);
  }

  // Erase the spot where the palette will go.
  for(i = 0; i < 32*16; i++)
    erase_char(i % 32 + content_x, i / 32 + content_y);

  // Always enable help during smzx_mode 3; the preview area is useless.
  if(!minimal_help || smzx_mode==3)
  {
    draw_window_box(PAL_ED_256_HELP_X1, PAL_ED_256_HELP_Y1, PAL_ED_256_HELP_X2,
     PAL_ED_256_HELP_Y2, DI_GREY_DARK, DI_GREY, DI_GREY_CORNER, 1, 1);

    // Write menu
    write_string
    (
      "\x12\x1d- Select color   Alt+D- Default pal.   \n"
      " - Increase        Alt+ - Decrease       \n"
      " - Increase        Alt+ - Decrease       \n"
      " - Increase        Alt+ - Decrease       \n"
      "\n"
      "0- Blacken color\n"
      "\n"
      "PgUp- Prev. mode   PgDn- Next mode       \n"
      "F2- Store color    F3- Retrieve color    \n",
      PAL_ED_256_HELP_X1 + 2,
      PAL_ED_256_HELP_Y1 + 1,
      DI_GREY_TEXT,
      1
    );

    if(smzx_mode == 2)
    {
      // SMZX mode 2 specific help
      write_string
      (
        "Alt+H- Hide help",
        PAL_ED_256_HELP_X1 + 21,
        PAL_ED_256_HELP_Y1 + 6,
        DI_GREY_TEXT,
        1
      );
      write_string
      (
        "Space- Toggle subpalette index cursors\n"
        "Q- Quit editing\n"
        "\n"
        "  Left click- edit component/activate\n"
        " Right click- change color (drag)\n"
        "Middle/wheel- toggle subpalette cursors\n",
        PAL_ED_256_HELP_X1 + 2,
        PAL_ED_256_HELP_Y1 + 11,
        DI_GREY_TEXT,
        1
      );
    }
    else
    {
      // SMZX mode 3 specific help
      write_string
      (
        "Space- Subpalette  1-4- Current color to \n"
        "Q- Quit editing         subpalette index\n"
        "\n"
        "  Left click- edit component/activate\n"
        " Right click- change color (drag)\n"
        "Middle/wheel- change subpalette\n",
        PAL_ED_256_HELP_X1 + 2,
        PAL_ED_256_HELP_Y1 + 11,
        DI_GREY_TEXT,
        1
      );
    }

    // Component instructions
    y = PAL_ED_256_HELP_Y1 + 2;
    for(i = 0; i < 3; i++, y++)
    {
      c = &(mode->components[i]);
      x = PAL_ED_256_HELP_X1 + 2;

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
       PAL_ED_256_HELP_X1 + 2,
       PAL_ED_256_HELP_Y1 + 5,
       DI_GREY_TEXT,
       1
      );
    }
  }
}

static void draw_unbound_cursor_chars(Uint32 layer, int color)
{
  select_layer(layer);
  set_layer_mode(layer, 0);
  // Note: mode 0, so offset by 16 (PAL_SIZE) for the protected palette.
  draw_char_to_layer(color, CHAR_CORNER_TL, 0, 0, PRO_CH, PAL_SIZE);
  draw_char_to_layer(color, CHAR_CORNER_TR, 2, 0, PRO_CH, PAL_SIZE);
  draw_char_to_layer(color, CHAR_CORNER_BL, 0, 1, PRO_CH, PAL_SIZE);
  draw_char_to_layer(color, CHAR_CORNER_BR, 2, 1, PRO_CH, PAL_SIZE);
  draw_char_to_layer(color, CHAR_LINE_HORIZ, 1, 0, PRO_CH, PAL_SIZE);
  draw_char_to_layer(color, CHAR_LINE_HORIZ, 1, 1, PRO_CH, PAL_SIZE);
}

static void draw_unbound_cursor(int x, int y, int fg, int bg, int offset)
{
  // Cursor for mode 2/3 palette selector
  int order;

  x = x * CHAR_W - 3;
  y = y * CHAR_H - 5;
  fg += 0xD0;
  bg += 0xD0;

  // Shadow
  order = LAYER_DRAWORDER_UI + offset*2 + 1;
  cursor_bg_layer = create_layer(x, y, 3, 2, order, 0x10D, 0, 1);
  draw_unbound_cursor_chars(cursor_bg_layer, bg);

  // FG
  x -= 2;
  y--;
  order++;
  cursor_fg_layer = create_layer(x, y, 3, 2, order, 0x10D, 0, 1);
  draw_unbound_cursor_chars(cursor_fg_layer, fg);
}

static void palette_editor_update_window_256(struct color_status *current,
 const struct color_mode *mode, int smzx_mode)
{
  char buffer[5];
  char chr;
  int pal;
  int i, i2;

  int lo = current_id & 0x0F;
  int hi = (current_id & 0xF0) >> 4;
  int co = current_id;

  int blink_cursor = graphics.cursor_flipflop;
  int x = PAL_ED_256_PAL_X1;
  int y = PAL_ED_256_PAL_Y1;

  if(smzx_mode == 2)
  {
    int c0 = hi << 4 | hi;
    int c1 = lo << 4 | hi;
    int c2 = hi << 4 | lo;
    int c3 = lo << 4 | lo;

    // Current subpalette number
    sprintf(buffer, "%03d", current_id);
    write_string(buffer, x + 5, y + 2, DI_GREY_TEXT, 0);
    sprintf(buffer, "%02X", current_id);
    write_string(buffer, x + 7, y + 4, DI_GREY_TEXT, 0);

    // Subpalette indices
    sprintf(buffer, "%03d", c0);
    write_string(buffer, x + 13, y + 1, DI_GREY_TEXT, 0);
    sprintf(buffer, "%03d", c1);
    write_string(buffer, x + 19, y + 1, DI_GREY_TEXT, 0);
    sprintf(buffer, "%03d", c2);
    write_string(buffer, x + 25, y + 1, DI_GREY_TEXT, 0);
    sprintf(buffer, "%03d", c3);
    write_string(buffer, x + 31, y + 1, DI_GREY_TEXT, 0);

    select_layer(OVERLAY_LAYER);

    // Draw the current subpalette
    for(i = 0; i < 15; i++)
    {
      draw_char_ext(CHAR_SMZX_C0, co, (i%5 + x + 11), (i/5 + y + 2), PRO_CH, 0);
      draw_char_ext(CHAR_SMZX_C1, co, (i%5 + x + 17), (i/5 + y + 2), PRO_CH, 0);
      draw_char_ext(CHAR_SMZX_C2, co, (i%5 + x + 23), (i/5 + y + 2), PRO_CH, 0);
      draw_char_ext(CHAR_SMZX_C3, co, (i%5 + x + 29), (i/5 + y + 2), PRO_CH, 0);
    }

    // Draw the palette
    for(i = 0; i < 32; i++)
    {
      for(i2 = 0; i2 < 16; i2++)
      {
        pal = i2 << 4 | (i/2);
        chr = CHAR_SMZX_C2;
        draw_char_ext(chr, pal, (i + content_x), (i2 + content_y), PRO_CH, 0);
      }
    }

    // Extra mode 2 cursors
    if(mode2_extra_cursors)
    {
      if(blink_cursor)
      {
        draw_unbound_cursor(hi * 2 + content_x, hi + content_y, 0xC, 0x4, 0);
        draw_unbound_cursor(hi * 2 + content_x, lo + content_y, 0xC, 0x4, 1);
        draw_unbound_cursor(lo * 2 + content_x, lo + content_y, 0xC, 0x4, 2);
      }
      else
      {
        draw_unbound_cursor(hi * 2 + content_x, hi + content_y, 0x4, 0x0, 0);
        draw_unbound_cursor(hi * 2 + content_x, lo + content_y, 0x4, 0x0, 1);
        draw_unbound_cursor(lo * 2 + content_x, lo + content_y, 0x4, 0x0, 2);
      }
    }
  }

  // Mode 3
  else
  {
    // Note indices 1 and 2 are swapped to make sense.
    int sub = current_subpalette;
    int c0 = graphics.editor_backup_indices[sub * 4 + 0];
    int c1 = graphics.editor_backup_indices[sub * 4 + 2];
    int c2 = graphics.editor_backup_indices[sub * 4 + 1];
    int c3 = graphics.editor_backup_indices[sub * 4 + 3];

    // Current subpalette number
    sprintf(buffer, "%03d", current_subpalette);
    write_string(buffer, x + 5, y + 2, DI_GREY_TEXT, 0);
    sprintf(buffer, "%02X", current_subpalette);
    write_string(buffer, x + 7, y + 4, DI_GREY_TEXT, 0);

    // Subpalette indices
    sprintf(buffer, "%03d", c0);
    write_string(buffer, x + 13, y + 1, DI_GREY_TEXT, 0);
    sprintf(buffer, "%03d", c1);
    write_string(buffer, x + 19, y + 1, DI_GREY_TEXT, 0);
    sprintf(buffer, "%03d", c2);
    write_string(buffer, x + 25, y + 1, DI_GREY_TEXT, 0);
    sprintf(buffer, "%03d", c3);
    write_string(buffer, x + 31, y + 1, DI_GREY_TEXT, 0);

    select_layer(OVERLAY_LAYER);

    // Draw the current subpalette
    for(i = 0; i < 15; i++)
    {
      draw_char_ext(0, c0, (i%5 + x + 11), (i/5 + y + 2), PRO_CH, 0);
      draw_char_ext(0, c1, (i%5 + x + 17), (i/5 + y + 2), PRO_CH, 0);
      draw_char_ext(0, c2, (i%5 + x + 23), (i/5 + y + 2), PRO_CH, 0);
      draw_char_ext(0, c3, (i%5 + x + 29), (i/5 + y + 2), PRO_CH, 0);
    }

    // Draw the palette
    for(i = 0; i < 32*16; i++)
      draw_char_ext(0, i/2, (i%32 + content_x), (i/32 + content_y), PRO_CH, 0);
  }

  // Main cursor
  if(blink_cursor)
  {
    draw_unbound_cursor(lo * 2 + content_x, hi + content_y, 0xF, 0x8, 3);
  }
  else
  {
    draw_unbound_cursor(lo * 2 + content_x, hi + content_y, 0x7, 0x0, 3);
  }

  select_layer(UI_LAYER);
}

static int palette_editor_input_256(struct color_status *current,
 const struct color_mode *mode, int smzx_mode, int key)
{
  int mouse_press = get_mouse_press_ext();

  if(mouse_press)
  {
    int mouse_x, mouse_y;
    get_mouse_position(&mouse_x, &mouse_y);

    if(mouse_press == MOUSE_BUTTON_WHEELUP)
    {
      // Previous palette
      key = IKEY_MINUS;
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELDOWN)
    {
      // Next palette
      key = IKEY_EQUALS;
    }
    else

    if(mouse_press == MOUSE_BUTTON_MIDDLE)
    {
      // Palette selector/hide cursors
      key = IKEY_SPACE;
    }
    else

    if(mouse_press == MOUSE_BUTTON_LEFT &&
     (mouse_x >= content_x) && (mouse_x < content_x + 32) &&
     (mouse_y >= content_y) && (mouse_y < content_y + 16))
    {
      // Palette- select color
      current_id = (mouse_x - content_x)/2 + (mouse_y - content_y) * 16;
      return -1;
    }
    else

    if(mouse_press == MOUSE_BUTTON_LEFT &&
     (mouse_x >= PAL_ED_256_PAL_X1 + 1) &&
     (mouse_x <= PAL_ED_256_PAL_X1 + 9) &&
     (mouse_y >= PAL_ED_256_PAL_Y1 + 1) &&
     (mouse_y <= PAL_ED_256_PAL_Y2 - 1))
    {
      // Left side of subpalette menu
      if(smzx_mode == 3)
      {
        // Select palette- fake a space press
        key = IKEY_SPACE;
      }
    }
    else

    if(mouse_press == MOUSE_BUTTON_LEFT &&
     (mouse_x >= PAL_ED_256_PAL_X1 + 10) &&
     (mouse_x <= PAL_ED_256_PAL_X2 - 2) &&
     (mouse_y >= PAL_ED_256_PAL_Y1 + 1) &&
     (mouse_y <= PAL_ED_256_PAL_Y2 - 1) &&
     ((mouse_x - 10)%6 != 0))
    {
      // Subpalette colors
      int col = (mouse_x - 10)/6;

      if(smzx_mode == 3)
      {
        // Assign to index- fake a 1-4 keypress
        key = IKEY_1 + col;
      }
    }
  }

  switch(key)
  {
    case IKEY_SPACE:
    {
      if(smzx_mode == 2)
      {
        // Mode 2- toggle the extra palette cursors
        mode2_extra_cursors ^= 1;
      }
      else

      if(smzx_mode == 3)
      {
        // Mode 3- select subpalette
        int new_subpal;

        // Load the actual indices
        load_editor_indices();

        new_subpal = color_selection(current_subpalette, 0);

        // Reset indices for editor
        set_screen_mode(3);

        if(new_subpal >= 0)
          current_subpalette = new_subpal;
      }
      break;
    }

    case IKEY_UP:
    {
      if(current_id / 16 > 0)
        current_id -= 16;
      break;
    }

    case IKEY_DOWN:
    {
      if(current_id / 16 < 15)
        current_id += 16;
      break;
    }

    case IKEY_LEFT:
    {
      if(current_id % 16 > 0)
        current_id--;
      break;
    }

    case IKEY_RIGHT:
    {
      if(current_id % 16 < 15)
        current_id++;
      break;
    }

    case IKEY_MINUS:
    case IKEY_KP_MINUS:
    {
      if(smzx_mode == 3)
      {
        if(current_subpalette > 0)
          current_subpalette--;
      }
      else
      {
        if(current_id > 0)
          current_id--;
      }

      break;
    }

    case IKEY_EQUALS:
    case IKEY_KP_PLUS:
    {
      if(smzx_mode == 3)
      {
        if(current_subpalette < 255)
          current_subpalette++;
      }
      else
      {
        if(current_id < 255)
          current_id++;
      }

      break;
    }

    case IKEY_1:
    case IKEY_2:
    case IKEY_3:
    case IKEY_4:
    {
      if(smzx_mode == 3)
      {
        // Assign current color to subpalette index
        int index = (key - IKEY_1);

        // Indices 1 and 2 are swapped to make actual sense.
        if(index == 1)      index = 2;
        else if(index == 2) index = 1;

        index += current_subpalette * 4;

        graphics.editor_backup_indices[index] = current_id;
      }
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
 const struct color_mode *current_mode, int smzx_mode)
{
  restore_screen();
  save_screen();

  if(smzx_mode < 2)
    palette_editor_redraw_window_16(current_color, current_mode);

  else
    palette_editor_redraw_window_256(current_color, current_mode, smzx_mode);

  palette_editor_redraw_color_window(current_color, current_mode);
}

static void palette_editor_update_window(struct color_status *current_color,
 const struct color_mode *current_mode, int smzx_mode)
{
  if(smzx_mode < 2)
    palette_editor_update_window_16(current_color, current_mode);

  else
    palette_editor_update_window_256(current_color, current_mode, smzx_mode);

  palette_editor_update_color_window(current_color, current_mode);
}

static int palette_editor_input(struct color_status *current_color,
 const struct color_mode *current_mode, int smzx_mode, int key)
{
  if(smzx_mode <2)
    key = palette_editor_input_16(current_color, current_mode, key);

  else
    key = palette_editor_input_256(current_color, current_mode, smzx_mode, key);

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
  int smzx_mode = get_screen_mode();
  int mouse_last_x = -1;
  int mouse_last_y = -1;
  int key;

  if(minimal_help == -1)
  {
    minimal_help = mzx_world->editor_conf.pedit_hhelp;
  }

  palette = rebuild_palette(NULL);

  if(smzx_mode == 3)
  {
    save_editor_indices();

    // Reset the indices to default for display purposes.
    set_screen_mode(3);
  }

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  m_show();

  cursor_off();
  set_context(CTX_PALETTE_EDITOR);
  save_screen();

  do
  {
    select_layer(UI_LAYER);

    current_mode = &(mode_list[current_mode_id]);
    current_color = &(palette[current_id]);

    palette_editor_redraw_window(current_color, current_mode, smzx_mode);
    palette_editor_update_window(current_color, current_mode, smzx_mode);

    update_screen();
    update_event_status_delay();

    // Get rid of the extra cursor layers now because they display OVER the UI!
    // TODO should be called on each cursor layer
    // in case layer architecture changes
    destruct_extra_layers(0);

    key = get_key(keycode_internal_wrt_numlock);

    // Exit event -- mimic Escape
    if(get_exit_status())
      key = IKEY_ESCAPE;

    // Right click -- mimic directional presses
    if(!key && (get_mouse_press() == MOUSE_BUTTON_RIGHT))
    {
      int x;
      int y;
      get_mouse_position(&x, &y);

      if(get_mouse_drag())
      {
        if(abs(x-mouse_last_x) > abs(y-mouse_last_y))
        {
          if(x > mouse_last_x)
            key = IKEY_RIGHT;
          if(x < mouse_last_x)
            key = IKEY_LEFT;
        }
        else
        {
          if(y > mouse_last_y)
            key = IKEY_DOWN;
          if(y < mouse_last_y)
            key = IKEY_UP;
        }
        // Only warp the mouse if there's movement registered.
        // Otherwise, it will be unresponsive with precise mouse movements.
        if(key)
          warp_mouse(mouse_last_x, mouse_last_y);
      }
      else
      {
        mouse_last_x = x;
        mouse_last_y = y;
      }
    }

    // Run it through the editor-specific handling.
    key = palette_editor_input(current_color, current_mode, smzx_mode, key);

    switch(key)
    {
      case -2:
      {
        // Redraw entire window.
        // This gets done anyway now because of the layer effects.
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
          // SMZX mode 3 always has the help enabled, so don't allow toggling
          if(smzx_mode != 3)
            minimal_help ^= 1;
        }
        break;
      }

      case IKEY_PAGEUP:
      {
        if(current_mode_id == 0)
          current_mode_id = ARRAY_SIZE(mode_list);

        current_mode_id--;
        break;
      }

      case IKEY_PAGEDOWN:
      {
        current_mode_id++;

        if(current_mode_id == ARRAY_SIZE(mode_list))
          current_mode_id = 0;

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
          // SMZX modes 0 and 1 use the default palette
          if(smzx_mode <= 1)
          {
            default_palette();
          }
          // SMZX modes 2 and 3 need the externally stored SMZX palette
          else
          {
            load_palette(mzx_res_get_by_id(SMZX_PAL));
            update_palette();
          }

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
        help_system(mzx_world);
        break;
      }
#endif
    }
  } while(key != IKEY_ESCAPE);

  if(smzx_mode == 3)
  {
    // Apply the modified SMZX indices.
    load_editor_indices();
  }

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  restore_screen();
  pop_context();

  free(palette);
}
