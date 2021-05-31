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

#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "audio_struct.h"
#include "ext.h"

#include "../io/path.h"

struct registry_entry
{
  const char *ext;
  construct_stream_fn constructor;
};

static struct registry_entry *registry = NULL;
static int registry_size = 0;
static int registry_alloc = 0;

void audio_ext_register(const char *ext, construct_stream_fn constructor)
{
  if(registry_alloc <= registry_size)
  {
    if(!registry_alloc)
      registry_alloc = 4;

    else
      registry_alloc <<= 1;

    registry = crealloc(registry,
     registry_alloc * sizeof(struct registry_entry));
  }

  registry[registry_size].ext = ext;
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

struct audio_stream *audio_ext_construct_stream(char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct audio_stream *a_return = NULL;
  ssize_t ext_pos;
  int i;

  if(!audio.music_on)
    return NULL;

  ext_pos = path_get_ext_offset(filename);

  // Must contain a valid ext
  if(ext_pos < 0 || ext_pos >= (int)strlen(filename))
    return NULL;

  // Find a constructor in the registry
  for(i = 0; i < registry_size; i++)
  {
    if(!strcasecmp(filename + ext_pos + 1, registry[i].ext))
    {
      construct_stream_fn constructor = registry[i].constructor;

      a_return = constructor(filename, frequency, volume, repeat);

      if(a_return)
        break;
    }
  }

  return a_return;
}
