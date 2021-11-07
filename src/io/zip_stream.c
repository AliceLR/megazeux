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

#include "zip.h"
#include "zip_stream.h"
#include "zip_deflate.h"

/**
 * The extra decompressors (shrink, reduce, implode, deflate64) are useful for
 * utils that might need to scan an arbitrary archive (checkres).
 */
#if defined(CONFIG_UTILS) || defined(__EMSCRIPTEN__)
#define ZIP_EXTRA_DECOMPRESSORS
#include "zip_deflate64.h"
#include "zip_implode.h"
#include "zip_reduce.h"
#include "zip_shrink.h"
#endif

#ifdef ZIP_EXTRA_DECOMPRESSORS
static void zip_stream_destroy(struct zip_stream_data *zs)
{
  free(zs);
}

static void zip_stream_close(struct zip_stream_data *zs,
 size_t *final_input_length, size_t *final_output_length)
{
  if(final_input_length)
    *final_input_length = zs->final_input_length;

  if(final_output_length)
    *final_output_length = zs->final_output_length;
}

static boolean zip_stream_input(struct zip_stream_data *zs, const void *src,
 size_t src_len)
{
  if(!zs->input_buffer)
  {
    zs->input_buffer = (uint8_t *)src;
    zs->input_length = src_len;
    return true;
  }
  return false;
}

static boolean zip_stream_output(struct zip_stream_data *zs, void *dest,
 size_t dest_len)
{
  zs->output_buffer = (uint8_t *)dest;
  zs->output_length = dest_len;
  return true;
}

static struct zip_method_handler shrink_spec =
{
  unshrink_create,
  zip_stream_destroy,
  unshrink_open,
  NULL,
  unshrink_close,
  zip_stream_input,
  zip_stream_output,
  unshrink_file,
  NULL,
  NULL
};

static struct zip_method_handler reduce_spec =
{
  reduce_ex_create,
  zip_stream_destroy,
  reduce_ex_open,
  NULL,
  zip_stream_close,
  zip_stream_input,
  zip_stream_output,
  reduce_ex_file,
  NULL,
  NULL
};

static struct zip_method_handler implode_spec =
{
  expl_create,
  zip_stream_destroy,
  expl_open,
  NULL,
  expl_close,
  zip_stream_input,
  zip_stream_output,
  expl_file,
  NULL,
  NULL
};

static struct zip_method_handler deflate64_spec =
{
  inflate64_create,
  zip_stream_destroy,
  inflate64_open,
  NULL,
  inflate64_close,
  zip_stream_input,
  zip_stream_output,
  inflate64_file,
  NULL,
  NULL
};
#endif

static struct zip_method_handler deflate_spec =
{
  deflate_create,
  deflate_destroy,
  inflate_open,
  deflate_open,
  deflate_close,
  deflate_input,
  deflate_output,
  NULL,
  inflate_block,
  deflate_block
};

struct zip_method_handler *zip_method_handlers[] =
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
