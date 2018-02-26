/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __AUDIO_STREAM_REGISTRY_H
#define __AUDIO_STREAM_REGISTRY_H

#include "compat.h"

__M_BEGIN_DECLS

#include "audio.h"
#include "platform.h"

typedef struct audio_stream *(*construct_stream_fn)(char *,
 Uint32, Uint32, Uint32);

void audio_register_ext(const char *ext, construct_stream_fn constructor);
void audio_free_registry(void);

struct audio_stream *construct_stream_audio_file(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat);

__M_END_DECLS

#endif /* __AUDIO_STREAMS_H */
