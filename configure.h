/* $Id$
 * MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "SDL.h"

typedef struct _config_info config_info;

struct _config_info
{
  // Video options
  int fullscreen;
  int resolution_x;
  int resolution_y;
  int height_multiplier;

  // Audio options
  int buffer_size;
	int oversampling_on;
	int resampling_mode;
  int music_volume;
  int sam_volume;
  int music_on;
  int pc_speaker_on;

  // Game options
  char startup_file[64];
  char default_save_name[64];
  int mzx_speed;

  // Robot editor options
  char color_codes[32];
  int color_coding_on;
  int disassemble_extras;
  int disassemble_base;
  int default_invalid_status;
  char default_macros[5][64];
};

typedef void (* config_function)(config_info *conf,
 char *name, char *value);

typedef struct
{
  char option_name[32];
  config_function change_option;
} config_entry;

config_entry *find_option(char *name);
void set_config_from_file(config_info *conf, char *conf_file_name);
void default_config(config_info *conf);
void set_config_from_command_line(config_info *conf, int argc,
 char *argv[]);

#endif
