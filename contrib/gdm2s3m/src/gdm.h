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
  uint8_t title[33];
  uint8_t artist[33];
  uint16_t version;        /* GDM format version (always 1.0) */
  uint16_t tracker;        /* tracker used to generate file */
  uint16_t tracker_ver;    /* version of tracker used */
  uint8_t panning[32];     /* panning map, 1 byte per channel */
  uint8_t global_vol;      /* global volume */
  uint8_t tempo;
  uint8_t bpm;
  uint16_t origfmt;        /* original format */
};

struct GDM_order {
  uint8_t *patterns;
};

struct GDM_pattern {
  uint16_t length;
  uint8_t *data;
};

struct GDM_samhdr {
  uint8_t name[33];
  uint8_t filename[13];    /* original filename */
  uint32_t length;         /* length of sample, in bytes */
  uint32_t begin;          /* loop beginning, in samples */
  uint32_t end;            /* loop ending, in samples */
  uint8_t flags;           /* sample type flags */
  uint16_t rate;           /* playback speed in Hz, should be 8363 */
  uint8_t volume;          /* default volume */
  uint8_t pan;             /* default panning */
};

/**
 * end of file structures
 */

struct GDM_file {
  struct GDM_header header;
  struct GDM_order orders;
  struct GDM_pattern *patterns;
  struct GDM_sample *samples;
  uint8_t order_len;
  uint8_t numpatterns;
  uint8_t numsamples;
};

struct GDM_sample {
  struct GDM_samhdr header;  /* file structure */
  uint8_t *data;             /* sample data */
};

/* function prototypes */
struct GDM_file *load_gdm (uint8_t *stream, uint32_t stream_len);
void info_gdm (struct GDM_file *gdm);
void free_gdm (struct GDM_file *gdm);

__G_END_DECLS

#endif /* __GDM_H */
