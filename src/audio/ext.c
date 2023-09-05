/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "audio_struct.h"
#include "ext.h"

#include "../util.h"
#include "../io/vio.h"

struct registry_entry
{
  filter_stream_fn test;
  construct_stream_fn constructor;
};

static struct registry_entry *registry = NULL;
static size_t registry_size = 0;
static size_t registry_alloc = 0;

void audio_ext_register(filter_stream_fn test, construct_stream_fn constructor)
{
  assert(constructor);
  if(registry_alloc <= registry_size)
  {
    struct registry_entry *tmp;

    if(!registry_alloc)
      registry_alloc = 8;
    else
      registry_alloc <<= 1;

    tmp = (struct registry_entry *)realloc(registry,
     registry_alloc * sizeof(struct registry_entry));
    if(!tmp)
    {
      warn("failed to allocate memory for audio format registry.\n");
      return;
    }
    registry = tmp;
  }

  registry[registry_size].test = test;
  registry[registry_size].constructor = constructor;
  registry_size++;
}

void audio_ext_free_registry(void)
{
  free(registry);
  registry = NULL;
  registry_size = 0;
  registry_alloc = 0;
}

struct audio_stream *audio_ext_construct_stream(const char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct audio_stream *a_return = NULL;
  vfile *vf;
  unsigned i;

  if(!audio.music_on)
    return NULL;

  // Using a buffer vastly improves module load times on some architectures.
  vf = vfopen_unsafe_ext(filename, "rb", V_LARGE_BUFFER);
  if(!vf)
    return NULL;

  // Find a constructor in the registry
  for(i = 0; i < registry_size; i++)
  {
    construct_stream_fn constructor = registry[i].constructor;
    if(registry[i].test)
    {
      boolean result = registry[i].test(vf, filename);
      vrewind(vf);
      if(!result)
        continue;
    }

    a_return = constructor(vf, filename, frequency, volume, repeat);
    if(a_return)
      break;

    vrewind(vf);
  }

  // The constructor function is responsible for closing or
  // retaining the file handle on successful loads.
  if(!a_return)
    vfclose(vf);

  return a_return;
}
