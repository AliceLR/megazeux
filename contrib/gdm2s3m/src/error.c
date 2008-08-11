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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>

#include "error.h"

void error_handle (char *file, int line, error_code error)
{
  char msg[256];

  switch (error) {
    case FILE_OPEN:
      sprintf (msg, "problem opening file");
      break;

    case FILE_INVALID:
      sprintf (msg, "invalid file format");
      break;

    case OUT_OF_DATA:
      sprintf (msg, "conversion failure");
      break;

    case CONVERT_ERROR:
      sprintf (msg, "convertor ran out of data");
      break;

    default:
      sprintf (msg, "unknown error");
      break;
  }

  /* echo error string to output */
  fprintf (stderr, "%s:%d: %s.\n", file, line, msg);

  /* abnormal quit */
  exit (-1);
}
