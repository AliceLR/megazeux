/* MegaZeux
 *
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
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

/* NOMENCLATURE NOTICE:
 * What we call the "subscreen" is called the "main screen" by libnds.
 * But in terms of functionality, it only contains auxiliary screen modes.
 */

#include <string.h>

#include "render_nds.h"
#include "renderers.h"
#include "graphics.h"
#include "render.h"

#include "keyboard.h"

// These variables control the panning along the 1:1 "main" screen.
static int cell_pan_x = 0;
static int cell_pan_y = 0;

// This table stores scroll register values for each scanline.
static u16 scroll_table[192];

// If we're looking around with the mouse, ignore the next call to focus.
static bool mouselook;

// The subscreen can display different information.
enum Subscreen_Mode subscreen_mode;

// Forward declarations
void nds_mainscreen_focus(struct graphics_data *graphics, Uint32 x, Uint32 y);

bool is_scaled_mode(enum Subscreen_Mode mode)
{
  return (mode == SUBSCREEN_SCALED);
}

static void nds_subscreen_scaled_init(void)
{
  int xscale = (int)(320.0/256.0 * 256.0);
  int yscale = (int)(350.0/192.0 * 256.0);

  /* Use banks A and B for the ZZT screen. */
  videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_BG);

  /* Use flickered alpha lerp to scale 320x350 to 256x192. */
  BG2_CR  = BG_BMP8_512x512 | BG_BMP_BASE(0) | BG_PRIORITY(0);
  BG2_XDX = xscale;
  BG2_XDY = 0;
  BG2_YDX = 0;
  BG2_YDY = yscale;
  BG2_CX  = 0;
  BG2_CY  = 0;

  BG3_CR  = BG_BMP8_512x512 | BG_BMP_BASE(0) | BG_PRIORITY(0);
  BG3_XDX = xscale;
  BG3_XDY = 0;
  BG3_YDX = 0;
  BG3_YDY = yscale;
  BG3_CX  = 0;
  BG3_CY  = 0;

  /* Enable BG2/BG3 blending. */
  BLEND_CR = BLEND_ALPHA | BLEND_SRC_BG2 | BLEND_DST_BG3;
  BLEND_AB = (8 << 8) | 8; /* 50% / 50% */

  update_palette();
  update_screen();
}


static void nds_subscreen_keyboard_init(void)
{
  vramSetBankA(VRAM_A_MAIN_BG);
  initKeyboard();

  // Disable blending.
  BLEND_CR = 0;
}

static void nds_mainscreen_init(struct graphics_data *graphics)
{
  u16 *vram;
  int i;

  graphics->fullscreen = true;
  graphics->resolution_width = 640;
  graphics->resolution_height = 350;

  /* Use bank C for the text screens. */
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE);
  vramSetBankC(VRAM_C_SUB_BG);

  /* BG0: foreground characters. */
  SUB_BG0_CR = BG_64x32 | BG_COLOR_16 | BG_MAP_BASE(0) |
               BG_TILE_BASE(1);
  SUB_BG0_X0 = 0;
  SUB_BG0_Y0 = 0;

  /* BG1: background characters. */
  SUB_BG1_CR = BG_64x32 | BG_COLOR_16 | BG_MAP_BASE(2) |
               BG_TILE_BASE(1);
  SUB_BG1_X0 = 0;
  SUB_BG1_Y0 = 0;

  // By default, pan to the center of the screen.
  nds_mainscreen_focus(graphics, 640/2, 350/2);

  // Add a solid tile for background colors.
  vram = (u16*)BG_TILE_RAM_SUB(1) + 32*256;
  for(i = 0; i < 16; i++)
    *(vram++) = 1 << 12 | 1 << 8 | 1 << 4 | 1;
}

void nds_video_jitter(void)
{
  const u16 jitterF4[] = {
    0x060, 0x000,   /* 0.375, 0.000 */
    0x020, 0x100,   /* 0.125, 1.000 */
    0x0e0, 0x000,   /* 0.875, 0.000 */
    0x0a0, 0x100,   /* 0.625, 1.000 */
  };
  static size_t jidx = 0;    /* jitter table index */

  /* Jitter the backgrounds for scaled mode. */
  if(is_scaled_mode(subscreen_mode))
  {
    if(jidx >= sizeof(jitterF4)/sizeof(*jitterF4))
      jidx = 0;

    BG2_CX = jitterF4[jidx];
    BG2_CY = jitterF4[jidx+1];
    BG3_CX = jitterF4[jidx+2];
    BG3_CY = jitterF4[jidx+3];
    jidx += 4;
  }
}

/* Use HBlank DMA to load row scroll values in for each line. */
void nds_video_rasterhack(void)
{
  /* Prepare DMA transfer. */
  DMA1_CR    = 0;
  SUB_BG0_Y0 = scroll_table[0];
  DMA1_SRC   = (u32)(scroll_table + 1);
  DMA1_DEST  = (u32)&SUB_BG0_Y0;
  DMA1_CR    = DMA_DST_FIX | DMA_SRC_INC | DMA_REPEAT | DMA_16_BIT |
               DMA_START_HBL | DMA_ENABLE | 1;

  DMA2_CR    = 0;
  SUB_BG1_Y0 = scroll_table[0];
  DMA2_SRC   = (u32)(scroll_table + 1);
  DMA2_DEST  = (u32)&SUB_BG1_Y0;
  DMA2_CR    = DMA_DST_FIX | DMA_SRC_INC | DMA_REPEAT | DMA_16_BIT |
               DMA_START_HBL | DMA_ENABLE | 1;
}

void nds_sleep_check(void)
{
  static bool asleep = false;

  // Check if we were just woken up.
  if(asleep)
  {
    asleep = false;

    // Wait a bit until returning power.
    while(REG_VCOUNT != 0);
    while(REG_VCOUNT == 0);
    while(REG_VCOUNT != 0);
    powerON(POWER_ALL);
  }

  // Check if it's time to go to sleep.
  if(keysDown() & KEY_LID)
  {
    // Cancel DMA from raster effects.
    DMA1_CR = 0;
    DMA2_CR = 0;

    // Power everything off.
    powerOFF(POWER_ALL);
    asleep = true;
  }
}

static bool nds_init_video(struct graphics_data *graphics,
 struct config_info *config)
{
  lcdMainOnBottom();

  // Start with scaled mode.
  subscreen_mode = SUBSCREEN_SCALED;
  nds_subscreen_scaled_init();

  // Initialize the 1:1 scaled "main" screen.
  nds_mainscreen_init(graphics);

  mouselook = false;
  return true;
}

static bool nds_check_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  return true;  // stub
}

static bool nds_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  return true;	// stub
}

// Render the scaled screen.
static void nds_render_graph_scaled(struct graphics_data *graphics)
{
  int const WIDTH_IN_PIXELS  = 80 * 4;  // Screen width in pixels
  int const WIDTH_IN_CELLS   = 80;      // Screen width in chars
  int const CHARACTER_HEIGHT = 14;      // Character height in pixels
  int const VRAM_WIDTH       = 512;     // HW surface width in pixels

  struct char_element *text_cell = graphics->text_video;
  u32 *vram_ptr  = (u32*)BG_BMP_RAM(0);
  int chars, lines;

  /* Iterate over every character in text memory. */
  int columns = 0;
  for(chars = 0; chars < 80*25; chars++)
  {
    /* Extract the character, and fore/back colors. */
    int chr = (*text_cell).char_value & 0xFF;
    int bg  = (*text_cell).bg_color   & 0x0F;
    int fg  = (*text_cell).fg_color   & 0x0F;

    /* Construct a table mapping charset two-bit pairs to
     * palette entries. */
    u32 pal_tbl[4] = {
      bg*16 + bg,       /* 00: bg color    */
      bg*16 + fg,       /* 01: fg+bg blend */
      bg*16 + fg,       /* 10: "         " */
      fg*16 + fg        /* 11: fg color    */
    };

    /* Iterate over every line in the character. */
    u8 *line = graphics->charset + CHARACTER_HEIGHT * chr;
    u32* vram_next = vram_ptr + 1;
    for(lines = 0; lines < CHARACTER_HEIGHT; lines++)
    {
      /* Plot four pixels using the two-bit pair table. */
      u32 fourpx = pal_tbl[((*line) & 0xC0) >> 6]      ;
      fourpx    |= pal_tbl[((*line) & 0x30) >> 4] <<  8;
      fourpx    |= pal_tbl[((*line) & 0x0C) >> 2] << 16;
      fourpx    |= pal_tbl[((*line) & 0x03)     ] << 24;
      *((u32*)vram_ptr) = fourpx;

      /* Advance to the next line. */
      line++;
      vram_ptr += VRAM_WIDTH/4;
    }

    /* Advance to the next character. */
    text_cell++;
    vram_ptr = vram_next;

    /* Insert padding at the end of a line. */
    columns++;
    if(columns == WIDTH_IN_CELLS)
    {
      columns = 0;
      vram_ptr += (VRAM_WIDTH - WIDTH_IN_PIXELS)/4;
      vram_ptr += VRAM_WIDTH * 13 / 4;
    }
  }
}

static void nds_render_graph_1to1(struct graphics_data *graphics)
{
  struct char_element *text_start =
   graphics->text_video + cell_pan_x + 80*cell_pan_y;
  struct char_element *text_cell  = text_start;
  u16 *vram;
  int x, y;

  /* Plot the first 32x30 foreground tiles. */
  vram = (u16*)BG_MAP_RAM_SUB(0);
  for(y = 0; y < 15; y++)
  {
    // Draw the top halves of this line for tile_offset=0, then the bottom
    // halves for tile_offset=1.
    int tile_offset = 0;

    while(tile_offset != 2)
    {
      int chr;
      int fg;

      for(x = 0; x < 32; x++)
      {
        // Extract the character and the foreground color.
        chr = (*text_cell).char_value & 0xFF;
        fg  = (*text_cell).fg_color   & 0x0F;

        *vram = (2*chr + tile_offset) | (fg << 12);

        text_cell++;
        vram++;
      }

      // Plot the 33rd column (in the next plane)
      chr = (*text_cell).char_value & 0xFF;
      fg  = (*text_cell).fg_color   & 0x0F;
      *(vram+992) = (2*chr + tile_offset) | (fg << 12);

      // Move back.
      text_cell -= 32;
      tile_offset++;
    }

    // Move to the next row.
    text_cell += 80;
  }

  // Plot the first 32x30 background tiles.
  text_cell = text_start;
  vram = (u16*)BG_MAP_RAM_SUB(2);
  for(y = 0; y < 15; y++)
  {
    // Draw the top halves of this line for tile_offset=0, then the bottom
    // halves for tile_offset=1.
    int tile_offset = 0;

    while(tile_offset != 2)
    {
      int bg;

      for(x = 0; x < 32; x++)
      {
        // Extract the background color only.
        bg  = (*text_cell).bg_color & 0x0F;

        *vram = 512 | (bg << 12);

        text_cell++;
        vram++;
      }

      // Plot the 33rd column (in the next plane)
      bg = (*text_cell).bg_color & 0x0F;
      *(vram+992) = 512 | (bg << 12);

      // Move back.
      text_cell -= 32;
      tile_offset++;
    }

    // Move to the next row.
    text_cell += 80;
  }
}

static void nds_render_graph(struct graphics_data *graphics)
{
  // Render the scaled screen if requested.
  if(is_scaled_mode(subscreen_mode))
    nds_render_graph_scaled(graphics);

  // Always render the 1:1 "main" screen.
  nds_render_graph_1to1(graphics);
}

static void nds_update_palette_entry(struct rgb_color *palette, Uint32 idx)
{
  u16* hw_pal  = PALETTE + idx;
  struct rgb_color color1 = palette[idx];
  int entry2;

  if(idx < 16)
  {
    // Update the mainscreen palette.
    PALETTE_SUB[16*idx + 1] = RGB15(color1.r/8, color1.g/8, color1.b/8);

    // Update the subscreen palette if requested.
    if(is_scaled_mode(subscreen_mode))
    {
      /* Iterate over an entire column of the palette, blending this color
       * with all 16 other colors (including itself). */
      for(entry2 = 0; entry2 < 16; entry2++)
      {
        /* Average the colors while reducing their accuracy to 5 bits. */
        struct rgb_color color2 = palette[entry2];
        u16 r = (color1.r + color2.r) / 16;
        u16 g = (color1.g + color2.g) / 16;
        u16 b = (color1.b + color2.b) / 16;

        *hw_pal  = RGB15(r, g, b);

        /* Advance to next row. */
        hw_pal += 16;
      }
    }
  }
}

static void nds_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  Uint32 i;

  for(i = 0; i < count; i++)
    nds_update_palette_entry(palette, i);
}

static void nds_resize_screen(struct graphics_data *graphics, int w, int h)
{
  // stub
}

static void nds_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  // stub
}

static void nds_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  // stub
}

static void nds_sync_screen(struct graphics_data *graphics)
{
  // stub
}

static void nds_remap_char(struct graphics_data *graphics, Uint16 chr)
{
  if(chr < 256)
  {
    /* Each character is 64 bytes.  Advance the vram pointer. */
    u16* vram = (u16*)BG_TILE_RAM_SUB(1) + 32*chr;
    u8* charset = graphics->charset + 14*chr;

    /* Iterate over every line. */
    u8 *end = charset + 14;
    for(; charset < end; charset++)
    {
      u16 line = *charset;

      /* Plot 8 pixels, 4 pixels at a time. */
      *(vram++) = ((line >> 7) & 1)       |
                  ((line >> 6) & 1) <<  4 |
                  ((line >> 5) & 1) <<  8 |
                  ((line >> 4) & 1) << 12;
      *(vram++) = ((line >> 3) & 1)       |
                  ((line >> 2) & 1) <<  4 |
                  ((line >> 1) & 1) <<  8 |
                  ((line     ) & 1) << 12;
    }
  }
}

static void nds_remap_charsets(struct graphics_data *graphics)
{
  int i;

  for(i = 0; i < 256; i++)
    nds_remap_char(graphics, i);
}

// Focus on a given screen position (in pixels up to 640x350).
void nds_mainscreen_focus(struct graphics_data *graphics, Uint32 x, Uint32 y)
{
  int scroll_x, scroll_y;
  int i;

  if(mouselook)
  {
    // We're mouselooking, don't move the focus.
    return;
  }

  // Clamp the coordinates so we stay within the screen.
  if(x < 132) x = 132;
  if(x > 508) x = 508;
  if(y <  96) y =  96;
  if(y > 253) y = 253;

  // Move to the top-left corner.
  x -= 264/2;
  y -= 192/2;

  // Find the relevant cell coordinates and scroll offsets into those cells.
  cell_pan_x = x / 8;
  scroll_x   = x % 8;
  cell_pan_y = y / 14;
  scroll_y   = y % 14;

  // Adjust the X scroll registers now.
  SUB_BG0_X0 = scroll_x;
  SUB_BG1_X0 = scroll_x;

  // Recalculate the scroll table.
  u16 *sptr    = scroll_table;
  int ypos     = scroll_y;
  int ycounter = scroll_y;
  for(i = 0; i < 192; i++)
  {
    (*sptr++) = ypos;

    ycounter++;
    if(ycounter == 14)
    {
      ycounter = 0;
      ypos += 2;
    }
  }

  // Redraw the map with our new coords.
  nds_render_graph_1to1(graphics);
}

void render_nds_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = nds_init_video;
  renderer->check_video_mode = nds_check_video_mode;
  renderer->set_video_mode = nds_set_video_mode;
  renderer->update_colors = nds_update_colors;
  renderer->resize_screen = nds_resize_screen;
  renderer->remap_charsets = nds_remap_charsets;
  renderer->remap_char = nds_remap_char;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = nds_render_graph;
  renderer->render_cursor = nds_render_cursor;
  renderer->render_mouse = nds_render_mouse;
  renderer->sync_screen = nds_sync_screen;
  renderer->focus_pixel = nds_mainscreen_focus;
}

void nds_subscreen_switch(void)
{
  // Cycle between modes.
  if(++subscreen_mode == SUBSCREEN_MODE_COUNT)
    subscreen_mode = 0;

  // Call appropriate initialization function.
  switch(subscreen_mode)
  {
    case SUBSCREEN_SCALED:
      nds_subscreen_scaled_init();
      break;

    case SUBSCREEN_KEYBOARD:
      nds_subscreen_keyboard_init();
      break;

    default:
      break;
  }
}

void nds_mouselook(bool enabled)
{
  mouselook = enabled;
}

// Display a warning screen.
void warning_screen(u8 *pcx_data)
{
  sImage image;

  consoleDemoInit();

  if(loadPCX(pcx_data, &image))
  {
    videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);

    BG2_CR  = BG_BMP8_256x256;
    BG2_XDX = 1 << 8;
    BG2_XDY = 0;
    BG2_YDX = 0;
    BG2_YDY = 1 << 8;
    BG2_CX  = 0;
    BG2_CY  = 0;

    memcpy(PALETTE, image.palette, 2*256);

    int ox = 128 - image.width/2;
    int oy = 96 - image.height/2;
    u16 *gfx = (u16*)(0x06000000 + 256*oy + ox);
    u16 *img = image.image.data16;
    int row;
    int col;

    for(row = 0; row < image.height; row++)
    {
      for(col = 0; col < image.width/2; col++)
        *(gfx++) = *(img++);
      gfx += ox;
    }

    while(!keysDown())
      scanKeys();

    imageDestroy(&image);
  }
}
