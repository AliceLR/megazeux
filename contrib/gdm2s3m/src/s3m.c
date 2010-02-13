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

#include "s3m.h"
#include "gdm2s3m.h"
#include "error.h"
#include "utility.h"

uint8_t *save_s3m (struct S3M_file *s3m, size_t *stream_len)
{
  const uint8_t junk1[4] = { 0x1A, 16, 0, 0 };
  const uint8_t junk2[6] = { 0, 0, 0x17, 0x32, 2, 0 };
  const uint8_t junk3[13] = { 0xb0, 0, 0xfc, 0, 0, 0xe, 0x79,
                              0x10, 0xe4, 0, 0, 0, 0 };
  const uint8_t junk4[1] = { 1 };
  const uint8_t junk5[2] = { 0, 0 };
  const uint8_t junk6[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  uint8_t gap[16], *gap2, memseg[3], *backup = NULL, *stream = NULL;
  uint32_t offset = 0, spare, segpad, sample_table, pattern_table;
  struct S3M_order *orders = &s3m->orders;
  struct S3M_header *hdr = &s3m->header;
  char s3m_magic[4], sam_magic[4];
  uint16_t segment;
  int i;

  /* copy magics */
  memcpy (s3m_magic, S3M_MAGIC, 4);
  memcpy (sam_magic, S3M_SAMPLE_MAGIC, 4);

  /* zero gap */
  memset (gap, 0, 16);

  /* zero memseg dummy */
  memset (memseg, 0, 3);

  /* initialise stream_len */
  (*stream_len) = 0;

  /* dump song title */
  check_a_to_s (&backup, stream_len, &hdr->title, &stream, 28);

  /* pad with junk */
  check_a_to_s (&backup, stream_len, (uint8_t *)&junk1, &stream, 4);

  /* dump quantities */
  CHECK_ENDIAN_16 (&hdr->numorders);
  CHECK_ENDIAN_16 (&hdr->numinst);
  CHECK_ENDIAN_16 (&hdr->numpatterns);
  check_a_to_s (&backup, stream_len, &hdr->numorders, &stream, 2);
  check_a_to_s (&backup, stream_len, &hdr->numinst, &stream, 2);
  check_a_to_s (&backup, stream_len, &hdr->numpatterns, &stream, 2);
  CHECK_ENDIAN_16 (&hdr->numorders);
  CHECK_ENDIAN_16 (&hdr->numinst);
  CHECK_ENDIAN_16 (&hdr->numpatterns);

  /* pad with more junk */
  check_a_to_s (&backup, stream_len, (uint8_t *)&junk2, &stream, 6);

  /* dump magic bytes */
  check_a_to_s (&backup, stream_len, s3m_magic, &stream, 4);

  /* dump global volume, speed, tempo */
  check_a_to_s (&backup, stream_len, &hdr->global_vol, &stream, 1);
  check_a_to_s (&backup, stream_len, &hdr->speed, &stream, 1);
  check_a_to_s (&backup, stream_len, &hdr->tempo, &stream, 1);

  /* pad with remaining junk */
  check_a_to_s (&backup, stream_len, (uint8_t *)&junk3, &stream, 13);

  /* channel settings */
  check_a_to_s (&backup, stream_len, &hdr->chansett, &stream, 32);

  /* ORDER HEADER ---------------------------------------------------------- */
  check_a_to_s (&backup, stream_len, orders->patterns, &stream,
                  hdr->numorders);

  /* update offset */
  offset = 96 + hdr->numorders;

  /* figure out how much space we've used up */
  sample_table = offset;
  pattern_table = offset + 2 * hdr->numinst;

  /* calculate padding size */
  segpad = hdr->numorders + 2 * hdr->numinst + 2 * hdr->numpatterns;
  segpad += (segpad % 16) ? 16 - segpad % 16 : 0;
  segpad -= hdr->numorders;
  segment = 2 * hdr->numinst + 2 * hdr->numpatterns;

  /* allocate space for sample table, pattern table and padding */
  gap2 = malloc (segpad);
  memset (gap2, 0, segpad);

  /* pad up to segment boundary */
  check_a_to_s (&backup, stream_len, gap2, &stream, segment);

  /* write channel pan values */
  check_a_to_s (&backup, stream_len, &s3m->panmap, &stream, 32);

  /* pad up to segment boundary */
  check_a_to_s (&backup, stream_len, &gap2[segment], &stream,
                 segpad - segment);

  /* free padding */
  free (gap2);

  /* update offset */
  offset += 32 + segpad;

  /* SAMPLE HEADERS -------------------------------------------------------- */

  /* dump sample headers */
  for (i = 0; i < hdr->numinst; i++) {
    struct S3M_samhdr *sample = &s3m->samples[i].header;

    /* write out junk byte */
    check_a_to_s (&backup, stream_len, (uint8_t *)&junk4, &stream, 1);

    /* dos filename */
    check_a_to_s (&backup, stream_len, &sample->filename, &stream, 12);

    /* memseg (dummy) */
    check_a_to_s (&backup, stream_len, memseg, &stream, 3);

    /* sample length */
    CHECK_ENDIAN_32 (&sample->length);
    check_a_to_s (&backup, stream_len, &sample->length, &stream, 4);
    CHECK_ENDIAN_32 (&sample->length);

    /* loop beginning */
    CHECK_ENDIAN_32 (&sample->begin);
    check_a_to_s (&backup, stream_len, &sample->begin, &stream, 4);

    /* loop ending */
    CHECK_ENDIAN_32 (&sample->end);
    check_a_to_s (&backup, stream_len, &sample->end, &stream, 4);

    /* volume */
    check_a_to_s (&backup, stream_len, &sample->volume, &stream, 1);

    /* more junk */
    check_a_to_s (&backup, stream_len, (uint8_t *)&junk5, &stream, 2);

    /* flags */
    check_a_to_s (&backup, stream_len, &sample->flags, &stream, 1);

    /* rate */
    CHECK_ENDIAN_16 (&sample->rate);
    check_a_to_s (&backup, stream_len, &sample->rate, &stream, 2);

    /* last bit of junk */
    check_a_to_s (&backup, stream_len, (uint8_t *)&junk6, &stream, 14);

    /* sample name */
    check_a_to_s (&backup, stream_len, &sample->name, &stream, 28);

    /* sample magic */
    check_a_to_s (&backup, stream_len, sam_magic, &stream, 4);

    /* work out segment */
    segment = (uint16_t)(offset / 16);

    /* update sample parapointer */
    CHECK_ENDIAN_16 (&segment);
    memcpy (&backup[sample_table], &segment, 2);

    /* update sample_table pointer */
    sample_table += 2;

    /* update offset */
    offset += 80;
  }

  /* PATTERN SEGMENT ------------------------------------------------------- */

  /* dump pattern lengths followed by pattern data */
  for (i = 0; i < hdr->numpatterns; i++) {
    struct S3M_pattern *pattern = &s3m->patterns[i];

    /* add on size of length data to length */
    pattern->length += 2;

    /* write out length of pattern */
    CHECK_ENDIAN_16 (&pattern->length);
    check_a_to_s (&backup, stream_len, &pattern->length, &stream, 2);
    CHECK_ENDIAN_16 (&pattern->length);

    /* write out pattern data */
    check_a_to_s (&backup, stream_len, pattern->data, &stream,
                    pattern->length - 2);

    /* work out segment */
    segment = (uint16_t)(offset / 16);

    /* update pattern parapointer */
    CHECK_ENDIAN_16 (&segment);
    memcpy (&backup[pattern_table], &segment, 2);

    /* update pattern_table pointer */
    pattern_table += 2;

    /* update offset */
    offset += pattern->length;

    /* work out padding */
    segpad = (offset % 16) ? 16 - offset % 16 : 0;

    /* write out padding */
    check_a_to_s (&backup, stream_len, gap, &stream, segpad);

    /* update offset */
    offset += segpad;
  }

  /* SAMPLE DATA -------------------------------------------------------- */

  /**
   * we can use the parapointers we've been storing to re-access the samples
   * saved before at the appropriate file offsets. Quite useful.
   */

  sample_table = 96 + hdr->numorders;

  /* dump sample data */
  for (i = 0; i < hdr->numinst; i++) {
    struct S3M_sample *sample = &s3m->samples[i];

    /* write out sample data */
    check_a_to_s (&backup, stream_len, sample->data, &stream,
                   sample->header.length);

    /* extract offset from parapointer table */
    memcpy (&segment, &backup[sample_table], 2);
    CHECK_ENDIAN_16 (&segment);

    /* update sample table pointer */
    sample_table += 2;

    /* real byte offset */
    spare = segment * 16 + 13;

    /* work out segment */
    segment = (uint16_t)(offset / 16);

    /* stick it into that awkward 24bit parapointer */
    CHECK_ENDIAN_16 (&segment);
    memcpy (&backup[spare + 1], &segment, 2);

    /* update offset! */
    offset += sample->header.length;

    /* work out padding */
    segpad = (offset % 16) ? 16 - offset % 16 : 0;

    /* write out padding */
    check_a_to_s (&backup, stream_len, gap, &stream, segpad);

    /* update offset */
    offset += segpad;
  }

  /* return entire stream */
  return backup;
}

/**
 * written by madbrain; contact him if something here isn't right
 */
static void remap_effects (uint8_t gdm_effect, uint8_t gdm_param,
                           uint8_t *dest_effect, uint8_t *dest_param)
{
  uint8_t s3m_effect = 0;
  uint8_t s3m_param = 0;

  switch(gdm_effect)
  {
    case 0: // "No Effect"
      s3m_effect=0;
      s3m_param=gdm_param;
      break;

    case 1: // Portamento up
      s3m_effect='F'-'@';
      s3m_param=gdm_param;
      if(s3m_param>=0xE0)
        s3m_param=0xDF;
      break;

    case 2: // Portamento down
      s3m_effect='E'-'@';
      s3m_param=gdm_param;
      if(s3m_param>=0xE0)
        s3m_param=0xDF;
      break;

    case 3: // Glissando
      s3m_effect='G'-'@';
      s3m_param=gdm_param;
      break;

    case 4: // Vibrato
      s3m_effect='H'-'@';
      s3m_param=gdm_param;
      break;

    case 5: // Portamento + volume slide
      s3m_effect='L'-'@';
      s3m_param=gdm_param;
      break;

    case 6: // Vibrato + volume slide
      s3m_effect='K'-'@';
      s3m_param=gdm_param;
      break;

    case 7: // Tremolo
      s3m_effect='R'-'@';
      s3m_param=gdm_param;
      break;

    case 8: // Tremor/stupid effect nobody uses
      s3m_effect='I'-'@';
      s3m_param=gdm_param;
      break;

    case 9: // Sample offset
      s3m_effect='O'-'@';
      s3m_param=gdm_param;
      break;

    case 0xa: // Volume slide
      s3m_effect='D'-'@';
      s3m_param=gdm_param;
      break;

    case 0xb: // Order jump
      s3m_effect='B'-'@';
      s3m_param=gdm_param;
      break;

    case 0xd: // Pattern break
      s3m_effect='C'-'@';
      s3m_param=gdm_param;
      break;

    case 0xe: // EXTENDED EFFECTS, DO THIS LATER
      switch(gdm_param>>4)
      {
        case 0: // Set filter (AMIGA!)
          s3m_effect='S'-'@';
          s3m_param= 0x00 + (gdm_param & 15);
          break;
        case 1: // Fine slide up
          s3m_effect='F'-'@';
          s3m_param= 0xF0 + (gdm_param & 15);
          break;
        case 2: // Fine slide down
          s3m_effect='E'-'@';
          s3m_param= 0xF0 + (gdm_param & 15);
          break;
        case 3: // Glissando control
          s3m_effect='S'-'@';
          s3m_param= 0x10 + (gdm_param & 15);
          break;
        case 4: // Vibrato waveform
          s3m_effect='S'-'@';
          s3m_param= 0x30 + (gdm_param & 15);
          break;
        case 5: // Set finetune
          s3m_effect='S'-'@';
          s3m_param= 0x20 + (gdm_param & 15);
          break;
        case 6: // Pattern loop
          s3m_effect='S'-'@';
          s3m_param= 0xB0 + (gdm_param & 15);
          break;
        case 7: // Tremolo waveform
          s3m_effect='S'-'@';
          s3m_param= 0x40 + (gdm_param & 15);
          break;
        case 8: // Extra Fine slide up
          s3m_effect='F'-'@';
          s3m_param= 0xE0 + (gdm_param & 15);
          break;
        case 9: // Extra Fine slide down
          s3m_effect='E'-'@';
          s3m_param= 0xE0 + (gdm_param & 15);
          break;
        case 0xA: // Fine volume up
          s3m_effect='D'-'@';
          s3m_param= 0x0F + ((gdm_param & 15) << 4);
          break;
        case 0xB: // Fine volume down
          s3m_effect='D'-'@';
          s3m_param= 0xF0 + (gdm_param & 15);
          break;
        case 0xC: // Cut note after x ticks
          s3m_effect='S'-'@';
          s3m_param= 0xC0 + (gdm_param & 15);
          break;
        case 0xD: // Delay note for x ticks
          s3m_effect='S'-'@';
          s3m_param= 0xD0 + (gdm_param & 15);
          break;
        case 0xE: // Delay whole pattern, in ticks???!?
          s3m_effect='S'-'@';
          s3m_param= 0xE0 + (gdm_param & 15);
          break;
        case 0xF: // Invert loop?!? Funk repeat?!?
          s3m_effect='S'-'@';
          s3m_param= 0xF0 + (gdm_param & 15);
          break;
      }
      break;

    case 0xf: // "Set tempo", apparently sets speed
      s3m_effect='A'-'@';
      s3m_param=gdm_param;
      break;

    case 0x10: // Arpeggio
      s3m_effect='J'-'@';
      s3m_param=gdm_param;
      break;

    case 0x11: // Set internal flag, no idea what this is
      s3m_effect=0; // Convert to no effect
      s3m_param=gdm_param;
      break;

    case 0x12: // Retrigger note
      s3m_effect='Q'-'@';
      s3m_param=gdm_param;
      break;

    case 0x13: // Global volume, 0-63 in both s3m and gdm
      s3m_effect='V'-'@';
      s3m_param=gdm_param;
      break;

    case 0x14: // Fine vibrato, crashes mzx
      s3m_effect='U'-'@';
      s3m_param=gdm_param;
      break;

    case 0x1e: // Special
      if((gdm_param>>4)==8)
      {
        s3m_effect='S'-'@';
        s3m_param= 0x80 + (gdm_param & 15);
      }
      else
      {
        s3m_effect=0;
        s3m_param=gdm_param;
      }
      break;

    case 0x1f: // Set bpm, ie set tempo?!?
      s3m_effect='T'-'@';
      s3m_param=gdm_param;
      break;

    default:
      fprintf (stderr, "gdm2s3m: implemented effect! (contact ajs)\n");
      break;
  }

  *dest_effect = s3m_effect;
  *dest_param = s3m_param;
}

struct S3M_file *convert_gdm_to_s3m (struct GDM_file *gdm)
{
  struct S3M_file *s3m;
  int i, j;

  /* optimal channel settings for ST3 */
  uint8_t chansett[] = { 0,   8,   1,   9,   2,   10,  3,   11,
                         4,   12,  5,   13,  6,   14,  7,   15,
                         255, 255, 255, 255, 255, 255, 255, 255,
                         255, 255, 255, 255, 255, 255, 255, 255 };

  /* allocate s3m struct */
  s3m = malloc (sizeof (struct S3M_file));

  /* clear it */
  memset (s3m, 0, sizeof (struct S3M_file));

  /* copy module title (as much as possible) */
  memcpy (s3m->header.title, gdm->header.title, 27);

  /* pass over order, sample, pattern counts */
  if (gdm->order_len & 1)
    s3m->header.numorders = gdm->order_len;
  else
    s3m->header.numorders = gdm->order_len + 1;
  s3m->header.numinst = gdm->numsamples;
  s3m->header.numpatterns = gdm->numpatterns;

  /* pass over global volume, speed, tempo */
  s3m->header.global_vol = gdm->header.global_vol;
  s3m->header.speed = gdm->header.bpm;
  s3m->header.tempo = gdm->header.tempo;

  /* channel settings have no appreciable function */
  memcpy (s3m->header.chansett, chansett, 32);

  /* handle pan map */
  for (i = 0; i < 32; i++) {
    uint8_t g = gdm->header.panning[i];
    uint8_t *s = &s3m->panmap[i];

    if (g == 16) {     /* surround sound */
      *s = 8 + 0x20;
    }
    else if (g > 16) { /* disabled */
      *s = 0;
    }
    else {             /* normal panning */
      *s = g + 0x20;
    }
  }

  /* allocate order space */
  s3m->orders.patterns = malloc (s3m->header.numorders);
  memset (s3m->orders.patterns, 254, s3m->header.numorders);
  s3m->orders.patterns[s3m->header.numorders - 1] = 255;  // FIXME

  /* copy over order data */
  memcpy (s3m->orders.patterns, gdm->orders.patterns, gdm->order_len);

  /* allocate pattern space */
  s3m->patterns =
    malloc (sizeof (struct S3M_pattern) * s3m->header.numpatterns);

  /* copy over patterns */
  for (i = 0; i < s3m->header.numpatterns; i++) {
    struct S3M_pattern *pattern = &s3m->patterns[i];
    struct GDM_pattern *gpattern = &gdm->patterns[i];
    uint8_t *pdata = NULL, *backup = NULL;
    size_t plen = 0;

    /* big pattern expander and collapser */
    for (j = 0; j < gpattern->length; j++) {
      uint8_t vol = 0, norm = 0, note = 0, effect = 0;
      uint8_t b = 0, n, s, e, ep, se, sep, sv;

      /* it's a zero byte */
      if (gpattern->data[j] != 0) {
        /* copy first 5 bits over */
        b = gpattern->data[j] & 0x1F;

        /* see if we've got a note on */
        if (gpattern->data[j] & 0x20)
          note = 1;

        /* see if we've got an effect */
        if (gpattern->data[j] & 0x40)
          effect = 1;
      }

      /* copy over note data */
      if (note) {
        /* get note and sample */
        n = (gpattern->data[j + 1] & 0x7F) - 1;
        s = gpattern->data[j + 2];

        /* mask in note on */
        b |= 0x20;

        /* increment j */
        j += 2;
      }

      /* copy over effect */
      if (effect) {
        do
        {
          /* get effect number */
          e = gpattern->data[j + 1];

          /* get effect parameter */
          ep = gpattern->data[j + 2];

          /* check for volume event */
          if ((e & 0x1F) == 0xC) {
            /* we can't have more than one volume effect per note */
            if (vol == 1)
              break;
            vol = 1;

            /* mask on that we've got one */
            b |= 0x40;

            /* se for the volume */
            sv = ep;
          } else {
            /* we can't have more than one of these */
            if (norm == 1)
              break;
            norm = 1;

            /* mask on that we've got a real effect */
            b |= 0x80;

            /* convert effect */
            remap_effects (e & 0x1F, ep, &se, &sep);
          }

          /* move along */
          j += 2;
        }
        while (e & 0x20);
      }

      /* copy over */
      check_a_to_s (&backup, &plen, &b, &pdata, 1);

      /* write out note and sample */
      if (note) {
        check_a_to_s (&backup, &plen, &n, &pdata, 1);
        check_a_to_s (&backup, &plen, &s, &pdata, 1);
      }

      /* volume event */
      if (effect && vol) {
        check_a_to_s (&backup, &plen, &sv, &pdata, 1);
      }

      /* normal effect */
      if (effect && norm) {
        check_a_to_s (&backup, &plen, &se, &pdata, 1);
        check_a_to_s (&backup, &plen, &sep, &pdata, 1);
      }
    }

    /* copy over length */
    pattern->length = (uint16_t)plen;

    /* copy over data */
    pattern->data = backup;
  }

  /* allocate sample space */
  s3m->samples = malloc (sizeof (struct S3M_sample) * s3m->header.numinst);

  /* zero it all */
  memset (s3m->samples, 0, sizeof (struct S3M_sample) * s3m->header.numinst);

  /* copy over samples */
  for (i = 0; i < s3m->header.numinst; i++) {
    struct S3M_sample *sample = &s3m->samples[i];
    struct GDM_sample *gsample = &gdm->samples[i];

    /* ------------ HEADER ------------ */

    /* copy filename 8.3 format */
    memcpy (sample->header.filename, gsample->header.filename, 13);

    /* copy sample attributes */
    sample->header.length = gsample->header.length;
    sample->header.begin = gsample->header.begin;
    sample->header.end = gsample->header.end - 1;
    sample->header.volume = gsample->header.volume;
    sample->header.flags = gsample->header.flags & 1;
    sample->header.rate = gsample->header.rate;

    /* 16 bit sample */
    if (gsample->header.flags & 0x2)
      sample->header.flags |= 0x4;

    /* stereo sample */
    if (gsample->header.flags & 0x20)
      sample->header.flags |= 0x2;

    /* sample name (truncated) */
    memcpy (sample->header.name, gsample->header.name, 27);

    /* --------- SAMPLE DATA ---------- */

    /* allocate sample space */
    sample->data = malloc (sample->header.length);

    /* copy sample data */
    memcpy (sample->data, gsample->data, sample->header.length);
  }

  /* return new s3m struct */
  return s3m;
}

void free_s3m (struct S3M_file *s3m)
{
  int i;

  /* free up order data */
  free (s3m->orders.patterns);

  /* free up pattern data */
  for (i = 0; i < s3m->header.numpatterns; i++)
    free (s3m->patterns[i].data);
  free (s3m->patterns);

  /* free up sample data */
  for (i = 0; i < s3m->header.numinst; i++)
    free (s3m->samples[i].data);
  free (s3m->samples);

  /* free it */
  free (s3m);
}
