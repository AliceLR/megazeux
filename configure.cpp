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

// Config file options, which can be given either through config.txt
// or at the command line.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "configure.h"

void config_set_audio_buffer(config_info *conf, char *name, char *value)
{
  conf->buffer_size = strtol(value, NULL, 10);
}

// Default colors for color coding:
// 0 current line - 11
// 1 immediates - 10
// 2 characters - 14
// 3 colors - color box or 2
// 4 directions - 3
// 5 things - 11
// 6 params - 2
// 7 strings - 14
// 8 equalities - 0
// 9 conditions - 15
// 10 items - 11
// 11 extras - 7
// 12 commands and command fragments - 15


void config_ccode_colors(config_info *conf, char *name, char *value)
{
  conf->color_codes[4] = strtol(value, NULL, 10);
}

void config_ccode_commands(config_info *conf, char *name, char *value)
{
  conf->color_codes[13] = strtol(value, NULL, 10);
}

void config_ccode_conditions(config_info *conf, char *name, char *value)
{
  conf->color_codes[10] = strtol(value, NULL, 10);
}

void config_ccode_current_line(config_info *conf, char *name, char *value)
{
  conf->color_codes[0] = strtol(value, NULL, 10);
}

void config_ccode_directions(config_info *conf, char *name, char *value)
{
  conf->color_codes[5] = strtol(value, NULL, 10);
}

void config_ccode_equalities(config_info *conf, char *name, char *value)
{
  conf->color_codes[9] = strtol(value, NULL, 10);
}

void config_ccode_extras(config_info *conf, char *name, char *value)
{
  conf->color_codes[8] = strtol(value, NULL, 10);
}

void config_ccode_on(config_info *conf, char *name, char *value)
{
  conf->color_coding_on = strtol(value, NULL, 10);
}

void config_ccode_immediates(config_info *conf, char *name, char *value)
{
  int new_color = strtol(value, NULL, 10);
  conf->color_codes[1] = new_color;
  conf->color_codes[2] = new_color;
}

void config_ccode_items(config_info *conf, char *name, char *value)
{
  conf->color_codes[11] = strtol(value, NULL, 10);
}

void config_ccode_params(config_info *conf, char *name, char *value)
{
  conf->color_codes[7] = strtol(value, NULL, 10);
}

void config_ccode_strings(config_info *conf, char *name, char *value)
{
  conf->color_codes[8] = strtol(value, NULL, 10);
}

void config_ccode_things(config_info *conf, char *name, char *value)
{
  conf->color_codes[6] = strtol(value, NULL, 10);
}

void config_default_invald(config_info *conf, char *name, char *value)
{
  if(!strcasecmp(value, "ignore"))
  {
    conf->default_invalid_status = 1;
  }
  else

  if(!strcasecmp(value, "delete"))
  {
    conf->default_invalid_status = 2;
  }
  else

  if(!strcasecmp(value, "comment"))
  {
    conf->default_invalid_status = 3;
  }
}

void config_disassemble_extras(config_info *conf, char *name, char *value)
{
  conf->disassemble_extras = strtol(value, NULL, 10);
}

void config_disassemble_base(config_info *conf, char *name, char *value)
{
  int new_base = strtol(value, NULL, 10);

  if((new_base == 10) || (new_base == 16))
    conf->disassemble_base = new_base;
}

void config_set_resolution(config_info *conf, char *name, char *value)
{
  char *next;
  conf->resolution_x = strtol(value, &next, 10);
  conf->resolution_y = strtol(next + 1, NULL, 10);
}

void config_set_multiplier(config_info *conf, char *name, char *value)
{
  conf->height_multiplier = strtol(value, NULL, 10);
}

void config_set_fullscreen(config_info *conf, char *name, char *value)
{
  conf->fullscreen = strtol(value, NULL, 10);
}

void config_macro_five(config_info *conf, char *name, char *value)
{
  value[63] = 0;
  strcpy(conf->default_macros[4], value);
}

void config_macro_four(config_info *conf, char *name, char *value)
{
  value[63] = 0;
  strcpy(conf->default_macros[3], value);
}

void config_macro_one(config_info *conf, char *name, char *value)
{
  value[63] = 0;
  strcpy(conf->default_macros[0], value);
}

void config_macro_three(config_info *conf, char *name, char *value)
{
  value[63] = 0;
  strcpy(conf->default_macros[2], value);
}

void config_macro_two(config_info *conf, char *name, char *value)
{
  value[63] = 0;
  strcpy(conf->default_macros[1], value);
}

void config_set_music(config_info *conf, char *name, char *value)
{
  conf->music_on = strtol(value, NULL, 10);
}

void config_set_mod_volume(config_info *conf, char *name, char *value)
{
  int new_volume = strtol(value, NULL, 10);

  if(new_volume < 1)
    new_volume = 1;

  if(new_volume > 8)
    new_volume = 8;

  conf->music_volume = new_volume;
}

void config_set_mzx_speed(config_info *conf, char *name, char *value)
{
  int new_speed = strtol(value, NULL, 10);

  if(new_speed < 1)
    new_speed = 1;

  if(new_speed > 9)
    new_speed = 9;

  conf->mzx_speed = new_speed;
}

void config_set_pc_speaker(config_info *conf, char *name, char *value)
{
  conf->pc_speaker_on = strtol(value, NULL, 10);
}

void config_set_sam_volume(config_info *conf, char *name, char *value)
{
  int new_volume = strtol(value, NULL, 10);

  if(new_volume < 1)
    new_volume = 1;

  if(new_volume > 8)
    new_volume = 8;

  conf->sam_volume = new_volume;
}

void config_save_file(config_info *conf, char *name, char *value)
{
  strcpy(conf->default_save_name, value);
}

void config_startup_file(config_info *conf, char *name, char *value)
{
  strcpy(conf->startup_file, value);
}

config_entry config_options[] =
{
  { "audio_buffer", config_set_audio_buffer },
  { "ccode_colors", config_ccode_colors },
  { "ccode_commands", config_ccode_commands },
  { "ccode_conditions", config_ccode_conditions },
  { "ccode_current_line", config_ccode_current_line },
  { "ccode_directions", config_ccode_directions },
  { "ccode_equalities", config_ccode_equalities },
  { "ccode_extras", config_ccode_extras },
  { "ccode_immediates", config_ccode_immediates },
  { "ccode_items", config_ccode_items },
  { "ccode_params", config_ccode_params },
  { "ccode_strings", config_ccode_strings },
  { "ccode_things", config_ccode_things },
  { "color_coding_on", config_ccode_on },
  { "default_invalid_status", config_default_invald },
  { "disassemble_base", config_disassemble_base },
  { "disassemble_extras", config_disassemble_extras },
  { "force_height_multiplier", config_set_multiplier },
  { "force_resolution", config_set_resolution },
  { "fullscreen", config_set_fullscreen },
  { "macro_five", config_macro_five },
  { "macro_four", config_macro_four },
  { "macro_one", config_macro_one },
  { "macro_three", config_macro_three },
  { "macro_two", config_macro_two },
  { "music_on", config_set_music },
  { "music_volume", config_set_mod_volume },
  { "mzx_speed", config_set_mzx_speed },
  { "pc_speaker_on", config_set_pc_speaker },
  { "sample_volume", config_set_sam_volume },
  { "save_file", config_save_file },
  { "startup_file", config_startup_file }
};

const int num_config_options =
 sizeof(config_options) / sizeof(config_entry);

config_entry *find_option(char *name)
{
  int bottom = 0, top = num_config_options - 1, middle;
  int cmpval;
  config_entry *base = config_options;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    cmpval = strcasecmp(name, (base[middle]).option_name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return base + middle;
  }

  return NULL;
}

config_info default_options =
{
  // Video options
  0,
  640,
  350,
  1,

  // Audio options
  2048,
  7,
  7,
  1,
  1,

  // Game options
  "caverns.mzx",
  "saved.sav",
  4,

  // Robot editor options
  { 11, 10, 10, 14, 255, 3, 11, 2, 14, 0, 15, 11, 7, 15, 1, 2, 3 },
  1,
  1,
  10,
  1,
  { "char ", "color ", "goto ", "send ", ": playershot^" }
};

void default_config(config_info *conf)
{
  memcpy(conf, &default_options, sizeof(config_info));
}

void set_config_from_file(config_info *conf, char *conf_file_name)
{
  FILE *conf_file = fopen(conf_file_name, "rb");

  if(conf_file)
  {
    char line_buffer[256];
    char line_buffer_alternate[256];
    char current_char, *input_position, *output_position;
    char *equals_position;
    char *value;
    config_entry *current_option;

    while(fgets(line_buffer_alternate, 255, conf_file))
    {
      if(line_buffer_alternate[0] != '#')
      {
        input_position = line_buffer_alternate;
        output_position = line_buffer;
        equals_position = NULL;
        do
        {
          current_char = *input_position;

          if(!isspace(current_char))
          {
            if((current_char == '\\') &&
             (input_position[1] == 's'))
            {
              input_position++;
              current_char = ' ';
            }

            if(current_char == '=')
              equals_position = output_position;

            *output_position = current_char;
            output_position++;
          }
          input_position++;
        } while(current_char);

        if(equals_position)
        {
          *equals_position = 0;
          value = equals_position + 1;
        }
        else
        {
          value = "1";
        }

        if(line_buffer[0])
        {
          current_option = find_option(line_buffer);

          if(current_option)
            current_option->change_option(conf, line_buffer, value);
        }
      }
    }
  }
}

void set_config_from_command_line(config_info *conf, int argc,
 char *argv[])
{
  char line_buffer[256];
  char current_char, *input_position, *output_position;
  char *equals_position;
  char *value;
  config_entry *current_option;

  int i;

  for(i = 1; i < argc; i++)
  {
    input_position = argv[i];
    output_position = line_buffer;
    equals_position = NULL;

    do
    {
      current_char = *input_position;

      if((current_char == '\\') &&
       (input_position[1] == 's'))
      {
        input_position++;
        current_char = ' ';
      }

      if(current_char == '=')
        equals_position = output_position;

      *output_position = current_char;
      output_position++;
      input_position++;
    } while(current_char);

    if(equals_position)
    {
      *equals_position = 0;
      value = equals_position + 1;
    }
    else
    {
      value = "1";
    }

    if(line_buffer[0])
    {
      current_option = find_option(line_buffer);

      if(current_option)
        current_option->change_option(conf, line_buffer, value);
    }
  }
}

