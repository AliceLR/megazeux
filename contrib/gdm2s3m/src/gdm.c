/**
 *  Copyright (C) 2003-2004  Alistair John Strachan  (alistair@devzero.co.uk)
 *
 *  This is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2, or (at your option) any later
 *  version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gdm.h"
#include "error.h"
#include "utility.h"

struct GDM_file *load_gdm (u8 *stream, u32 stream_len)
{
  u8 *backup = stream, *trace, *trace2, magic[4];
  struct GDM_header *header;
  struct GDM_pattern *pattern;
  struct GDM_samhdr *sample;
  struct GDM_file *gdm;
  u32 address;
  int i;

  /* allocate gdm file structure */
  gdm = (struct GDM_file *) malloc (sizeof (struct GDM_file));
  memset (gdm, 0, sizeof (struct GDM_file));

  /* level of indirection */
  header = &gdm->header;

  /* extract header */
  check_s_to_a (backup, stream_len, (void *) magic, &stream, 4);

  /* check it's a valid file */
  if (strncmp ((const char *) magic, GDM_MAGIC, 4) != 0)
    error (FILE_INVALID);

  /* get artist and title */
  check_s_to_a (backup, stream_len, (void *) header->title, &stream, 32);
  check_s_to_a (backup, stream_len, (void *) header->artist, &stream, 32);

  /* skip junk */
  stream += 3 + 4;

  /* get file version */
  check_s_to_a (backup, stream_len, (void *) &header->version, &stream, 2);
  CHECK_ENDIAN_16 (&header->version);

  /* get tracker and version */
  check_s_to_a (backup, stream_len, (void *) &header->tracker, &stream, 2);
  check_s_to_a (backup, stream_len, (void *) &header->tracker_ver, &stream, 2);
  CHECK_ENDIAN_16 (&header->tracker);
  CHECK_ENDIAN_16 (&header->tracker_ver);

  /* get panning map */
  check_s_to_a (backup, stream_len, (void *) &header->panning, &stream, 32);

  /* get global volume, tempo and bpm */
  check_s_to_a (backup, stream_len, (void *) &header->global_vol, &stream, 1);
  check_s_to_a (backup, stream_len, (void *) &header->bpm, &stream, 1);
  check_s_to_a (backup, stream_len, (void *) &header->tempo, &stream, 1);

  /* get original format */
  check_s_to_a (backup, stream_len, (void *) &header->origfmt, &stream, 2);
  CHECK_ENDIAN_16 (&header->origfmt);

  /* ORDER HEADER ---------------------------------------------------------- */
  check_s_to_a (backup, stream_len, (void *) &address, &stream, 4);
  check_s_to_a (backup, stream_len, (void *) &gdm->order_len, &stream, 1);
  CHECK_ENDIAN_32 (&address);

  /* stupid gdm bug */
  gdm->order_len++;

  /* put stream at order offset */
  trace = (u8 *) (backup + address);

  /* allocate space for orders */
  gdm->orders.patterns = (u8 *) malloc (gdm->order_len);

  /* copy orders */
  memcpy (gdm->orders.patterns, trace, gdm->order_len);

  /* PATTERN SEGMENT ------------------------------------------------------- */
  check_s_to_a (backup, stream_len, (void *) &address, &stream, 4);
  check_s_to_a (backup, stream_len, (void *) &gdm->numpatterns, &stream, 1);
  CHECK_ENDIAN_32 (&address);

  /* stupid gdm bug */
  gdm->numpatterns++;

  /* put stream at pattern offset */
  trace = (u8 *) (backup + address);

  /* allocate pattern space */
  gdm->patterns = (struct GDM_pattern *)
                    malloc (sizeof (struct GDM_pattern) * gdm->numpatterns);

  /* read in encoded pattern data */
  for (i = 0; i < gdm->numpatterns; i++) {
    pattern = &gdm->patterns[i];

    /* get length of data */
    check_s_to_a (backup, stream_len, (void *) &pattern->length, &trace, 2);
    CHECK_ENDIAN_16 (&pattern->length);

    /* remove top two bytes */
    pattern->length -= 2;

    /* allocate data space */
    pattern->data = (u8 *) malloc (pattern->length);

    /* get pattern data */
    memcpy (pattern->data, trace, pattern->length);

    /* move along */
    trace += pattern->length;
  }

  /* SAMPLE HEADERS -------------------------------------------------------- */
  check_s_to_a (backup, stream_len, (void *) &address, &stream, 4);
  CHECK_ENDIAN_32 (&address);

  /* move stream along to sample header */
  trace = (u8 *) (backup + address);

  /* get sample data segment */
  check_s_to_a (backup, stream_len, (void *) &address, &stream, 4);
  check_s_to_a (backup, stream_len, (void *) &gdm->numsamples, &stream, 1);
  CHECK_ENDIAN_32 (&address);

  /* stupid gdm bug */
  gdm->numsamples++;

  /* set up initial offset */
  trace2 = (u8 *) (backup + address);

  /* allocate sample space */
  gdm->samples = (struct GDM_sample *)
                   malloc (sizeof (struct GDM_sample) * gdm->numsamples);
  memset (gdm->samples, 0, sizeof (struct GDM_sample) * gdm->numsamples);

  /* read in sample headers and copy sample data */
  for (i = 0; i < gdm->numsamples; i++) {
    sample = &gdm->samples[i].header;

    /* get sample name */
    check_s_to_a (backup, stream_len, (void *) sample->name, &trace, 32);

    /* get sample filename */
    check_s_to_a (backup, stream_len, (void *) sample->filename, &trace, 12);

    /* skip EMS handle (should be zero) */
    trace++;

    /* get sample data length */
    check_s_to_a (backup, stream_len, (void *) &sample->length, &trace, 4);
    CHECK_ENDIAN_32 (&sample->length);

    /* get loop beginning/end */
    check_s_to_a (backup, stream_len, (void *) &sample->begin, &trace, 4);
    check_s_to_a (backup, stream_len, (void *) &sample->end, &trace, 4);
    CHECK_ENDIAN_32 (&sample->begin);
    CHECK_ENDIAN_32 (&sample->end);

    /* get sample flags */
    check_s_to_a (backup, stream_len, (void *) &sample->flags, &trace, 1);

    /* get rate data */
    check_s_to_a (backup, stream_len, (void *) &sample->rate, &trace, 2);
    CHECK_ENDIAN_16 (&sample->rate);

    /* get volume */
    check_s_to_a (backup, stream_len, (void *) &sample->volume, &trace, 1);

    /* get pan */
    check_s_to_a (backup, stream_len, (void *) &sample->pan, &trace, 1);

    /* allocate space for sample data */
    gdm->samples[i].data = (u8 *) malloc (sample->length);

    /* copy sample data */
    memcpy (gdm->samples[i].data, trace2, sample->length);

    /* move sample data pointer along */
    trace2 += sample->length;
  }

  /* gdm structure */
  return gdm;
}

void info_gdm (struct GDM_file *gdm)
{
  union version { u16 v; struct { u8 maj, min; } ver; } version;
  struct GDM_header *header = &gdm->header;
  struct GDM_samhdr *samhdr;
  int i;

  /* prepare version dump */
  memcpy (&version, &header->tracker_ver, 2);

  /* basic information */
  PRINT_OUT ("title:\t\t\t%s\n", header->title);
  PRINT_OUT ("artist:\t\t\t%s\n", header->artist);
  PRINT_DEBUG ("version:\t\t%u\n", header->version);
  PRINT_DEBUG ("tracker:\t\t%u\n", header->tracker);
  PRINT_DEBUG ("tracker version:\t%u.%u\n", version.ver.maj, version.ver.min);
  PRINT_OUT ("global vol:\t\t%u\n", header->global_vol);
  PRINT_OUT ("tempo/speed:\t\t%u/%u\n", header->tempo, header->bpm);

  /* dump originating format */
  PRINT_OUT ("original format:\t");

  switch (header->origfmt) {
    case 1: PRINT_OUT ("MOD\n"); break;
    case 2: PRINT_OUT ("MTM\n"); break;
    case 3: PRINT_OUT ("S3M\n"); break;
    case 4: PRINT_OUT ("669\n"); break;
    case 5: PRINT_OUT ("FAR\n"); break;
    case 6: PRINT_OUT ("ULT\n"); break;
    case 7: PRINT_OUT ("STM\n"); break;
    case 8: PRINT_OUT ("MED\n"); break;
  }

#ifdef DEBUG
  /* panning map */
  PRINT_OUT ("\n\t\tL      MM      R  S?:\n");

  for (i = 0; i < 32; i++)
    if (header->panning[i] != 255) {
      /* channel's active */
      PRINT_OUT (" chan %d:\t", i);

      /* padding */
      if (header->panning[i] != 16)
      {
        for (int j = 0; j < header->panning[i]; j++)
          PRINT_OUT (" ");
      }
      else
        PRINT_OUT ("                  ");

      /* and a marker.. */
      PRINT_OUT ("|\n");
    }
#endif

  PRINT_OUT ("\n");

  /* dump sample information */
  for (i = 0; i < gdm->numsamples; i++) {
    samhdr = &gdm->samples[i].header;

    PRINT_OUT ("name:\t\t\t%s\n", samhdr->name);
    PRINT_OUT ("filename:\t\t%s\n", samhdr->filename);
    PRINT_OUT ("data length:\t\t%d\n", samhdr->length);
    PRINT_DEBUG ("loop begin:\t\t%d\n", samhdr->begin);
    PRINT_DEBUG ("loop end:\t\t%d\n", samhdr->end);
    PRINT_DEBUG ("flags:\t\t%d\n", samhdr->flags);
    PRINT_DEBUG ("rate:\t\t%d\n", samhdr->rate);
    PRINT_OUT ("volume:\t\t\t%d\n", samhdr->volume);
    PRINT_OUT ("pan:\t\t\t%d\n", samhdr->pan);
    PRINT_OUT ("\n");
  }

  /* dump order information */
  PRINT_OUT ("%d orders:\n", gdm->order_len);

  for (i = 0; i < gdm->order_len; i++)
    PRINT_OUT ("%d ", gdm->orders.patterns[i]);

  PRINT_OUT ("\n\n");

  /* dump pattern information */
  PRINT_OUT ("%d patterns:\n", gdm->numpatterns);

  for (i = 0; i < gdm->numpatterns; i++)
    PRINT_OUT ("  pattern %d, %d bytes.\n", i, gdm->patterns[i].length);

  PRINT_OUT ("\n");
}

void free_gdm (struct GDM_file *gdm)
{
  int i;

  /* free up order data */
  free (gdm->orders.patterns);

  /* free up pattern data */
  for (i = 0; i < gdm->numpatterns; i++)
    free (gdm->patterns[i].data);
  free (gdm->patterns);

  /* free up sample data */
  for (i = 0; i < gdm->numsamples; i++)
    free (gdm->samples[i].data);
  free (gdm->samples);

  /* free it */
  free (gdm);
}
