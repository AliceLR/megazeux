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

#ifndef __GDM_H
#define __GDM_H

#include "types.h"

__G_BEGIN_DECLS

/**
 * below are the file structures used by the GDM file format
 */

#define GDM_MAGIC "GDM\xFE"

struct GDM_header {
  u8 title[33];
  u8 artist[33];
  u16 version;        /* GDM format version (always 1.0) */
  u16 tracker;        /* tracker used to generate file */
  u16 tracker_ver;    /* version of tracker used */
  u8 panning[32];     /* panning map, 1 byte per channel */
  u8 global_vol;      /* global volume */
  u8 tempo;
  u8 bpm;
  u16 origfmt;        /* original format */
};

struct GDM_order {
  u8 *patterns;
};

struct GDM_pattern {
  u16 length;
  u8 *data;
};

struct GDM_samhdr {
  u8 name[33];
  u8 filename[13];    /* original filename */
  u32 length;         /* length of sample, in bytes */
  u32 begin;          /* loop beginning, in samples */
  u32 end;            /* loop ending, in samples */
  u8 flags;           /* sample type flags */
  u16 rate;           /* playback speed in Hz, should be 8363 */
  u8 volume;          /* default volume */
  u8 pan;             /* default panning */
};

/**
 * end of file structures
 */

struct GDM_file {
  struct GDM_header header;
  struct GDM_order orders;
  struct GDM_pattern *patterns;
  struct GDM_sample *samples;
  u8 order_len;
  u8 numpatterns;
  u8 numsamples;
};

struct GDM_sample {
  struct GDM_samhdr header;  /* file structure */
  u8 *data;                  /* sample data */
};

/* function prototypes */
struct GDM_file *load_gdm (u8 *stream, u32 stream_len);
void info_gdm (struct GDM_file *gdm);
void free_gdm (struct GDM_file *gdm);

__G_END_DECLS

#endif /* __GDM_H */
