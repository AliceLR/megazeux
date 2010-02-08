/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// Config file options, which can be given either through config.txt
// or at the command line.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "configure.h"
#include "counter.h"
#include "event.h"
#include "rasm.h"
#include "macro.h"
#include "fsafeopen.h"

#if defined(CONFIG_NDS)
#define VIDEO_OUTPUT_DEFAULT "nds"
#elif defined(CONFIG_GP2X)
#define VIDEO_OUTPUT_DEFAULT "gp2x"
#define AUDIO_BUFFER_SIZE 128
#elif defined(CONFIG_PSP)
#define FORCE_BPP_DEFAULT 8
#endif

#ifndef FORCE_BPP_DEFAULT
#define FORCE_BPP_DEFAULT 32
#endif

#ifndef VIDEO_OUTPUT_DEFAULT
#define VIDEO_OUTPUT_DEFAULT "software"
#endif

#ifndef AUDIO_BUFFER_SIZE
#define AUDIO_BUFFER_SIZE 4096
#endif

static void config_set_audio_buffer(config_info *conf, char *name, char *value,
 char *extended_data)
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

static void config_ccode_colors(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[4] = strtol(value, NULL, 10);
}

static void config_ccode_commands(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[13] = strtol(value, NULL, 10);
}

static void config_ccode_conditions(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[10] = strtol(value, NULL, 10);
}

static void config_ccode_current_line(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[0] = strtol(value, NULL, 10);
}

static void config_ccode_directions(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[5] = strtol(value, NULL, 10);
}

static void config_ccode_equalities(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[9] = strtol(value, NULL, 10);
}

static void config_ccode_extras(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[8] = strtol(value, NULL, 10);
}

static void config_ccode_on(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_coding_on = strtol(value, NULL, 10);
}

static void config_ccode_immediates(config_info *conf, char *name, char *value,
 char *extended_data)
{
  int new_color = strtol(value, NULL, 10);
  conf->color_codes[1] = new_color;
  conf->color_codes[2] = new_color;
}

static void config_ccode_items(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[11] = strtol(value, NULL, 10);
}

static void config_ccode_params(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[7] = strtol(value, NULL, 10);
}

static void config_ccode_strings(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[8] = strtol(value, NULL, 10);
}

static void config_ccode_things(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->color_codes[6] = strtol(value, NULL, 10);
}

static void config_default_invald(config_info *conf, char *name, char *value,
 char *extended_data)
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

static void config_disassemble_extras(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->disassemble_extras = strtol(value, NULL, 10);
}

static void config_disassemble_base(config_info *conf, char *name, char *value,
 char *extended_data)
{
  int new_base = strtol(value, NULL, 10);

  if((new_base == 10) || (new_base == 16))
    conf->disassemble_base = new_base;
}

static void config_set_resolution(config_info *conf, char *name, char *value,
 char *extended_data)
{
  char *next;
  conf->resolution_width = strtol(value, &next, 10);
  conf->resolution_height = strtol(next + 1, NULL, 10);
}

static void config_set_fullscreen(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->fullscreen = strtol(value, NULL, 10);
}

static void config_macro(config_info *conf, char *name, char *value,
 char *extended_data)
{
  char *macro_name = name + 6;

  if(isdigit(macro_name[0]) && !macro_name[1] &&
   !extended_data)
  {
    int macro_num = macro_name[0] - 0x31;
    value[63] = 0;
    strcpy(conf->default_macros[macro_num], value);
  }
  else
  {
    if(extended_data)
      add_ext_macro(conf, macro_name, extended_data, value);
  }
}

static void config_set_music(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->music_on = strtol(value, NULL, 10);
}

static void config_set_mod_volume(config_info *conf, char *name, char *value,
 char *extended_data)
{
  int new_volume = strtol(value, NULL, 10);

  if(new_volume < 1)
    new_volume = 1;

  if(new_volume > 8)
    new_volume = 8;

  conf->music_volume = new_volume;
}

static void config_set_mzx_speed(config_info *conf, char *name, char *value,
 char *extended_data)
{
  int new_speed = strtol(value, NULL, 10);

  if(new_speed < 1)
    new_speed = 1;

  if(new_speed > 9)
    new_speed = 9;

  conf->mzx_speed = new_speed;
}

static void config_set_pc_speaker(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->pc_speaker_on = strtol(value, NULL, 10);
}

static void config_set_sam_volume(config_info *conf, char *name, char *value,
 char *extended_data)
{
  int new_volume = strtol(value, NULL, 10);

  if(new_volume < 1)
    new_volume = 1;

  if(new_volume > 8)
    new_volume = 8;

  conf->sam_volume = new_volume;
}

static void config_save_file(config_info *conf, char *name, char *value,
 char *extended_data)
{
  strncpy(conf->default_save_name, value, 256);
}

static void config_startup_editor(config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->startup_editor = strtol(value, NULL, 10);
}

static void config_startup_file(config_info *conf, char *name, char *value,
 char *extended_data)
{
  strncpy(conf->startup_file, value, 256);
}

static void config_enable_oversampling(config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->oversampling_on = strtol(value, NULL, 10);
}

static void config_resample_mode(config_info *conf, char *name, char *value,
 char *extended_data)
{
  if(!strcasecmp(value, "none"))
  {
    conf->resample_mode = 0;
  }
  else

  if(!strcasecmp(value, "linear"))
  {
    conf->resample_mode = 1;
  }
  else

  if(!strcasecmp(value, "cubic"))
  {
    conf->resample_mode = 2;
  }
}

static void config_mp_resample_mode(config_info *conf, char *name,
 char *value, char *extended_data)
{
  if(!strcasecmp(value, "none"))
  {
    conf->modplug_resample_mode = 0;
  }
  else

  if(!strcasecmp(value, "linear"))
  {
    conf->modplug_resample_mode = 1;
  }
  else

  if(!strcasecmp(value, "cubic"))
  {
    conf->modplug_resample_mode = 2;
  }
  else

  if(!strcasecmp(value, "fir"))
  {
    conf->modplug_resample_mode = 3;
  }
}

static void bedit_hhelp(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->bedit_hhelp = strtol(value, NULL, 10);
}

static void redit_hhelp(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->redit_hhelp = strtol(value, NULL, 10);
}

static void backup_count(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->backup_count = strtol(value, NULL, 10);
}

static void backup_interval(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->backup_interval = strtol(value, NULL, 10);
}

static void backup_name(config_info *conf, char *name, char *value,
 char *extended_data)
{
  strncpy(conf->backup_name, value, 256);
}

static void backup_ext(config_info *conf, char *name, char *value,
 char *extended_data)
{
  strncpy(conf->backup_ext, value, 256);
}

static void joy_axis_set(config_info *conf, char *name, char *value,
 char *extended_data)
{
  int joy_num, joy_axis;
  int joy_key_min, joy_key_max;

  sscanf(name, "joy%daxis%d", &joy_num, &joy_axis);
  sscanf(value, "%d, %d", &joy_key_min, &joy_key_max);

  if(joy_num < 1)
    joy_num = 1;

  if(joy_num > 16)
    joy_num = 16;

  if(joy_axis < 1)
    joy_axis = 1;

  if(joy_axis > 16)
    joy_axis = 16;

  map_joystick_axis(joy_num - 1, joy_axis - 1, (keycode)joy_key_min,
   (keycode)joy_key_max);
}

static void joy_button_set(config_info *conf, char *name, char *value,
 char *extended_data)
{
  int joy_num, joy_button;
  int joy_key;

  sscanf(name, "joy%dbutton%d", &joy_num, &joy_button);
  joy_key = (keycode)strtol(value, NULL, 10);

  if(joy_num < 1)
    joy_num = 1;

  if(joy_num > 16)
    joy_num = 16;

  map_joystick_button(joy_num - 1, joy_button - 1, (keycode)joy_key);
}

static void pause_on_unfocus(config_info *conf, char *name, char *value,
 char *extended_data)
{
  int int_val = strtol(value, NULL, 10);

  set_refocus_pause(int_val);
}

static void include_config(config_info *conf, char *name, char *value,
 char *extended_data)
{
  // This one's for the original include N form
  set_config_from_file(conf, name + 7);
}

static void include2_config(config_info *conf, char *name, char *value,
 char *extended_data)
{
  // This one's for the include = N form
  set_config_from_file(conf, value);
}

static void config_set_sfx_volume(config_info *conf, char *name, char *value,
 char *extended_data)
{
  int new_volume = strtol(value, NULL, 10);

  if(new_volume < 1)
    new_volume = 1;

  if(new_volume > 8)
    new_volume = 8;

  conf->pc_speaker_volume = new_volume;
}

static void config_mask_midchars(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->mask_midchars = strtol(value, NULL, 10);
}

static void config_set_audio_freq(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->output_frequency = strtol(value, NULL, 10);
}

static void config_force_bpp(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->force_bpp = strtol(value, NULL, 10);
}

static void config_window_resolution(config_info *conf, char *name, char *value,
 char *extended_data)
{
  char *next;
  conf->window_width = strtol(value, &next, 10);
  conf->window_height = strtol(next + 1, NULL, 10);
}

static void config_set_video_output(config_info *conf, char *name, char *value,
 char *extended_data)
{
  strncpy(conf->video_output, value, 16);
}

static void config_enable_resizing(config_info *conf, char *name, char *value,
 char *extended_data)
{
  conf->allow_resize = strtol(value, NULL, 10);
}

static void config_set_gl_filter_method(config_info *conf, char *name,
 char *value, char *extended_data)
{
  strncpy(conf->gl_filter_method, value, 16);
}

static void config_editor_space_toggles(config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->editor_space_toggles = strtol(value, NULL, 10);
}

static void config_gl_vsync(config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->gl_vsync = strtol(value, NULL, 10);
}

/* FAT NOTE: This is searched as a binary tree, the nodes must be
 *           sorted alphabetically, or they risk being ignored.
 */
static config_entry config_options[] =
{
  { "audio_buffer", config_set_audio_buffer },
  { "audio_sample_rate", config_set_audio_freq },
  { "backup_count", backup_count },
  { "backup_ext", backup_ext },
  { "backup_interval", backup_interval },
  { "backup_name", backup_name },
  { "board_editor_hide_help", bedit_hhelp },
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
  { "editor_space_toggles", config_editor_space_toggles },
  { "enable_oversampling", config_enable_oversampling },
  { "enable_resizing", config_enable_resizing },
  { "force_bpp", config_force_bpp },
  { "force_resolution", config_set_resolution }, /* backwards compatibility */
  { "fullscreen", config_set_fullscreen },
  { "fullscreen_resolution", config_set_resolution },
  { "gl_filter_method", config_set_gl_filter_method },
  { "gl_vsync", config_gl_vsync },
  { "include", include2_config },
  { "include*", include_config },
  { "joy!axis!", joy_axis_set },
  { "joy!button!", joy_button_set },
  { "macro_*", config_macro },
  { "mask_midchars", config_mask_midchars },
  { "modplug_resample_mode", config_mp_resample_mode },
  { "music_on", config_set_music },
  { "music_volume", config_set_mod_volume },
  { "mzx_speed", config_set_mzx_speed },
  { "pause_on_unfocus", pause_on_unfocus },
  { "pc_speaker_on", config_set_pc_speaker },
  { "pc_speaker_volume", config_set_sfx_volume },
  { "resample_mode", config_resample_mode },
  { "robot_editor_hide_help", redit_hhelp },
  { "sample_volume", config_set_sam_volume },
  { "save_file", config_save_file },
  { "startup_editor", config_startup_editor },
  { "startup_file", config_startup_file },
  { "video_output", config_set_video_output },
  { "window_resolution", config_window_resolution }
};

const int num_config_options =
 sizeof(config_options) / sizeof(config_entry);

static config_entry *find_option(char *name)
{
  int bottom = 0, top = num_config_options - 1, middle;
  int cmpval;
  config_entry *base = config_options;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    cmpval = match_function_counter(name, (base[middle]).option_name);

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

// FIXME: Use C99 initializers?
static config_info default_options =
{
  // Video options
  0,                            // fullscreen
  640,                          // resolution_width
  480,                          // resolution_height
  640,                          // window_width
  350,                          // window_height
  0,                            // allow_resize
  VIDEO_OUTPUT_DEFAULT,         // video_output
  FORCE_BPP_DEFAULT,            // force_bpp
  "linear",                     // opengl filter method
  0,                            // opengl vsync mode

  // Audio options
  44100,                        // output_frequency
  AUDIO_BUFFER_SIZE,            // buffer_size
  0,                            // oversampling_on
  1,                            // resample_mode
  1,                            // modplug_resample_mode
  8,                            // music_volume
  8,                            // sam_volume
  8,                            // pc_speaker_volume
  1,                            // music_on
  1,                            // pc_speaker_on

  // Game options
  "caverns.mzx",                // startup_file
  "saved.sav",                  // default_save_name
  4,                            // mzx_speed
  0,                            // startup_editor

  // Board editor options
  0,                            // editor_space_toggles
  0,				// board_editor_hide_help

  // Robot editor options
  { 11, 10, 10, 14, 255, 3, 11, 2, 14, 0, 15, 11, 7, 15, 1, 2, 3 },
  1,                            // color_coding_on
  1,                            // disassemble_extras
  10,                           // disassemble_base
  1,                            // default_invalid_status
  { "char ", "color ", "goto ", "send ", ": playershot^" },
  0,                            // robot_editor_hide_help

  // Backup options
  3,                            // backup_count
  60,                           // backup_interval
  "backup",                     // backup_name
  ".mzx",                       // backup_ext

  // Macro options
  0,                            // num_extended_macros
  0,
  NULL,

  // Misc options
  1
};

void default_config(config_info *conf)
{
  memcpy(conf, &default_options, sizeof(config_info));
}

void set_config_from_file(config_info *conf, const char *conf_file_name)
{
  FILE *conf_file = fopen(conf_file_name, "rb");

  if(conf_file)
  {
    char line_buffer[256];
    char line_buffer_alternate[256];
    char *extended_buffer = malloc(512);
    char current_char, *input_position, *output_position;
    char *equals_position;
    char *value;
    config_entry *current_option;
    int line_size;
    int extended_size;
    int extended_allocate_size = 512;
    int peek_char;
    int extended_buffer_offset;
    char *use_extended_buffer;

    while(fsafegets(line_buffer_alternate, 255, conf_file))
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

            if((current_char == '=') && (equals_position == NULL))
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
          value = (char *)"1";
        }

        if(line_buffer[0])
        {
          // There might be extended information too - get it.
          peek_char = fgetc(conf_file);
          extended_size = 0;
          extended_buffer_offset = 0;
          use_extended_buffer = NULL;

          while((peek_char == ' ') || (peek_char == '\t'))
          {
            // Extended data line
            use_extended_buffer = extended_buffer;
            if(fsafegets(line_buffer_alternate, 254, conf_file))
            {
              line_size = strlen(line_buffer_alternate);
              line_buffer_alternate[line_size] = '\n';
              line_size++;

              extended_size += line_size;
              if(extended_size >= extended_allocate_size)
              {
                extended_allocate_size *= 2;
                extended_buffer = realloc(extended_buffer,
                 extended_allocate_size);
              }

              strcpy(extended_buffer + extended_buffer_offset,
               line_buffer_alternate);
              extended_buffer_offset += line_size;
            }

            peek_char = fgetc(conf_file);
          }
          ungetc(peek_char, conf_file);

          current_option = find_option(line_buffer);

          if(current_option)
          {
            current_option->change_option(conf, line_buffer, value,
             use_extended_buffer);
          }
        }
      }
    }

    free(extended_buffer);

    fclose(conf_file);
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

      if((current_char == '=') && (equals_position == NULL))
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
      value = (char *)"1";
    }

    if(line_buffer[0])
    {
      current_option = find_option(line_buffer);

      if(current_option)
        current_option->change_option(conf, line_buffer, value, NULL);
    }
  }
}



