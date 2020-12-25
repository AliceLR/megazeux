/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ansi.h"
#include "buffer.h"
#include "edit.h"

#include "../error.h"
#include "../idput.h"
#include "../robot.h"
#include "../util.h"
#include "../world_struct.h"

#include <stdio.h>
#include <time.h>

enum sauce_flags
{
  SAUCE_NO_BLINK      = (1<<0),
  SAUCE_DEFAULT_FONT  = (0<<1),
  SAUCE_8_PIXEL_FONT  = (1<<1),
  SAUCE_9_PIXEL_FONT  = (2<<1),
  SAUCE_DEFAULT_RATIO = (0<<3),
  SAUCE_CLASSIC_RATIO = (1<<3),
  SAUCE_MODERN_RATIO  = (2<<3),
};

static const char ansi_csi_prefix[3] = "\x1b[";

/**
 * Output meta codes to transform the current color into the destination color.
 *
 * @param curr    color to change from.
 * @param dest    color to change to.
 * @param fp      file handle.
 * @return        size of the written meta code (0 if none), or -1 on error.
 */
static ssize_t issue_color_meta_codes(int curr, int dest, FILE *fp)
{
  static const char ega_to_ansi_colors[8] =
  {
    '0', '4', '2', '6', '1', '5', '3', '7'
  };

  int size = 2;
  boolean reset = false;

  if(curr == dest)
    return 0;

  fwrite(ansi_csi_prefix, 1, 2, fp);

  if((curr & 128) && !(dest & 128))
    reset = true;

  if((curr & 8) && !(dest & 8))
    reset = true;

  if(reset)
  {
    fputc('0', fp);
    size++;
  }

  if(dest & 128)
  {
    // Blink
    if(size > 2)
    {
      fputc(';', fp);
      size++;
    }
    fputc('5', fp);
    size++;
  }

  if(dest & 8)
  {
    // Bold
    if(size > 2)
    {
      fputc(';', fp);
      size++;
    }
    fputc('1', fp);
    size++;
  }

  if((curr & 7) != (dest & 7))
  {
    // FG color
    if(size > 2)
    {
      fputc(';', fp);
      size++;
    }
    fputc('3', fp);
    fputc(ega_to_ansi_colors[dest & 7], fp);
    size += 2;
  }

  if((curr & 112) != (dest & 112))
  {
    // BG color
    if(size > 2)
    {
      fputc(';', fp);
      size++;
    }
    fputc('4', fp);
    fputc(ega_to_ansi_colors[(dest >> 4) & 7], fp);
    size += 2;
  }

  if(fputc('m', fp) < 0)
    return -1;

  return size;
}

/**
 * Export a block of ANSi (or plaintext) from the current board. This is
 * assumed to not go over the edges.
 *
 * @param mzx_world   MZX world.
 * @param filename    filename of ANSi or plaintext to write to.
 * @param start_x     position of block to write to ANSi.
 * @param start_y     position of block to write to ANSi.
 * @param width       width of block to write to ANSi.
 * @param height      height of block to write to ANSi.
 * @param text_only   `true`: write plaintext; `false`: write ANSi.
 * @return            `true` on success, otherwise `false`.
 */
boolean export_ansi(struct world *mzx_world, const char *filename,
 enum editor_mode mode, int start_x, int start_y, int width, int height,
 boolean text_only, const char *title, const char *author)
{
  struct board *cur_board = mzx_world->current_board;
  FILE *fp;
  ssize_t color_size;
  int x, y, line_size;
  int curr_color;
  int col, chr;
  int offset;
  int board_width;
  int line_skip;

  fp = fopen_unsafe(filename, "wb");
  if(!fp)
    goto err;

  /**
   * NOTE: DOS versions limited the dimensions to 78x23 here but there's not
   * much reason to do this anymore (terminals are variable width now).
   */

  if(mode == EDIT_VLAYER)
    board_width = mzx_world->vlayer_width;
  else
    board_width = cur_board->board_width;

  offset = start_x + start_y * board_width;
  line_skip = board_width - width;

  curr_color = 7;
  line_size = 0;

  if(!text_only)
  {
    fwrite(ansi_csi_prefix, 1, 2, fp);
    fwrite("0m", 1, 2, fp);
    fwrite(ansi_csi_prefix, 1, 2, fp);
    fwrite("2J", 1, 2, fp);
    line_size += 8;
  }

  // Longest meta code issuable is \x1b[0;5;1;30;40m, or 14 characters.
  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width; x++, offset++)
    {
      switch(mode)
      {
        case EDIT_BOARD:
          chr = get_id_char(cur_board, offset);
          col = get_id_color(cur_board, offset);
          break;

        case EDIT_OVERLAY:
          chr = cur_board->overlay[offset];
          col = cur_board->overlay_color[offset];
          break;

        case EDIT_VLAYER:
          chr = mzx_world->vlayer_chars[offset];
          col = mzx_world->vlayer_colors[offset];
          break;

        default:
          goto err;
      }

      if(!text_only)
      {
        color_size = issue_color_meta_codes(curr_color, col, fp);
        if(color_size < 0)
          goto err;
        line_size += color_size;
      }
      curr_color = col;

      // Print alternate characters for reserved chars.
      switch(chr)
      {
        case '\0':
          chr = ' ';
          break;
        case '\a':
          chr = 249;
          break;
        case '\b':
          chr = 219;
          break;
        case '\t':
          chr = 'o';
          break;
        case '\n':
          chr = 219;
          break;
        case '\r':
          chr = 14;
          break;
        case 27: // ESC
          chr = '-';
          break;
      }
      fputc(chr, fp);
      line_size++;

      // Line too long? (Can't happen in text mode)
      // TODO it's not clear why this is here but this comes from DOS MZX.
      if(!text_only && line_size > 230)
      {
        //Issue save/restore
        fwrite(ansi_csi_prefix, 1, 2, fp);
        fputc('s', fp);
        fputc('\r', fp);
        fputc('\n', fp);
        fwrite(ansi_csi_prefix, 1, 2, fp);
        fputc('u', fp);
        line_size = 3;
      }
    }
    // Issue CR/LF.
    fputc('\r', fp);
    fputc('\n', fp);
    line_size = 0;
    offset += line_skip;
  }

  if(!text_only)
  {
    time_t t;
    struct tm *tm;
    long total_len;
    char datebuf[9];

    color_size = issue_color_meta_codes(curr_color, col, fp);
    if(color_size < 0)
      goto err;

    t = time(NULL);
    tm = localtime(&t);
    strftime(datebuf, ARRAY_SIZE(datebuf), "%Y%m%d", tm);

    total_len = ftell(fp);

    // SAUCE record.
    fputc(0x1A, fp);
    fprintf(fp, "COMNT%-64.64s", // Comment
     "Created with MegaZeux " VERSION VERSION_DATE ".");
    fprintf(fp, "SAUCE00%-35.35s%-20.20s%-20.20s%-8.8s",
      title,
      author,
      "",     // Group
      datebuf
    );
    // FIXME :(
    fputc(total_len & 0xFF, fp);
    fputc((total_len >> 8) & 0xFF, fp);
    fputc((total_len >> 16) & 0xFF, fp);
    fputc((total_len >> 24) & 0xFF, fp);
    fputc(1, fp);                     // DataType (Character)
    fputc(1, fp);                     // FileType (ANSi)
    fputc(width & 0xFF, fp);          // TInfo1 (width in characters)
    fputc((width >> 8) & 0xFF, fp);
    fputc(height & 0xFF, fp);         // TInfo2 (height in rows)
    fputc((height >> 8) & 0xFF, fp);
    fputc(0, fp);                     // TInfo3
    fputc(0, fp);
    fputc(0, fp);                     // TInfo4
    fputc(0, fp);
    fputc(1, fp);                     // Comment lines
    fputc(SAUCE_NO_BLINK | SAUCE_8_PIXEL_FONT | SAUCE_MODERN_RATIO, fp); // TFlags
    fprintf(fp, "%-22.22s", "IBM EGA"); // TInfoS
  }
  fclose(fp);
  return true;

err:
  /* FIXME */
  if(text_only) error("Error exporting text",1,24,0x1401);
  else error("Error exporting ANSi",1,24,0x0F01);

  if(fp)
    fclose(fp);
  return false;
}

enum ansi_retvals
{
  ANSI_EOF    = -1,
  ANSI_ESCAPE = -2,
  ANSI_ERROR  = -3,
};

enum ansi_erase
{
  ANSI_NO_ERASE       = -1,
  ANSI_ERASE_TO_END   = 0,
  ANSI_ERASE_TO_START = 1,
  ANSI_ERASE_ALL      = 2,
};

struct ansi_data
{
  int color;
  int x;
  int y;
  int width;
  int height;
  int saved_x;
  int saved_y;
  enum ansi_erase erase_display;
  enum ansi_erase erase_line;
};

static const struct ansi_data default_state =
{
  7, // Color
  0, // x
  0, // y
  0, // width
  0, // height,
  0, // saved_x
  0, // saved_y,
  ANSI_NO_ERASE,
  ANSI_NO_ERASE
};

/**
 * Apply an SGR (Select Graphic Rendition) parameter. Only colors are handled.
 *
 * @param ansi    ANSi data.
 * @param param   SGR parameter to apply.
 */
static void apply_ansi_sgr_code(struct ansi_data *ansi, int param)
{
  static const int ansi_to_ega_color[8] =
  {
    0, 4, 2, 6, 1, 5, 3, 7
  };

  switch(param)
  {
    case 0:
      ansi->color = 7;
      break;

    case 1:
      // Bold enable.
      ansi->color |= 8;
      break;

    case 5:
      // Blink enable.
      ansi->color |= 128;
      break;

    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
      // FG color.
      ansi->color = (ansi->color & 248) | ansi_to_ega_color[param - 30];
      break;

    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
      // BG color.
      ansi->color = (ansi->color & 143) | (ansi_to_ega_color[param - 40] << 4);
      break;
  }
}

/**
 * Read a single char or escape code from an ANSI file.
 *
 * @param ansi  ANSi data.
 * @param fp    file handle.
 * @return      -1 for EOF, -2 for escape code, -3 for error, otherwise a char.
 */
static int read_ansi(struct ansi_data *ansi, FILE *fp)
{
  int sym = fgetc(fp);
  if(feof(fp))
    return ANSI_EOF;

  if(sym == '\r')
  {
    ansi->x = 0;
    return ANSI_ESCAPE;
  }
  else

  if(sym == '\n')
  {
    ansi->y++;
    return ANSI_ESCAPE;
  }
  else

  if(sym == 0x1A)
  {
    return ANSI_EOF;
  }
  else

  // ANSi escape sequence.
  if(sym == 0x1B)
  {
    sym = fgetc(fp);
    if(sym == '[')
    {
      // CSI sequence.
      int param[32] =
      {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      };
      int cur_param = 0;
      int i;

      sym = fgetc(fp);
      while(1)
      {
        if(sym == ';')
        {
          // End param.
          if(param[cur_param] < 0)
            return ANSI_ERROR;

          if(cur_param >= (int)ARRAY_SIZE(param))
            return ANSI_ERROR;

          cur_param++;
          sym = fgetc(fp);
          continue;
        }
        else

        if(sym == 'm')
        {
          for(i = 0; i <= cur_param; i++)
            apply_ansi_sgr_code(ansi, param[i]);

          return ANSI_ESCAPE;
        }
        else

        if(sym == 's')
        {
          // Save position.
          ansi->saved_x = ansi->x;
          ansi->saved_y = ansi->y;
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'u')
        {
          // Restore position.
          ansi->x = ansi->saved_x;
          ansi->y = ansi->saved_y;
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'J')
        {
          // Erase display.
          param[0] = CLAMP(param[0], 0, 2);

          ansi->erase_display = param[0];
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'K')
        {
          // Erase line.
          param[0] = CLAMP(param[0], 0, 2);

          ansi->erase_line = param[0];
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'A' || sym == 'F')
        {
          // Move up (F is non-standard).
          if(sym == 'F')
            ansi->x = 0;

          if(param[0] <= 0)
            param[0] = 1;

          if(ansi->y >= param[0])
            ansi->y -= param[0];
          else
            ansi->y = 0;

          return ANSI_ESCAPE;
        }
        else

        if(sym == 'B' || sym == 'E')
        {
          // Move down (E is non-standard).
          if(sym == 'E')
            ansi->x = 0;

          if(param[0] <= 0)
            param[0] = 1;

          ansi->y += param[0];
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'C')
        {
          // Move right.
          if(param[0] <= 0)
            param[0] = 1;

          ansi->x += param[0];
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'D')
        {
          // Move left.
          if(param[0] <= 0)
            param[0] = 1;

          if(ansi->x >= param[0])
            ansi->x -= param[0];
          else
            ansi->x = 0;

          return ANSI_ESCAPE;
        }
        else

        if(sym == 'G')
        {
          // Cursor column (non-standard).
          if(param[0] <= 0)
            param[0] = 1;

          ansi->x = param[0] - 1;
          return ANSI_ESCAPE;
        }

        if(sym == 'H' || sym == 'f')
        {
          // Cursor position.
          if(param[0] <= 0)
            param[0] = 1;
          if(param[1] <= 0)
            param[1] = 1;

          ansi->x = param[0] - 1;
          ansi->y = param[1] - 1;
          return ANSI_ESCAPE;
        }
        else

        // Ignore unsupported CSI sequences.
        if((sym < '0') || (sym > '9'))
          return ANSI_ESCAPE;

        // Read parameter.
        param[cur_param] = 0;
        while(sym >= '0' && sym <= '9')
        {
          param[cur_param] = (param[cur_param] * 10) + (sym - '0');
          sym = fgetc(fp);
        }
      }
    }
  }
  return sym;
}

/**
 * Test an ANSi file for correctness and determine its output dimensions.
 * The expected output dimensions are required to correctly load the ANSi later.
 *
 * @param filename    filename of ANSi or plaintext file.
 * @param wrap_width  force line wrap after this many columsn (<=0 for none).
 * @param width       pointer to store width at.
 * @param height      pointer to store height at.
 * @return            `true` if the ANSi file is valid, otherwise `false`.
 */
boolean validate_ansi(const char *filename, int wrap_width, int *width, int *height)
{
  struct ansi_data ansi = default_state;
  char magic[7];
  long file_len;
  FILE *fp;
  int chr;

  trace("--ANSI-- validate_ansi\n");

  *width = -1;
  *height = -1;

  fp = fopen_unsafe(filename, "rb");
  if(!fp)
    goto err;

  setvbuf(fp, NULL, _IOFBF, 128);
  file_len = ftell_and_rewind(fp);

  // Check for a SAUCE record. If found and valid, prefer its requested
  // wrap width over the provided wrap width.
  if(file_len > 128)
  {
    fseek(fp, file_len - 128, SEEK_SET);

    if(fread(magic, 7, 1, fp) && !strncmp(magic, "SAUCE00", 7))
    {
      int data_type;
      int file_type;
      int sauce_width;
      int sauce_height;

      // Skip optional fields.
      fseek(fp, 35+20+20+8+4, SEEK_CUR);

      // FIXME :(
      data_type = fgetc(fp);
      file_type = fgetc(fp);
      sauce_width = fgetc(fp) | (fgetc(fp) << 8);
      sauce_height = fgetc(fp) | (fgetc(fp) << 8);

      if(!feof(fp) && data_type == 1 /* Character */ && file_type == 1 /* ANSi */)
      {
        ansi.width = sauce_width;
        ansi.height = sauce_height;
        trace("--ANSI-- SAUCE width=%d, height=%d\n", ansi.width, ansi.height);

        if(sauce_width && (wrap_width < 1 || wrap_width > sauce_width))
        {
          wrap_width = sauce_width;
          trace("--ANSI-- using SAUCE width to wrap ANSi.\n");
        }
        else

        if(!sauce_width && wrap_width < 1)
        {
          wrap_width = 80;
          trace("--ANSI-- SAUCE record with width 0--using default width "
           "of 80 to wrap ANSi.\n");
        }
      }
    }
  }
  rewind(fp);

  while(1)
  {
    chr = read_ansi(&ansi, fp);
    if(chr == ANSI_EOF)
      break;

    if(chr == ANSI_ERROR)
      goto err;

    if(wrap_width > 0 && ansi.x >= wrap_width)
    {
      ansi.x = 0;
      ansi.y++;
    }

    if(chr >= 0)
    {
      if(ansi.x + 1 > ansi.width)
        ansi.width = ansi.x + 1;
      if(ansi.y + 1 > ansi.height)
        ansi.height = ansi.y + 1;

      ansi.x++;
    }
  }
  trace("--ANSI-- scan width=%d, height=%d\n", ansi.width, ansi.height);

  fclose(fp);
  *width = ansi.width;
  *height = ansi.height;
  return true;

err:
  if(fp)
    fclose(fp);

  // FIXME
  error("Error importing ANSi",1,24,0x1901);
  return false;
}

/**
 * Import an ANSi or text graphic into the board as the selected type.
 * Undo history should be managed separately for this block. This function
 * clips the ANSi to the size of the destination board/vlayer.
 *
 * @param mzx_world   MZX world.
 * @param filename    filename of ANSi or plaintext file.
 * @param mode        location to load ANSi to (board, overlay, or vlayer).
 * @param start_x     position to load ANSi to.
 * @param start_y     position to load ANSi to.
 * @param width       width of the ANSi to load (see `validate_ansi`).
 *                    Note this isn't the width of the area to load to.
 * @param height      height of the ANSi to load (see `validate_ansi`).
 *                    Note this isn't the height of the area to load to.
 * @param convert_id  thing to load ANSi chars as (board only).
 * @return            `true` on success, otherwise `false`.
 */
boolean import_ansi(struct world *mzx_world, const char *filename,
 enum editor_mode mode, int start_x, int start_y, int width, int height,
 enum thing convert_id)
{
  struct ansi_data ansi = default_state;
  struct buffer_info ansi_buffer;
  struct buffer_info clear_buffer;
  struct board *cur_board = mzx_world->current_board;
  int board_width = cur_board->board_width;
  int board_height = cur_board->board_height;
  int wrap_width = width;
  boolean need_erase = true;
  FILE *fp;
  int chr;

  // Bound the area to the board (or vlayer).
  if(mode == EDIT_VLAYER)
  {
    board_width = mzx_world->vlayer_width;
    board_height = mzx_world->vlayer_height;
  }

  if(start_x >= board_width || start_y >= board_height ||
   start_x < 0 || start_y < 0 || width < 1 || height < 1)
    return false;

  if(start_x + width > board_width)
    width = board_width - start_x;

  if(start_y + height > board_height)
    height = board_height - start_y;

  memset(&ansi_buffer, 0, sizeof(struct buffer_info));
  memset(&clear_buffer, 0, sizeof(struct buffer_info));

  if(!is_storageless(convert_id))
    convert_id = CUSTOM_BLOCK;

  ansi_buffer.id = convert_id;
  ansi_buffer.color = ansi.color;
  clear_buffer.id = convert_id;
  clear_buffer.color = 7;
  clear_buffer.param = ' ';

  fp = fopen_unsafe(filename, "rb");
  if(!fp)
    goto err;

  setvbuf(fp, NULL, _IOFBF, 1024);

  while(1)
  {
    if(need_erase)
    {
      // MZX ANSI files contain an initial display clear but many others do not.
      // Inject an erase display escape at the start of the file so the drawing
      // area is guaranteed to be filled with the selected ID.
      ansi.erase_display = ANSI_ERASE_ALL;
      chr = ANSI_ESCAPE;
      need_erase = false;
    }
    else
      chr = read_ansi(&ansi, fp);

    if(chr == ANSI_EOF)
      break;

    if(chr == ANSI_ERROR) // Shouldn't happen.
      goto err;

    if(wrap_width > 0 && ansi.x >= wrap_width)
    {
      ansi.x = 0;
      ansi.y++;
    }

    if(chr == ANSI_ESCAPE)
    {
      if(ansi.erase_display != ANSI_NO_ERASE)
      {
        int x1 = 0;
        int x2 = width - 1;
        int y1 = 0;
        int y2 = height - 1;

        switch(ansi.erase_display)
        {
          case ANSI_ERASE_TO_START:
            x2 = ansi.x;
            y2 = ansi.y;
            break;
          case ANSI_ERASE_TO_END:
            x1 = ansi.x;
            y1 = ansi.y;
            break;
          default:
            ansi.x = 0;
            ansi.y = 0;
            break;
        }

        while(1)
        {
          if(x1 < width && y1 < height)
          {
            place_current_at_xy(mzx_world, &clear_buffer,
             start_x + x1, start_y + y1, mode, NULL);
          }

          if(x1 >= x2)
          {
            if(y1 >= y2)
              break;
            x1 = 0;
            y1++;
          }
          else
            x1++;
        }
      }
      else

      if(ansi.erase_line != ANSI_NO_ERASE)
      {
        int x1 = (ansi.erase_line == ANSI_ERASE_TO_END) ? ansi.x : 0;
        int x2 = (ansi.erase_line == ANSI_ERASE_TO_START) ? ansi.x : width - 1;

        while(x1 <= x2)
        {
          if(x1 < width && ansi.y < height)
            place_current_at_xy(mzx_world, &clear_buffer,
             start_x + x1, start_y + ansi.y, mode, NULL);
          x1++;
        }
      }

      ansi_buffer.color = ansi.color;
      ansi.erase_line = ANSI_NO_ERASE;
      ansi.erase_display = ANSI_NO_ERASE;
    }
    else
    {
      if(ansi.x < width && ansi.y < height)
      {
        ansi_buffer.param = chr;

        if(place_current_at_xy(mzx_world, &ansi_buffer,
         start_x + ansi.x, start_y + ansi.y, mode, NULL) == -1)
        {
          goto err; // Shouldn't ever happen--can't place storage objects.
        }
      }
      ansi.x++;
    }
  }
  fclose(fp);
  return true;

err:
  if(fp)
    fclose(fp);

  // FIXME
  error("Error importing ANSi",1,24,0x1901);
  return false;
}
