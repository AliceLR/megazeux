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

#ifndef __S3M_H
#define __S3M_H

#include "types.h"
#include "gdm.h"

__G_BEGIN_DECLS

/**
 * below are the file structures used by the GDM file format
 */

#define S3M_MAGIC         "SCRM"
#define S3M_SAMPLE_MAGIC  "SCRS"

struct S3M_header {
  uint8_t title[28];      /* song title, last byte must be \0 */
  uint16_t numorders;     /* number of orders */
  uint16_t numinst;       /* number of adlib instruments */
  uint16_t numpatterns;   /* number of wav samples */
  uint8_t global_vol;     /* global volume */
  uint8_t speed;
  uint8_t tempo;
  uint8_t chansett[32];   /* channel settings */
};

struct S3M_order {
  uint8_t *patterns;
};

struct S3M_samhdr {
  uint8_t filename[13];    /* original filename */
  uint32_t length;         /* length of sample, in bytes */
  uint32_t begin;          /* loop beginning, in samples */
  uint32_t end;            /* loop ending, in samples */
  uint8_t volume;          /* default volume */
  uint8_t flags;           /* sample type flags */
  uint16_t rate;           /* playback speed in Hz, should be 8363 */
  uint8_t name[28];
};

struct S3M_pattern {
  uint16_t length;
  uint8_t *data;
};

/**
 * end of file structures
 */

struct S3M_file {
  struct S3M_header header;
  struct S3M_order orders;
  struct S3M_sample *samples;
  struct S3M_pattern *patterns;
  uint8_t panmap[32];
};

struct S3M_sample {
  struct S3M_samhdr header;  /* file structure */
  uint8_t *data;             /* sample data */
};

/* function prototypes */
uint8_t *save_s3m (struct S3M_file *s3m, size_t *stream_len);
struct S3M_file *convert_gdm_to_s3m (struct GDM_file *gdm);
void free_s3m (struct S3M_file *s3m);

__G_END_DECLS

#endif /* __S3M_H */
