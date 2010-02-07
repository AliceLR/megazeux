/**
 *  This software is a "lite" version of gdm2s3m, for Exophase's MegaZeux.
 *
 *  Copyright (C) 2003-2005  Alistair John Strachan  (alistair@devzero.co.uk)
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
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "gdm.h"
#include "s3m.h"
#include "error.h"
#include "utility.h"

int convert_gdm_s3m (const char *gdmfile, const char *s3mfile)
{
  struct GDM_file *gdm;
  struct S3M_file *s3m;
  uint32_t filesize;
  uint8_t *stream;
  FILE *handle;

  /* open module */
  if ((handle = fopen (gdmfile, "rb")) == NULL)
    return -1;

  /* get file size */
  fseek (handle, 0, SEEK_END);
  filesize = (uint32_t) ftell (handle);
  fseek (handle, 0, SEEK_SET);

  /* allocate space */
  stream = malloc (filesize);

  /* stream into memory */
  if (fread (stream, 1, filesize, handle) != filesize)
    return -2;

  /* close file */
  fclose (handle);

  /* try to load it */
  gdm = load_gdm (stream, filesize);

  /* free up stream */
  free (stream);

  /* do convert routine */
  s3m = convert_gdm_to_s3m (gdm);

  /* free up gdm */
  free_gdm (gdm);

  /* open up s3m file */
  if ((handle = fopen (s3mfile, "wb")) == NULL)
    return -1;

  /* save s3m file */
  stream = save_s3m (s3m, &filesize);

  /* free up s3m */
  free_s3m (s3m);

  /* write out file */
  if (fwrite (stream, 1, filesize, handle) != filesize)
    return -3;

  /* close file */
  fclose (handle);

  /* free up the stream */
  free (stream);

  /* all ok */
  return 0;
}
