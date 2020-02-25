/* MegaZeux
 *
 * Copyright (C) 2007-2009 Kevin Vance <kvance@kvance.com>
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

#include "render.h"

#include "../../src/renderers.h"
#include "../../src/graphics.h"
#include "../../src/render.h"

#include <nds/arm9/keyboard.h>

// These variables control the panning along the 1:1 "main" screen.
static int cell_pan_x = 0;
static int cell_pan_y = 0;

// When set to >= 0, the upper screen will attempt to focus to this position
// the next  time it is drawn and then reset them to -1.
static int focus_x = -1;
static int focus_y = -1;

// This table stores scroll register values for each scanline.
static u16 scroll_table[192];

// This table maps palette color combinations to their index in BG_PALETTE.
static int palette_idx_table[16][16];

// If we're looking around with the mouse, ignore the next call to focus.
static boolean mouselook;

// Y offset information for subscreen
static s16 subscreen_offset_y = 0;
u16 subscreen_height;

// The current transition state.
static struct {
  enum { TRANSITION_NONE = 0, TRANSITION_IN, TRANSITION_OUT } state;
  int time;
} transition;

// The subscreen can display different information.
enum Subscreen_Mode last_subscreen_mode;
enum Subscreen_Mode subscreen_mode;

// Forward declarations
static void nds_keyboard_scroll_in(void);
static void nds_keyboard_scroll_out(void);

// Do this every vblank irq:
static void nds_on_vblank(void)
{
  /* Handle sleep mode. */
  nds_sleep_check();

  /* Do all special video handling. */
  nds_video_rasterhack();
  nds_video_jitter();
  nds_video_do_transition();
}

boolean is_scaled_mode(enum Subscreen_Mode mode)
{
  return (mode == SUBSCREEN_SCALED);
}

static void palette_idx_table_init(void)
{
  // Start the palette at 16, reserving colors 0-16 for the overlay.
  const int offset = 16;
  int idx;

  // Store the normal palette sequentially.
  for(idx = 0; idx < 16; idx++)
    palette_idx_table[idx][idx] = idx + offset;

  // Store the blends after that.
  idx += offset;
  for(int a = 0; a < 16; a++)
  {
    for(int b = a+1; b < 16; b++)
    {
      palette_idx_table[a][b] = idx;
      palette_idx_table[b][a] = idx;
      idx++;
    }
  }
}

static void nds_subscreen_scaled_init(void)
{
  int xscale = (int)(320.0/256.0 * 256.0);
  int yscale = (int)(350.0/subscreen_height * 256.0);

  /* Use banks A and B for the ZZT screen. */
  videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_BG);

  /* Use flickered alpha lerp to scale 320x350 to 256x192. */
  /* (Bump the BG up to base 3 (+0xc0000) to make room for the keyboard.) */
  REG_BG2CNT = BG_BMP8_512x512 | BG_BMP_BASE(3) | BG_PRIORITY(0);
  REG_BG2PA  = xscale;
  REG_BG2PB  = 0;
  REG_BG2PC  = 0;
  REG_BG2PD  = yscale;
  REG_BG2X   = 0;
  REG_BG2Y   = subscreen_offset_y;

  REG_BG3CNT = BG_BMP8_512x512 | BG_BMP_BASE(3) | BG_PRIORITY(0);
  REG_BG3PA  = xscale;
  REG_BG3PB  = 0;
  REG_BG3PC  = 0;
  REG_BG3PD  = yscale;
  REG_BG3X   = 0;
  REG_BG3Y   = subscreen_offset_y;

  /* Enable BG2/BG3 blending. */
  REG_BLDCNT   = BLEND_ALPHA | BLEND_SRC_BG2 | BLEND_DST_BG3;
  REG_BLDALPHA = (8 << 8) | 8; /* 50% / 50% */

  update_palette();
  update_screen();
}

static void nds_subscreen_keyboard_init(void)
{
  Keyboard *kb = keyboardGetDefault();

  // BG0: Keyboard 256x512 text, map following tiles (45056 bytes total)
  // Clear the keyboard area before drawing it.
  dmaFillWords(0, BG_MAP_RAM(20), 4096);
  keyboardInit(kb, 0, BgType_Text4bpp, BgSize_T_256x512, 20, 0, true, true);
  bgHide(0);
  kb->scrollSpeed = 7;
  transition.time = 0;
  transition.state = TRANSITION_IN;
}

static void nds_subscreen_keyboard_exit(void)
{
  transition.time = 0;
  transition.state = TRANSITION_OUT;
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
  REG_BG0CNT_SUB  = BG_64x32 | BG_COLOR_16 | BG_MAP_BASE(0) |
                    BG_TILE_BASE(1);
  REG_BG0HOFS_SUB = 0;
  REG_BG0VOFS_SUB = 0;

  /* BG1: background characters. */
  REG_BG1CNT_SUB  = BG_64x32 | BG_COLOR_16 | BG_MAP_BASE(2) |
                    BG_TILE_BASE(1);
  REG_BG1HOFS_SUB = 0;
  REG_BG1VOFS_SUB = 0;

  // By default, pan to the center of the screen.
  focus_x = 640/2;
  focus_y = 350/2;

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
  if(jidx >= sizeof(jitterF4)/sizeof(*jitterF4))
    jidx = 0;

  REG_BG2X = jitterF4[jidx];
  REG_BG2Y = jitterF4[jidx+1] + subscreen_offset_y;
  REG_BG3X = jitterF4[jidx+2];
  REG_BG3Y = jitterF4[jidx+3] + subscreen_offset_y;
  jidx += 4;
}

/* Use HBlank DMA to load row scroll values in for each line. */
void nds_video_rasterhack(void)
{
  /* Prepare DMA transfer. */
  DMA1_CR         = 0;
  REG_BG0VOFS_SUB = scroll_table[0];
  DMA1_SRC        = (u32)(scroll_table + 1);
  DMA1_DEST       = (u32)&REG_BG0VOFS_SUB;
  DMA1_CR         = DMA_DST_FIX | DMA_SRC_INC | DMA_REPEAT | DMA_16_BIT |
                    DMA_START_HBL | DMA_ENABLE | 1;

  DMA2_CR         = 0;
  REG_BG1VOFS_SUB = scroll_table[0];
  DMA2_SRC        = (u32)(scroll_table + 1);
  DMA2_DEST       = (u32)&REG_BG1VOFS_SUB;
  DMA2_CR         = DMA_DST_FIX | DMA_SRC_INC | DMA_REPEAT | DMA_16_BIT |
                    DMA_START_HBL | DMA_ENABLE | 1;
}

// Handle the transition animation.
void nds_video_do_transition(void)
{
  // On transitioning in, use the current screen.  Out, use the last screen.
  enum Subscreen_Mode mode;
  if(transition.state == TRANSITION_NONE)
    return;
  if(transition.state == TRANSITION_IN)
    mode = subscreen_mode;
  else                // TRANSITION_OUT
    mode = last_subscreen_mode;

  if(mode == SUBSCREEN_KEYBOARD)
  {
    if(transition.state == TRANSITION_IN)
      nds_keyboard_scroll_in();
    else
      nds_keyboard_scroll_out();
  }
}

static void nds_keyboard_scroll_in(void)
{
  Keyboard *kb = keyboardGetDefault();
  if(transition.time == 0)
  {
    // Show the background.
    kb->visible = 1;
    bgSetScroll(kb->background, 0, -192);
    bgUpdate();
    bgShow(kb->background);
  }
  else
  {
    // Scroll 80 pixels.
    if(transition.time < 80)
    {
      bgSetScroll(kb->background, 0, -192 + transition.time);
    }
    else
    {
      bgSetScroll(kb->background, 0, kb->offset_y);
      transition.state = TRANSITION_NONE;
    }
    bgUpdate();
  }

  transition.time += kb->scrollSpeed;
}

static void nds_keyboard_scroll_out(void)
{
  Keyboard *kb = keyboardGetDefault();
  if(transition.time == 0)
    kb->visible = 0;

  if(transition.time >= 80)
  {
    // Hide the background.
    bgHide(kb->background);
    transition.state = TRANSITION_NONE;
  }
  else
  {
    // Scroll 80 pixels.
    bgSetScroll(kb->background, 0, kb->offset_y - transition.time);
    bgUpdate();
  }

  transition.time += kb->scrollSpeed;
}

void nds_sleep_check(void)
{
  static boolean asleep = false;

  // Check if we were just woken up.
  if(asleep)
  {
    asleep = false;

    // Wait a bit until returning power.
    while(REG_VCOUNT != 0);
    while(REG_VCOUNT == 0);
    while(REG_VCOUNT != 0);
    powerOn(POWER_ALL);
  }

  // Check if it's time to go to sleep.
  if(keysDown() & KEY_LID)
  {
    // Cancel DMA from raster effects.
    DMA1_CR = 0;
    DMA2_CR = 0;

    // Power everything off.
    powerOff(POWER_ALL);
    asleep = true;
  }
}

static boolean nds_init_video(struct graphics_data *graphics,
 struct config_info *config)
{
  lcdMainOnBottom();

  // Build the palette blend mapping table.
  palette_idx_table_init();

  // Start with scaled mode.
  switch(config->video_ratio)
  {
    case RATIO_CLASSIC_4_3:
    case RATIO_STRETCH:
      subscreen_height = 192;
      break;
    default:
      subscreen_height = 140;
      break;
  }
  subscreen_offset_y = -((192 - subscreen_height) * 320 /* 1/2 * 256 * (1/0.4) */);

  subscreen_mode = SUBSCREEN_SCALED;
  last_subscreen_mode = SUBSCREEN_MODE_INVALID;
  nds_subscreen_scaled_init();

  // Initialize the 1:1 scaled "main" screen.
  nds_mainscreen_init(graphics);

  mouselook = false;
  transition.state = TRANSITION_NONE;
  transition.time = 0;

  // Now that we're initialized, install the vblank handler.
  irqSet(IRQ_VBLANK, nds_on_vblank);

  return true;
}

static boolean nds_check_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  return true;  // stub
}

static boolean nds_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  return true;	// stub
}

// Focus on a given screen position (in pixels up to 640x350).
static void nds_mainscreen_focus(int x, int y)
{
  static int old_x = -1;
  static int old_y = -1;
  int scroll_x, scroll_y, i, ypos, ycounter;
  u16 *sptr;

  if(mouselook)
  {
    // We're mouselooking, don't move the focus.
    return;
  }

  if(x == old_x && y == old_y)
  {
    // Already focused there, quick abort.
    return;
  }
  old_x = x;
  old_y = y;

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
  REG_BG0HOFS_SUB = scroll_x;
  REG_BG1HOFS_SUB = scroll_x;

  // Recalculate the scroll table.
  sptr     = scroll_table;
  ypos     = scroll_y;
  ycounter = scroll_y;
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
}

// Render the scaled screen.
static void nds_render_graph_scaled(struct graphics_data *graphics)
{
  int const WIDTH_IN_PIXELS  = 80 * 4;  // Screen width in pixels
  int const WIDTH_IN_CELLS   = 80;      // Screen width in chars
  int const CHARACTER_HEIGHT = 14;      // Character height in pixels
  int const VRAM_WIDTH       = 512;     // HW surface width in pixels

  struct char_element *text_cell = graphics->text_video;
  u32 *vram_ptr  = (u32*)BG_BMP_RAM(3);
  int chars, lines;

  /* Iterate over every character in text memory. */
  int columns = 0;
  for(chars = 0; chars < 80*25; chars++)
  {
    /* Extract the character, and fore/back colors. */
    int chr = (*text_cell).char_value & 0xFF;
    int bg  = (*text_cell).bg_color   & 0x0F;
    int fg  = (*text_cell).fg_color   & 0x0F;

    // Construct a table mapping charset two-bit pairs to palette entries.
    u32 bg_idx = bg + 16; // Solid colors are offset by 16.
    u32 fg_idx = fg + 16;
    u32 mix_idx = palette_idx_table[bg][fg];
    u32 pal_tbl[4] = {
      bg_idx,   /* 00: bg color    */
      mix_idx,  /* 01: fg+bg blend */
      mix_idx,  /* 10: "         " */
      fg_idx    /* 11: fg color    */
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
  struct char_element *text_start = graphics->text_video;
  struct char_element *text_cell;
  u16 *vram;
  int x, y;

  // Before drawing the screen, check if the focus needs to be updated!
  if(focus_x > -1 && focus_y > -1)
  {
    nds_mainscreen_focus(focus_x, focus_y);
    focus_x = -1;
    focus_y = -1;
  }

  text_start += cell_pan_x + 80*cell_pan_y;
  text_cell = text_start;

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
  // Always render the scaled screen.
  nds_render_graph_scaled(graphics);

  // Always render the 1:1 "main" screen.
  nds_render_graph_1to1(graphics);
}

static void nds_update_palette_entry(struct rgb_color *palette, Uint32 idx)
{
  struct rgb_color color1 = palette[idx];
  int idx2;

  if(idx < 16)
  {
    // Update the mainscreen palette.
    BG_PALETTE_SUB[16*idx + 1] = RGB15(color1.r/8, color1.g/8, color1.b/8);

    // Update the subscreen palette.
    /* Iterate over an entire column of the palette, blending this color
     * with all 16 other colors (including itself). */
    for(idx2 = 0; idx2 < 16; idx2++)
    {
      /* Average the colors while reducing their accuracy to 5 bits. */
      struct rgb_color color2 = palette[idx2];
      u16 r = (color1.r + color2.r) / 16;
      u16 g = (color1.g + color2.g) / 16;
      u16 b = (color1.b + color2.b) / 16;

      BG_PALETTE[ palette_idx_table[idx][idx2] ] = RGB15(r, g, b);
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
 Uint32 x, Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
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

static void nds_remap_charbyte(struct graphics_data *graphics, Uint16 chr,
 Uint8 byte)
{
  if(chr < 256)
  {
    /* Each character is 64 bytes.  Advance the vram pointer. */
    u16* vram = (u16*)BG_TILE_RAM_SUB(1) + 32*chr + 2*byte;
    u8* charset = graphics->charset + 14*chr + byte;
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

static void nds_remap_char_range(struct graphics_data *graphics, Uint16 first,
 Uint16 count)
{
  int stop = first + count;
  int chr;

  if(stop > 256)
    stop = 256;

  for(chr = first; chr < stop; chr++)
    nds_remap_char(graphics, chr);
}

static void nds_focus_pixel(struct graphics_data *graphics, Uint32 x, Uint32 y)
{
  // We want these values to be handled later while render_graph is run.
  focus_x = (int)x;
  focus_y = (int)y;
}

void render_nds_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = nds_init_video;
  renderer->check_video_mode = nds_check_video_mode;
  renderer->set_video_mode = nds_set_video_mode;
  renderer->update_colors = nds_update_colors;
  renderer->resize_screen = nds_resize_screen;
  renderer->remap_char_range = nds_remap_char_range;
  renderer->remap_char = nds_remap_char;
  renderer->remap_charbyte = nds_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = nds_render_graph;
  renderer->render_cursor = nds_render_cursor;
  renderer->render_mouse = nds_render_mouse;
  renderer->sync_screen = nds_sync_screen;
  renderer->focus_pixel = nds_focus_pixel;
}

void nds_subscreen_switch(void)
{
  // Don't switch during a transition.
  if(transition.state != TRANSITION_NONE)
    return;

  // Call appropriate exit function.
  if(subscreen_mode == SUBSCREEN_KEYBOARD)
    nds_subscreen_keyboard_exit();

  // Cycle between modes.
  last_subscreen_mode = subscreen_mode;
  if(++subscreen_mode == SUBSCREEN_MODE_COUNT)
    subscreen_mode = 0;

  // Call the appropriate init function.
  if(subscreen_mode == SUBSCREEN_KEYBOARD)
    nds_subscreen_keyboard_init();
}

void nds_mouselook(boolean enabled)
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
    u16 *img = image.image.data16;
    int ox, oy;
    u16 *gfx;

    videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);

    REG_BG2CNT = BG_BMP8_256x256;
    REG_BG2PA  = 1 << 8;
    REG_BG2PB  = 0;
    REG_BG2PC  = 0;
    REG_BG2PD  = 1 << 8;
    REG_BG2X   = 0;
    REG_BG2Y   = 0;

    memcpy(BG_PALETTE, image.palette, 2*256);

    ox = 128 - image.width/2;
    oy = 96 - image.height/2;
    gfx = (u16*)(0x06000000 + 256*oy + ox);

    for(int row = 0; row < image.height; row++)
    {
      for(int col = 0; col < image.width/2; col++)
        *(gfx++) = *(img++);
      gfx += ox;
    }

    while(!keysDown())
      scanKeys();

    imageDestroy(&image);
  }
}
