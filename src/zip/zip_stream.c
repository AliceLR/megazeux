/* MegaZeux
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <inttypes.h>

#include "zip_stream.h"
#include "deflate.h"

/**
 * The extra decompressors (shrink, reduce, implode, deflate64) are useful for
 * utils that might need to scan an arbitrary archive (checkres).
 */
#if defined(CONFIG_UTILS) || defined(__EMSCRIPTEN__)
#define ZIP_EXTRA_DECOMPRESSORS
#include "deflate64.h"
#include "implode.h"
#include "reduce.h"
#include "shrink.h"
#endif

static void zip_stream_close(struct zip_stream *zs, size_t *final_input_length,
 size_t *final_output_length)
{
  if(final_input_length)
    *final_input_length = zs->final_input_length;

  if(final_output_length)
    *final_output_length = zs->final_output_length;
}

static boolean zip_stream_input(struct zip_stream *zs, const void *src,
 size_t src_len)
{
  if(!zs->next_input)
  {
    zs->next_input = src;
    zs->next_input_length = src_len;
    return true;
  }
  return false;
}

static boolean zip_stream_output(struct zip_stream *zs, void *dest, size_t len)
{
  if(!zs->output_start)
  {
    zs->output_start = (uint8_t *)dest;
    zs->output_pos = zs->output_start;
    zs->output_end = zs->output_start + len;
    return true;
  }
  return false;
}

#ifdef ZIP_EXTRA_DECOMPRESSORS
static struct zip_stream_spec shrink_spec =
{
  unshrink_open,
  NULL,
  unshrink_close,
  zip_stream_input,
  zip_stream_output,
  unshrink_file,
  NULL,
  NULL,
  NULL
};

static struct zip_stream_spec reduce_spec =
{
  reduce_ex_open,
  NULL,
  zip_stream_close,
  zip_stream_input,
  zip_stream_output,
  reduce_ex_file,
  NULL,
  NULL,
  NULL
};

static struct zip_stream_spec implode_spec =
{
  expl_open,
  NULL,
  expl_close,
  zip_stream_input,
  zip_stream_output,
  expl_file,
  NULL,
  NULL,
  NULL
};

static struct zip_stream_spec deflate64_spec =
{
  inflate64_open,
  NULL,
  inflate64_close,
  zip_stream_input,
  zip_stream_output,
  inflate64_file,
  NULL,
  NULL,
  NULL
};
#endif

static struct zip_stream_spec deflate_spec =
{
  inflate_open,
  deflate_open,
  zip_stream_close,
  deflate_input,
  deflate_output,
  inflate_file,
  NULL, // TODO
  deflate_file,
  NULL // TODO
};

struct zip_stream_spec *zip_stream_specs[] =
{
#ifdef ZIP_EXTRA_DECOMPRESSORS
  [ZIP_M_SHRUNK]    = &shrink_spec,
  [ZIP_M_REDUCED_1] = &reduce_spec,
  [ZIP_M_REDUCED_2] = &reduce_spec,
  [ZIP_M_REDUCED_3] = &reduce_spec,
  [ZIP_M_REDUCED_4] = &reduce_spec,
  [ZIP_M_IMPLODED]  = &implode_spec,
  [ZIP_M_DEFLATE64] = &deflate64_spec,
#endif
  [ZIP_M_DEFLATE]   = &deflate_spec,
};
