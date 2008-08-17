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
  u8 title[28];      /* song title, last byte must be \0 */
  u16 numorders;     /* number of orders */
  u16 numinst;       /* number of adlib instruments */
  u16 numpatterns;   /* number of wav samples */
  u8 global_vol;     /* global volume */
  u8 speed;
  u8 tempo;
  u8 chansett[32];   /* channel settings */
};

struct S3M_order {
  u8 *patterns;
};

struct S3M_samhdr {
  u8 filename[13];    /* original filename */
  u32 length;         /* length of sample, in bytes */
  u32 begin;          /* loop beginning, in samples */
  u32 end;            /* loop ending, in samples */
  u8 volume;          /* default volume */
  u8 flags;           /* sample type flags */
  u16 rate;           /* playback speed in Hz, should be 8363 */
  u8 name[28];
};

struct S3M_pattern {
  u16 length;
  u8 *data;
};

/**
 * end of file structures
 */

struct S3M_file {
  struct S3M_header header;
  struct S3M_order orders;
  struct S3M_sample *samples;
  struct S3M_pattern *patterns;
  u8 panmap[32];
};

struct S3M_sample {
  struct S3M_samhdr header;  /* file structure */
  u8 *data;                  /* sample data */
};

/* function prototypes */
u8 *save_s3m (struct S3M_file *s3m, u32 *stream_len);
struct S3M_file *convert_gdm_to_s3m (struct GDM_file *gdm);
void remap_effects (u8 gdm_effect, u8 gdm_param,
                    u8 *dest_effect, u8 *dest_param);
void free_s3m (struct S3M_file *s3m);

__G_END_DECLS

#endif /* __S3M_H */
