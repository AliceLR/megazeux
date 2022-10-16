/* MegaZeux
 *
 * Copyright (C) 2022 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_gif.h"

#if defined(_MSC_VER) && _MSC_VER < 1400
static void gifnodbg(...) {}
#else
#define gifnodbg(...) do {} while(0)
#endif

#if 0
#define gifdbg(...) \
 do { fprintf(stderr, "GIF: " __VA_ARGS__); fflush(stderr); } while(0)
#else
#define gifdbg gifnodbg
#endif

#if 0
#define giflzwdbg gifdbg
#else
#define giflzwdbg gifnodbg
#endif

#ifndef GIF_RESTRICT
#if (defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))) || \
    (defined(_MSC_VER) && (_MSC_VER >= 1400)) || \
    (defined(__WATCOMC__) && (__WATCOMC__ >= 1250) && !defined(__cplusplus))
#define GIF_RESTRICT __restrict
#else
#define GIF_RESTRICT
#endif
#endif

#undef GIF_MAX
#undef GIF_MIN
#define GIF_MAX(a,b) ((a) > (b) ? (a) : (b))
#define GIF_MIN(a,b) ((a) < (b) ? (a) : (b))

static inline uint16_t gif_u16(const void *_buf)
{
  const uint8_t *buf = (const uint8_t *)_buf;
  return buf[0] | (buf[1] << 8);
}

static inline uint32_t gif_u32(const void *_buf)
{
  const uint8_t *buf = (const uint8_t *)_buf;
  return buf[0] | (buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

const char *gif_error_string(enum gif_error err)
{
  switch(err)
  {
    case GIF_OK:
      return "ok";
    case GIF_IO:
      return "IO error";
    case GIF_MEM:
      return "out of memory";
    case GIF_INVALID:
      return "invalid stream";
    case GIF_SIGNATURE:
      return "not a GIF";
  }
  return "unknown";
}


static const struct gif_graphics_control default_control =
{
  1,                  /* do not dispose */
  0,                  /* no user input */
  0,                  /* no delay */
  -1                  /* no transparent color */
};

struct gif_lzw_node
{
#define GIF_MAX_CODES 0x1000
#define GIF_NO_CODE 0x1000
  uint16_t len;
  uint16_t prev;
  uint8_t chr;
};

struct gif_bitstream
{
  uint8_t *in;
  unsigned in_left;
  unsigned bits_left;
  size_t bits;
};

static gif_bool gif_fill_buffer(struct gif_bitstream *b)
{
  unsigned bytes_free = (sizeof(size_t) > 4 ? 64 - b->bits_left : 32 - b->bits_left) >> 3;
  unsigned to_read = GIF_MIN(bytes_free, b->in_left);

  if(to_read >= 4 && sizeof(size_t) > 4) /* rarely useful for 32-bit buffers */
  {
    b->bits |= (size_t)gif_u32(b->in) << b->bits_left;
    b->bits_left += 32;
    b->in_left -= 4;
    b->in += 4;
    return GIF_TRUE;
  }
  if(to_read >= 2)
  {
    b->bits |= (size_t)gif_u16(b->in) << b->bits_left;
    b->bits_left += 16;
    b->in_left -= 2;
    b->in += 2;
    return GIF_TRUE;
  }
  if(to_read >= 1)
  {
    b->bits |= (size_t)*(b->in) << b->bits_left;
    b->bits_left += 8;
    b->in_left--;
    b->in++;
    return GIF_TRUE;
  }
  return GIF_FALSE;
}

static uint16_t gif_read_code(struct gif_bitstream *b, unsigned code_len)
{
  uint16_t code;
  if(b->bits_left < code_len)
    if(!gif_fill_buffer(b) || b->bits_left < code_len)
      return GIF_NO_CODE;

  code = b->bits & ((1u << code_len) - 1);
  b->bits >>= code_len;
  b->bits_left -= code_len;
  return code;
}

/**
 * Unpack a GIF image. GIF images are stored using a LZW variant split into
 * blocks of up to 255 bytes. The minimum allowed bit width is 3 and the
 * maximum allowed bit width is 12. See GIF89a Appendix F.
 */
static enum gif_error gif_decode(uint8_t *out, struct gif_lzw_node **_tree,
 struct gif_image *layer, void *handle, const gif_read_function readfn)
{
  struct gif_lzw_node *tree = *_tree;
  struct gif_lzw_node *current;
  struct gif_bitstream b;
  uint8_t *out_end = out + layer->width * layer->height;
  uint8_t *pos;
  uint8_t buf[255];
  uint8_t size;
  uint8_t first_chr = 0;
  uint8_t code_len_init;
  unsigned prev_code = GIF_NO_CODE;
  unsigned next_code;
  unsigned clear_code;
  unsigned end_code;
  unsigned code_len;
  unsigned i;

  if(!tree)
  {
    *_tree = tree = (struct gif_lzw_node *)calloc(1<<12, sizeof(struct gif_lzw_node));
    if(!tree)
      return GIF_MEM;
  }
  else
    memset(tree, 0, (1<<12) * sizeof(struct gif_lzw_node));

  if(readfn(&code_len_init, 1, handle) < 1)
    return GIF_IO;

  code_len_init++;
  if(code_len_init < 2 || code_len_init > 12)
    return GIF_INVALID;

  code_len = code_len_init;
  for(i = 0; i < (1u << (code_len - 1)); i++)
  {
    tree[i].prev = GIF_NO_CODE;
    tree[i].len = 1;
    tree[i].chr = i;
  }
  clear_code = i;
  end_code = i + 1;
  next_code = i + 2;
  for(; i < GIF_MAX_CODES; i++)
    tree[i].prev = GIF_NO_CODE;

  b.bits_left = 0;
  b.bits = 0;

  giflzwdbg("  Depack\t\t\t code_len:%d\n", code_len);

  while(1)
  {
    if(readfn(&size, 1, handle) < 1)
      return GIF_IO;
    giflzwdbg("  BLOCK size:%d\n", size);
    if(size == 0)
      break;

    if(readfn(buf, size, handle) < size)
      return GIF_IO;

    b.in = buf;
    b.in_left = size;

    while(1)
    {
      uint16_t code = gif_read_code(&b, code_len);
      int kwkwk = 0;
      giflzwdbg("    %d\n", code);
      if(code > next_code || code == end_code)
        break;

      /* NOTE: clear is supposed to reset the entire table, so no encoder
       * should rely on stale codes after a reset and the current length
       * optimization should be safe. If a file that uses these is found
       * this should be revised. */
      if(code == clear_code)
      {
        code_len = code_len_init;
        next_code = end_code + 1;
        prev_code = GIF_NO_CODE;
        first_chr = 0;
        continue;
      }

      /* Add (kwkwk) */
      if(code == next_code && prev_code < GIF_NO_CODE && next_code < GIF_MAX_CODES)
      {
        giflzwdbg("      add:%d prev:%d prevlen:%d (kwkwk)\n",
         next_code, prev_code, tree[prev_code].len);

        current = &tree[next_code++];
        current->prev = prev_code;
        current->len = tree[prev_code].len + 1;
        current->chr = first_chr;
        kwkwk = 1;
      }

      /* Emit */
      current = &tree[code];
      out += current->len;
      pos = out;
      giflzwdbg("      len:%d\n", current->len);
      if(out > out_end)
      {
        giflzwdbg("    early end of buffer\n");
        return GIF_INVALID;
      }

      while(1)
      {
        *(--pos) = first_chr = current->chr;
        if(current->prev >= GIF_NO_CODE)
          break;
        current = &tree[current->prev];
      }

      /* Add (normal) */
      if(!kwkwk && prev_code < GIF_NO_CODE && next_code < GIF_MAX_CODES)
      {
        giflzwdbg("      add:%d prev:%d prevlen:%d\n",
         next_code, prev_code, tree[prev_code].len);

        current = &tree[next_code++];
        current->prev = prev_code;
        current->len = tree[prev_code].len + 1;
        current->chr = first_chr;
      }

      /* Expand after adding */
      if(next_code >= (1u << code_len))
      {
        if(code_len < 12)
        {
          code_len++;
          giflzwdbg("    code len is now: %d\n", code_len);
        }
        else
          giflzwdbg("    out of codes\n");
      }

      prev_code = code;
    }

    if(b.in_left)
    {
      giflzwdbg("  unexpected end code or invalid code (%u bits left in buffer,"
       "%u bytes left in buffer, %zd left in output)\n",
       b.bits_left, b.in_left, out_end - out);
      return GIF_INVALID;
    }

    giflzwdbg("  bits_left:%u\n", b.bits_left);
  }
  return GIF_OK;
}

/**
 * Deinterlace an image that has been stored interlaced. Rows are stored in the
 * following format (See GIF89a Appendix E):
 *
 * 1) Every 8th row starting from row 0.
 * 2) Every 8th row starting from row 4.
 * 3) Every 4th row starting from row 2.
 * 4) Every 2nd row starting from row 1.
 */
static void gif_deinterlace(uint8_t * GIF_RESTRICT out, const uint8_t *in,
 unsigned width, unsigned height)
{
  uint8_t *pos;
  uint8_t *end = out + (width * height);
  unsigned pitch = width * 8;

  for(pos = out; pos < end; pos += pitch, in += width)
    memcpy(pos, in, width);

  for(pos = out + width * 4; pos < end; pos += pitch, in += width)
    memcpy(pos, in, width);

  pitch = width * 4;
  for(pos = out + width * 2; pos < end; pos += pitch, in += width)
    memcpy(pos, in, width);

  pitch = width * 2;
  for(pos = out + width; pos < end; pos += pitch, in += width)
    memcpy(pos, in, width);
}

static unsigned gif_next_power_of_two(unsigned sz)
{
  sz |= sz >> 1;
  sz |= sz >> 2;
  sz |= sz >> 4;
  sz |= sz >> 8;
  sz |= sz >> 16;
  return sz + 1;
}

static unsigned gif_ptr_size(unsigned sz)
{
  return sz * sizeof(void *);
}

#define RESIZE(type, ptr, alloc, target, size_fn) \
  do { \
    unsigned tmp = GIF_MAX((alloc), (target)); \
    unsigned new_size = gif_next_power_of_two(tmp); \
    unsigned a_size = size_fn(new_size); \
    void *buf; \
    if(new_size < tmp || a_size < new_size) \
      return GIF_MEM; \
    buf = realloc((ptr), a_size); \
    if(!buf) \
      return GIF_MEM; \
    ptr = (type)buf; \
    alloc = new_size; \
  } while(0)

#define RESIZE_LIST(type, ptr, alloc, target) \
 RESIZE(type, ptr, alloc, target, gif_ptr_size)

static struct gif_color_table *gif_alloc_color_table(unsigned num_entries)
{
  size_t sz = num_entries * sizeof(struct gif_color);
  sz += sizeof(struct gif_color_table);

  return (struct gif_color_table *)malloc(sz);
}

static unsigned gif_add_image_size(unsigned size)
{
  return GIF_MAX(sizeof(struct gif_image),
   offsetof(struct gif_image, pixels) + size + 1);
}

static enum gif_error gif_add_image(struct gif_info *gif, unsigned size,
 struct gif_image **_layer)
{
  struct gif_image *layer;

  if(gif->num_images >= gif->num_images_alloc)
  {
    RESIZE_LIST(struct gif_image **, gif->images, gif->num_images_alloc,
     gif->num_images + 1);
  }

  layer = (struct gif_image *)calloc(1, gif_add_image_size(size));
  if(!layer)
    return GIF_MEM;

  layer->length_alloc = size;

  gif->images[gif->num_images++] = layer;
  *_layer = layer;
  return GIF_OK;
}

static enum gif_error gif_add_image_append(struct gif_image **_layer,
 uint8_t *data, uint8_t length)
{
  struct gif_image *layer = *_layer;
  if(layer->length_alloc - layer->length < length)
  {
    RESIZE(struct gif_image *, layer, layer->length_alloc,
     layer->length + length, gif_add_image_size);
    *_layer = layer;
  }
  memcpy(layer->pixels + layer->length, data, length);
  layer->length += length;
  layer->pixels[layer->length] = '\0';
  return GIF_OK;
}

static unsigned gif_add_comment_size(unsigned size)
{
  return GIF_MAX(sizeof(struct gif_comment),
   offsetof(struct gif_comment, comment) + size + 1);
}

static enum gif_error gif_add_comment(struct gif_info *gif, uint8_t length)
{
  struct gif_comment *c;

  if(gif->num_comments >= gif->num_comments_alloc)
  {
    RESIZE_LIST(struct gif_comment **, gif->comments, gif->num_comments_alloc,
     gif->num_comments + 1);
  }

  c = (struct gif_comment *)malloc(gif_add_comment_size(length));
  if(!c)
    return GIF_MEM;

  c->length = 0;
  c->length_alloc = length;
  c->comment[0] = '\0';

  gif->comments[gif->num_comments++] = c;
  return GIF_OK;
}

static enum gif_error gif_add_comment_append(struct gif_comment **_comment,
 uint8_t *data, uint8_t length)
{
  struct gif_comment *c = *_comment;
  if(c->length_alloc - c->length < length)
  {
    RESIZE(struct gif_comment *, c, c->length_alloc, c->length + length,
     gif_add_comment_size);
    *_comment = c;
  }
  memcpy(c->comment + c->length, data, length);
  c->length += length;
  c->comment[c->length] = '\0';
  return GIF_OK;
}

static enum gif_error gif_add_appdata(struct gif_info *gif, uint8_t *appdata, uint8_t length)
{
  struct gif_appdata *a;

  if(gif->num_appdata >= gif->num_appdata_alloc)
  {
    RESIZE_LIST(struct gif_appdata **, gif->appdata, gif->num_appdata_alloc,
     gif->num_appdata + 1);
  }

  a = (struct gif_appdata *)malloc(sizeof(struct gif_appdata) + length);
  if(!a)
    return GIF_MEM;

  a->length = length;
  memcpy(a->appdata, appdata, length);
  a->appdata[length] = '\0';

  gif->appdata[gif->num_appdata++] = a;
  return GIF_OK;
}

enum gif_error gif_read(struct gif_info *gif, void *handle,
 const gif_read_function readfn, gif_bool skip_signature)
{
  struct gif_lzw_node *unpack_tree = NULL;
  struct gif_color_table *table;
  uint8_t buffer[256];
  enum gif_error ret;
  unsigned current = 0;
  int has_control = 0;

  memset(gif, 0, sizeof(*gif));

  /* Header */
  if(!skip_signature)
  {
    if(readfn(gif->signature, 3, handle) < 3)
      return GIF_IO;
    if(readfn(gif->signature, 3, handle) < 3 || memcmp(gif->signature, "GIF", 3))
      return GIF_SIGNATURE;
  }

  if(readfn(gif->version, 3, handle) < 3)
    return GIF_IO;

  if(gif->version[0] < '0' || gif->version[0] > '9' ||
   gif->version[1] < '0' || gif->version[1] > '9' ||
   gif->version[2] < 'a' || gif->version[2] > 'z')
    return GIF_SIGNATURE;

  gifdbg("GIF%3.3s\n", gif->version);

  /* Logical Screen Descriptor */
  if(readfn(buffer, 7, handle) < 7)
    return GIF_IO;

  gif->width          = gif_u16(buffer + 0);
  gif->height         = gif_u16(buffer + 2);
  gif->component_res  = ((buffer[4] & 0x70) >> 4) + 1;
  gif->bg_color       = buffer[5];
  gif->pixel_ratio    = buffer[6];

  gifdbg("  Logical Screen\t\t w:%d h:%d rgbres:%d bgcolor:%d pixelratio:%d\n",
   gif->width, gif->height, gif->component_res, gif->bg_color, gif->pixel_ratio);

  /* Global Color Table */
  if(buffer[4] & 0x80)
  {
    unsigned num_entries = 2 << (buffer[4] & 7);

    table = gif_alloc_color_table(num_entries);
    if(!table)
      return GIF_MEM;

    table->is_sorted = (buffer[4] & 0x08) >> 3;
    table->num_entries = num_entries;
    gif->global_color_table = table;

    gifdbg("  Global Color Table\t sorted:%d entries:%d\n",
     table->is_sorted, table->num_entries);

    if(readfn(table->entries, num_entries * 3, handle) < num_entries * 3)
    {
      ret = GIF_IO;
      goto error;
    }
  }

  /* All other blocks */
  while(1)
  {
    if(readfn(buffer, 1, handle) < 1)
      break;

    switch(buffer[0])
    {
      case 0x21:        /* Extension Block */
      {
        unsigned type;
        unsigned sz;
        if(readfn(buffer, 2, handle) < 1)
        {
          ret = GIF_IO;
          goto error;
        }
        type = buffer[0];
        sz = buffer[1];

        gifdbg("Extension\t\t\t type:%u\n", type);

        for(; sz != 0; sz = buffer[sz])
        {
          if(readfn(buffer, sz + 1, handle) < sz + 1)
          {
            ret = GIF_IO;
            goto error;
          }

          switch(type)
          {
            default:
            {
              gifdbg("  ignoring subblock\t size:%u\n", sz);
              break;
            }

            case 0xF9:  /* Graphics Control Extension */
            {
              if(sz < 4)
                continue;

              has_control = 1;
              gif->control.disposal_method    = (buffer[0] & 0x1C) >> 2;
              gif->control.input_required     = (buffer[0] & 0x02) >> 1;
              gif->control.delay_time         = gif_u16(buffer + 1);
              gif->control.transparent_color  = (buffer[0] & 1) ? buffer[3] : -1;

              gifdbg("  Graphics Control\t\t size:%u disp:%d input:%d delay:%d tr:%d\n",
               sz, gif->control.disposal_method, gif->control.input_required,
               gif->control.delay_time, gif->control.transparent_color);

              type += 0x100; // Only read one.
              break;
            }

            case 0xFE:  /* Comment */
            {
              gifdbg("  Comment\t\t\t size:%u\n", sz);
              current = gif->num_comments;
              if(gif_add_comment(gif, sz) != GIF_OK)
              {
                type = 0; // Skip everything else
                break;
              }
            }
            /* fall-through */

            case 0x1FE: /* Comment (data blocks) */
            {
              if(gif_add_comment_append(&(gif->comments[current]), buffer, sz) != GIF_OK)
                type = 0; // Skip everything else

              gifdbg("    text: %*.*s\n", (int)sz, (int)sz, buffer);
              break;
            }

            case 0xFF:  /* Application */
            {
              if(sz < 11)
              {
                type = 0; // Skip everything else
                break;
              }
              memcpy(gif->application, buffer, 8);
              memcpy(gif->application_code, buffer + 8, 3);
              gif->application[8] = '\0';

              gifdbg("  Application\t\t size:%u application:%s code:%02x%02x%02xh\n",
               sz, gif->application, buffer[8], buffer[9], buffer[10]);

              type += 0x100;
              break;
            }

            case 0x1FF: /* Application (data blocks) */
            {
              gifdbg("  Application data\t\t size:%u\n", sz);
              gif_add_appdata(gif, buffer, sz); // Ignore failures.
              break;
            }

            case 0x01:  /* Plain Text */
            {
              struct gif_image *layer;
              if(sz < 12)
              {
                type = 0; // Skip everything else
                continue;
              }
              current = gif->num_images;

              ret = gif_add_image(gif, 32, &layer);
              if(ret != GIF_OK)
                continue;

              layer->type         = GIF_PLAINTEXT;
              layer->left         = gif_u16(buffer + 0);
              layer->top          = gif_u16(buffer + 2);
              layer->width        = gif_u16(buffer + 4);
              layer->height       = gif_u16(buffer + 6);
              layer->char_width   = buffer[8];
              layer->char_height  = buffer[9];
              layer->fg_color     = buffer[10];
              layer->bg_color     = buffer[11];

              layer->control = has_control ? gif->control : default_control;
              has_control = 0;

              gifdbg("  Plain text\t\t size:%u width:%u height:%u\n", sz,
               layer->width, layer->height);
              gifdbg("\t\t\t\t left:%d top:%d charw:%d charh:%d fg:%d bg:%d\n",
               layer->left, layer->top, layer->char_width, layer->char_height,
               layer->fg_color, layer->bg_color);
              type += 0x100;
              break;
            }

            case 0x101:  /* Plain Text (data blocks) */
            {
              if(current >= gif->num_images || gif->images[current]->type != GIF_PLAINTEXT)
                continue;

              gif_add_image_append(&(gif->images[current]), buffer, sz);
              gifdbg("  Plain text data\t\t size:%u\n", sz);
              break;
            }
          }
        }
        break;
      }

      case 0x2C:        /* Image Descriptor */
      {
        struct gif_image *layer;
        uint8_t *dest;
        unsigned width;
        unsigned height;

        if(readfn(buffer, 9, handle) < 9)
        {
          ret = GIF_IO;
          goto error;
        }

        width = gif_u16(buffer + 4);
        height = gif_u16(buffer + 6);

        gifdbg("Image Descriptor\t\t width:%u height:%u\n", width, height);

        ret = gif_add_image(gif, width * height, &layer);
        if(ret != GIF_OK)
          goto error;

        layer->type           = GIF_IMAGE;
        layer->left           = gif_u16(buffer + 0);
        layer->top            = gif_u16(buffer + 2);
        layer->width          = width;
        layer->height         = height;
        layer->is_interlaced  = (buffer[8] & 0x40) >> 6;

        gifdbg("\t\t\t\t left:%d top:%d interlaced:%d\n",
         layer->left, layer->top, layer->is_interlaced);

        /* Local Color Table */
        if(buffer[8] & 0x80)
        {
          unsigned num_entries = 2 << (buffer[8] & 0x07);
          table = gif_alloc_color_table(num_entries);
          if(!table)
          {
            ret = GIF_MEM;
            goto error;
          }
          layer->color_table = table;

          table->is_sorted = (buffer[8] & 0x20) >> 5;
          table->num_entries = num_entries;

          gifdbg("  Local Color Table\t sorted:%d entries:%d\n",
           table->is_sorted, table->num_entries);

          if(readfn(table->entries, num_entries * 3, handle) < num_entries * 3)
          {
            ret = GIF_IO;
            goto error;
          }
        }

        if(layer->is_interlaced)
        {
          dest = (uint8_t *)calloc(1, width * height);
          if(!dest)
          {
            ret = GIF_MEM;
            goto error;
          }
        }
        else
          dest = layer->pixels;

        ret = gif_decode(dest, &unpack_tree, layer, handle, readfn);
        if(ret != GIF_OK)
        {
          if(layer->is_interlaced)
            free(dest);
          goto error;
        }

        if(layer->is_interlaced)
        {
          gif_deinterlace(layer->pixels, dest, width, height);
          free(dest);
        }

        layer->control = has_control ? gif->control : default_control;
        has_control = 0;
        break;
      }

      case 0x3B:        /* Trailer */
        gifdbg("End of GIF\n\n");
        goto end_of_stream;
    }
  }
end_of_stream:
  if(has_control)
    gif->control_after_final_image = GIF_TRUE;

  free(unpack_tree);
  return GIF_OK;

error:
  free(unpack_tree);
  gif_close(gif);
  return ret;
}


void gif_close(struct gif_info *gif)
{
  unsigned i;

  free(gif->global_color_table);
  gif->global_color_table = NULL;

  if(gif->images)
  {
    for(i = 0; i < gif->num_images; i++)
    {
      if(gif->images[i])
        free(gif->images[i]->color_table);

      free(gif->images[i]);
    }
    free(gif->images);
    gif->images = NULL;
  }

  if(gif->comments)
  {
    for(i = 0; i < gif->num_comments; i++)
      free(gif->comments[i]);
    free(gif->comments);
    gif->comments = NULL;
  }

  if(gif->appdata)
  {
    for(i = 0; i < gif->num_appdata; i++)
      free(gif->appdata[i]);
    free(gif->appdata);
    gif->appdata = NULL;
  }
}


#ifndef GIF_NO_COMPOSITOR

/***********************************************/
/* Multi-image GIF compositor for GrafX2 GIFs. */
/***********************************************/

struct gif_composite
{
  struct gif_rgba *pixels;
  unsigned width;
  unsigned height;
  unsigned scale;
  double scalex;
  double scaley;
};

static void gif_composite_scale(double *x, double *y, const struct gif_info *gif)
{
  if(gif->pixel_ratio < (64 - 15))
  {
    *x = 1.0;
    *y = 64.0 / (gif->pixel_ratio + 15);
  }
  else
  {
    *x = (gif->pixel_ratio + 15) * 0.015625; /* 1/64 */
    *y = 1.0;
  }
}

void gif_composite_size(unsigned *w, unsigned *h, const struct gif_info *gif)
{
  if(gif->pixel_ratio)
  {
    double x, y;
    gif_composite_scale(&x, &y, gif);
    *w = (unsigned)(gif->width * x);
    *h = (unsigned)(gif->height * y);
  }
  else
  {
    *w = gif->width;
    *h = gif->height;
  }
}

static enum gif_error gif_composite_init(struct gif_composite * GIF_RESTRICT image,
 const struct gif_info *gif, const gif_alloc_function allocfn)
{
  uint64_t size;
  memset(image, 0, sizeof(struct gif_composite));

  if(gif->pixel_ratio)
  {
    image->scale = 1;
    gif_composite_scale(&image->scalex, &image->scaley, gif);
    gif_composite_size(&image->width, &image->height, gif);
  }
  else
  {
    image->width = gif->width;
    image->height = gif->height;
  }

  size = (uint64_t)image->width * (uint64_t)image->height;
  if(size > SIZE_MAX || size * sizeof(struct gif_rgba) > SIZE_MAX)
    return GIF_MEM;

  image->pixels = (struct gif_rgba *)allocfn(size * sizeof(struct gif_rgba));
  if(!image->pixels)
    return GIF_MEM;

  return GIF_OK;
}

static void gif_composite_palette(struct gif_rgba palette[256],
 const struct gif_color_table *table)
{
  const struct gif_color *in;
  unsigned count;
  unsigned i;

  memset(palette, 0, sizeof(struct gif_rgba) * 256);
  if(!table)
    return;

  in = table->entries;
  count = GIF_MIN(table->num_entries, 256);

  for(i = 0; i < count; i++)
  {
    palette->r = in->r;
    palette->g = in->g;
    palette->b = in->b;
    palette->a = 255;

    palette++;
    in++;
  }
}

static void gif_composite_line(struct gif_rgba * GIF_RESTRICT dest,
 const struct gif_rgba *palette, const uint8_t *line, size_t length)
{
  size_t x;
  for(x = 0; x < length; x++)
    *(dest++) = palette[line[x]];
}

static void gif_composite_line_tr(struct gif_rgba * GIF_RESTRICT dest,
 const struct gif_rgba *palette, const uint8_t *line, size_t length,
 unsigned tcol)
{
  size_t x;
  for(x = 0; x < length; x++)
  {
    if(line[x] != tcol)
      *dest = palette[line[x]];
    dest++;
  }
}

static void gif_composite_line_sc(struct gif_rgba * GIF_RESTRICT dest,
 const struct gif_rgba *palette, const uint8_t *line, size_t length,
 double scalex)
{
  double scale_stop = 0.0;
  double scale_pos = 0.0;
  size_t x;

  for(x = 0; x < length; x++)
  {
    struct gif_rgba col = palette[line[x]];
    for(scale_stop += scalex; scale_pos < scale_stop; scale_pos += 1.0)
      *(dest++) = col;
  }
}

static void gif_composite_line_sc_tr(struct gif_rgba * GIF_RESTRICT dest,
 const struct gif_rgba *palette, const uint8_t *line, size_t length,
 double scalex, unsigned tcol)
{
  double scale_stop = 0.0;
  double scale_pos = 0.0;
  size_t x;

  for(x = 0; x < length; x++)
  {
    struct gif_rgba col = palette[line[x]];
    for(scale_stop += scalex; scale_pos < scale_stop; scale_pos += 1.0)
    {
      if(line[x] != tcol)
        *dest = col;
      dest++;
    }
  }
}

static void gif_composite_layer(struct gif_composite * GIF_RESTRICT image,
 const struct gif_rgba *palette, const struct gif_info *gif,
 const struct gif_image *layer)
{
  /* FIXME: this is not going to interact well with non-integer scaling */
  const int left = GIF_MIN(gif->width, layer->left);
  const int top = GIF_MIN(gif->height, layer->top);
  const int width = GIF_MIN(gif->width - left, layer->width);
  const int height = GIF_MIN(gif->height - top, layer->height);
  const int layer_pitch = layer->width;

  const int output_left = image->scale ? (int)(left * image->scalex) : left;
  const int output_top = image->scale ? (int)(top * image->scaley) : top;
  const int output_pitch = image->width;

  struct gif_rgba *pixels = image->pixels + output_left + (image->width * output_top);
  const uint8_t *line = layer->pixels;

  /* pls unswitch gcc */
  const int scale = image->scale;
  const int tcol = layer->control.transparent_color;

  double scale_stop = 0.0;
  double scale_pos = 0.0;
  int y;

  for(y = 0; y < height; y++)
  {
    if(tcol >= 0)
    {
      if(scale)
        gif_composite_line_sc_tr(pixels, palette, line, width, image->scalex, tcol);
      else
        gif_composite_line_tr(pixels, palette, line, width, tcol);
    }
    else
    {
      if(scale)
        gif_composite_line_sc(pixels, palette, line, width, image->scalex);
      else
        gif_composite_line(pixels, palette, line, width);
    }

    if(scale)
    {
      scale_stop += image->scaley;
      scale_pos += 1.0;
      while(scale_pos < scale_stop)
      {
        /* Duplicate line */
        memcpy(pixels + image->width, pixels, image->width * sizeof(struct gif_rgba));
        pixels += image->width;
        scale_pos += 1.0;
      }
    }
    pixels += output_pitch;
    line += layer_pitch;
  }
}

static void gif_composite_background(struct gif_composite * GIF_RESTRICT image,
 const struct gif_rgba *palette, const struct gif_info *gif)
{
  struct gif_rgba *dest = image->pixels;
  struct gif_rgba col;
  unsigned i;
  unsigned j;

  col = palette[gif->bg_color];

  for(i = 0; i < image->height; i++)
    for(j = 0; j < image->width; j++)
      *(dest++) = col;
}

enum gif_error gif_composite(struct gif_rgba **_pixels,
 const struct gif_info *gif, const gif_alloc_function allocfn)
{
  struct gif_composite image;
  struct gif_rgba global_palette[256];
  struct gif_rgba layer_palette[256];
  unsigned i;

  enum gif_error err = gif_composite_init(&image, gif, allocfn);
  if(err != GIF_OK)
    return err;

  *_pixels = image.pixels;

  gif_composite_palette(global_palette, gif->global_color_table);
  gif_composite_background(&image, global_palette, gif);

  if(!gif->images)
    return GIF_OK;

  for(i = 0; i < gif->num_images; i++)
  {
    const struct gif_image *layer = gif->images[i];
    struct gif_rgba *palette = global_palette;

    if(!layer || layer->type != GIF_IMAGE)
      continue;

    if(layer->color_table)
    {
      gif_composite_palette(layer_palette, layer->color_table);
      palette = layer_palette;
    }
    gif_composite_layer(&image, palette, gif, layer);
  }
  return GIF_OK;
}

#endif /* !GIF_NO_COMPOSITOR */
