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
#include "fsafeopen.h"
#include "util.h"
#include "sys/stat.h"

// Arch-specific config.
#ifdef CONFIG_NDS
#define VIDEO_OUTPUT_DEFAULT "nds"
#endif

#ifdef CONFIG_GP2X
#define VIDEO_OUTPUT_DEFAULT "gp2x"
#define AUDIO_BUFFER_SAMPLES 128
#endif

#ifdef CONFIG_PSP
#define FULLSCREEN_WIDTH_DEFAULT 640
#define FULLSCREEN_HEIGHT_DEFAULT 363
#define FORCE_BPP_DEFAULT 8
#define FULLSCREEN_DEFAULT 1
#endif

#ifdef CONFIG_WII
#define AUDIO_SAMPLE_RATE 48000
#define FULLSCREEN_DEFAULT 1
#define GL_VSYNC_DEFAULT 1
#ifdef CONFIG_SDL
#define VIDEO_OUTPUT_DEFAULT "software"
#define FULLSCREEN_WIDTH_DEFAULT 640
#define FULLSCREEN_HEIGHT_DEFAULT 480
#define FORCE_BPP_DEFAULT 16
#endif
#endif

#ifdef CONFIG_3DS
#define FORCE_BPP_DEFAULT 16
#endif

#ifdef ANDROID
#define FORCE_BPP_DEFAULT 16
#define FULLSCREEN_DEFAULT 1
#endif

// End arch-specific config.

#ifndef FORCE_BPP_DEFAULT
#define FORCE_BPP_DEFAULT 32
#endif

#ifndef GL_VSYNC_DEFAULT
#define GL_VSYNC_DEFAULT 0
#endif

#ifndef VIDEO_OUTPUT_DEFAULT
#define VIDEO_OUTPUT_DEFAULT "auto_glsl"
#endif

#ifndef AUDIO_BUFFER_SAMPLES
#define AUDIO_BUFFER_SAMPLES 1024
#endif

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 44100
#endif

#ifndef FULLSCREEN_WIDTH_DEFAULT
#define FULLSCREEN_WIDTH_DEFAULT -1
#endif

#ifndef FULLSCREEN_HEIGHT_DEFAULT
#define FULLSCREEN_HEIGHT_DEFAULT -1
#endif

#ifndef FULLSCREEN_DEFAULT
#define FULLSCREEN_DEFAULT 0
#endif

#ifdef CONFIG_UPDATER
#ifndef MAX_UPDATE_HOSTS
#define MAX_UPDATE_HOSTS 16
#endif

#ifndef UPDATE_HOST_COUNT
#define UPDATE_HOST_COUNT 3
#endif

#ifndef UPDATE_HOSTS
#define UPDATE_HOSTS {     \
 "updates.digitalmzx.net", \
 "updates.megazeux.org",   \
 "updates.megazeux.net",   \
}
#endif
#endif /* CONFIG_UPDATER */

static bool is_startup = false;

struct config_entry
{
  char option_name[OPTION_NAME_LEN];
  config_function change_option;
  bool allow_in_game_config;
};

#ifdef CONFIG_NETWORK

static void config_set_network_enabled(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->network_enabled = strtoul(value, NULL, 10);
}

static void config_set_socks_host(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  strncpy(conf->socks_host, value, 256);
  conf->socks_host[256 - 1] = 0;
}

static void config_set_socks_port(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->socks_port = strtoul(value, NULL, 10);
}

#endif // CONFIG_NETWORK

#ifdef CONFIG_UPDATER

static int host_configured = 0;

static void config_update_host(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  if(!host_configured || !conf->update_host_count)
  {
    int i;

    // Free the defaults, we don't want them anymore
    for(i = 0; i < conf->update_host_count; i++)
      free(conf->update_hosts[i]);

    conf->update_host_count = 0;
    host_configured = 1;
  }

  if(conf->update_host_count < MAX_UPDATE_HOSTS)
  {
    conf->update_hosts = crealloc(conf->update_hosts,
     sizeof(char *) * (conf->update_host_count + 1));

    conf->update_hosts[conf->update_host_count] = cmalloc(128);
    strncpy(conf->update_hosts[conf->update_host_count], value, 128);
    conf->update_hosts[conf->update_host_count][127] = 0;
    conf->update_host_count++;
  }
}

static void config_update_branch_pin(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  strncpy(conf->update_branch_pin, value, 256);
  conf->update_branch_pin[256 - 1] = 0;
}

static void config_update_auto_check(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  if(!strcmp(value, "off"))
  {
    conf->update_auto_check = UPDATE_AUTO_CHECK_OFF;
  }
  else

  if(!strcmp(value, "on"))
  {
    conf->update_auto_check = UPDATE_AUTO_CHECK_ON;
  }
  else

  if(!strcmp(value, "silent"))
  {
    conf->update_auto_check = UPDATE_AUTO_CHECK_SILENT;
  }
}

#endif // CONFIG_UPDATER

static void config_disassemble_extras(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->disassemble_extras = strtoul(value, NULL, 10);
}

static void config_disassemble_base(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned long new_base = strtoul(value, NULL, 10);

  if((new_base == 10) || (new_base == 16))
    conf->disassemble_base = new_base;
}

static void config_set_audio_buffer(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->audio_buffer_samples = strtoul(value, NULL, 10);
}

static void config_set_resolution(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  char *next;
  conf->resolution_width = strtoul(value, &next, 10);
  conf->resolution_height = strtoul(next + 1, NULL, 10);
}

static void config_set_fullscreen(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->fullscreen = strtoul(value, NULL, 10);
}

static void config_set_music(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->music_on = strtoul(value, NULL, 10);
}

static void config_set_mod_volume(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned long new_volume = strtoul(value, NULL, 10);
  conf->music_volume = CLAMP(new_volume, 1, 8);
}

static void config_set_mzx_speed(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned long new_speed = strtoul(value, NULL, 10);
  conf->mzx_speed = CLAMP(new_speed, 1, 16);
}

static void config_set_pc_speaker(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->pc_speaker_on = strtol(value, NULL, 10);
}

static void config_set_sam_volume(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned long new_volume = strtoul(value, NULL, 10);
  conf->sam_volume = CLAMP(new_volume, 1, 8);
}

static void config_save_file(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  snprintf(conf->default_save_name, 256, "%s", value);
}

static void config_startup_file(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // Split file from path; discard the path and save the file.
  split_path_filename(value, NULL, 0, conf->startup_file, 256);
}

static void config_startup_path(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  struct stat stat_info;
  if(stat(value, &stat_info) ||
   !S_ISDIR(stat_info.st_mode))
    return;

  snprintf(conf->startup_path, 256, "%s", value);
}

static void config_system_mouse(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->system_mouse = strtol(value, NULL, 10);
}

static void config_enable_oversampling(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->oversampling_on = strtol(value, NULL, 10);
}

static void config_resample_mode(struct config_info *conf, char *name,
 char *value, char *extended_data)
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

static void config_mp_resample_mode(struct config_info *conf, char *name,
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

static void joy_axis_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int joy_num, joy_axis;
  unsigned int joy_key_min, joy_key_max;

  sscanf(name, "joy%uaxis%u", &joy_num, &joy_axis);
  sscanf(value, "%u, %u", &joy_key_min, &joy_key_max);

  joy_num = CLAMP(joy_num, 1, 16);
  joy_axis = CLAMP(joy_axis, 1, 16);

  map_joystick_axis(joy_num - 1, joy_axis - 1, (enum keycode)joy_key_min,
   (enum keycode)joy_key_max);
}

static void joy_button_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int joy_num, joy_button;
  unsigned long joy_key;

  sscanf(name, "joy%ubutton%u", &joy_num, &joy_button);

  joy_key = (enum keycode)strtoul(value, NULL, 10);
  joy_num = CLAMP(joy_num, 1, 16);

  map_joystick_button(joy_num - 1, joy_button - 1, (enum keycode)joy_key);
}

static void joy_hat_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int joy_num;
  unsigned int joy_key_up, joy_key_down, joy_key_left, joy_key_right;

  sscanf(name, "joy%uhat", &joy_num);
  sscanf(value, "%u, %u, %u, %u", &joy_key_up, &joy_key_down,
   &joy_key_left, &joy_key_right);

  joy_num = CLAMP(joy_num, 1, 16);

  map_joystick_hat(joy_num - 1, (enum keycode)joy_key_up,
   (enum keycode)joy_key_down, (enum keycode)joy_key_left,
   (enum keycode)joy_key_right);
}

static void pause_on_unfocus(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  set_unfocus_pause(strtoul(value, NULL, 10) > 0);
}

static void include_config(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // This one's for the original include N form
  set_config_from_file(conf, name + 7);
}

static void include2_config(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // This one's for the include = N form
  set_config_from_file(conf, value);
}

static void config_set_sfx_volume(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned long new_volume = strtoul(value, NULL, 10);
  conf->pc_speaker_volume = CLAMP(new_volume, 1, 8);
}

static void config_mask_midchars(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->mask_midchars = strtoul(value, NULL, 10);
}

static void config_set_audio_freq(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->output_frequency = strtoul(value, NULL, 10);
}

static void config_force_bpp(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->force_bpp = strtoul(value, NULL, 10);
}

static void config_window_resolution(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  char *next;
  conf->window_width = strtoul(value, &next, 10);
  conf->window_height = strtoul(next + 1, NULL, 10);
}

static void config_set_video_output(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  strncpy(conf->video_output, value, 16);
  conf->video_output[15] = 0;
}

static void config_enable_resizing(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->allow_resize = strtoul(value, NULL, 10);
}

static void config_set_gl_filter_method(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  strncpy(conf->gl_filter_method, value, 16);
  conf->gl_filter_method[15] = 0;
}

static void config_set_gl_scaling_shader(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  strncpy(conf->gl_scaling_shader, value, 32);
  conf->gl_scaling_shader[31] = 0;
}

static void config_gl_vsync(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->gl_vsync = strtol(value, NULL, 10);
}

static void config_startup_editor(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->startup_editor = strtoul(value, NULL, 10);
}

static void config_standalone_mode(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->standalone_mode = strtoul(value, NULL, 10);
}

static void config_no_titlescreen(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  conf->no_titlescreen = strtoul(value, NULL, 10);
}

static void config_set_video_ratio(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  if(!strcasecmp(value, "classic"))
    conf->video_ratio = RATIO_CLASSIC_4_3;
  else if(!strcasecmp(value, "modern"))
    conf->video_ratio = RATIO_MODERN_64_35;
  else
    conf->video_ratio = RATIO_STRETCH;
}

static void config_set_num_buffered_events(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  Uint8 v = (Uint8)strtoul(value, NULL, 10);
  set_num_buffered_events(v);
}

static void config_max_simultaneous_samples(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  int v = MAX( strtol(value, NULL, 10), -1 );
  conf->max_simultaneous_samples = v;
}

/* NOTE: This is searched as a binary tree, the nodes must be
 *       sorted alphabetically, or they risk being ignored.
 */
static const struct config_entry config_options[] =
{
  { "audio_buffer", config_set_audio_buffer, false },
  { "audio_buffer_samples", config_set_audio_buffer, false },
  { "audio_sample_rate", config_set_audio_freq, false },
  { "disassemble_base", config_disassemble_base, false },
  { "disassemble_extras", config_disassemble_extras, false },
  { "enable_oversampling", config_enable_oversampling, false },
  { "enable_resizing", config_enable_resizing, false },
  { "force_bpp", config_force_bpp, false },
  { "fullscreen", config_set_fullscreen, false },
  { "fullscreen_resolution", config_set_resolution, false },
  { "gl_filter_method", config_set_gl_filter_method, false },
  { "gl_scaling_shader", config_set_gl_scaling_shader, true },
  { "gl_vsync", config_gl_vsync, false },
  { "include", include2_config, true },
  { "include*", include_config, true },
  { "joy!axis!", joy_axis_set, true },
  { "joy!button!", joy_button_set, true },
  { "joy!hat", joy_hat_set, true },
  { "mask_midchars", config_mask_midchars, false },
  { "max_simultaneous_samples", config_max_simultaneous_samples, false },
  { "modplug_resample_mode", config_mp_resample_mode, false },
  { "music_on", config_set_music, false },
  { "music_volume", config_set_mod_volume, false },
  { "mzx_speed", config_set_mzx_speed, true },
#ifdef CONFIG_NETWORK
  { "network_enabled", config_set_network_enabled, false },
#endif
  { "no_titlescreen", config_no_titlescreen, false },
  { "num_buffered_events", config_set_num_buffered_events, false },
  { "pause_on_unfocus", pause_on_unfocus, false },
  { "pc_speaker_on", config_set_pc_speaker, false },
  { "pc_speaker_volume", config_set_sfx_volume, false },
  { "resample_mode", config_resample_mode, false },
  { "sample_volume", config_set_sam_volume, false },
  { "save_file", config_save_file, false },
#ifdef CONFIG_NETWORK
  { "socks_host", config_set_socks_host, false },
  { "socks_port", config_set_socks_port, false },
#endif
  { "standalone_mode", config_standalone_mode, false },
  { "startup_editor", config_startup_editor, false },
  { "startup_file", config_startup_file, false },
  { "startup_path", config_startup_path, false },
  { "system_mouse", config_system_mouse, false },
#ifdef CONFIG_UPDATER
  { "update_auto_check", config_update_auto_check, false },
  { "update_branch_pin", config_update_branch_pin, false },
  { "update_host", config_update_host, false },
#endif
  { "video_output", config_set_video_output, false },
  { "video_ratio", config_set_video_ratio, false },
  { "window_resolution", config_window_resolution, false }
};

static const int num_config_options =
 sizeof(config_options) / sizeof(struct config_entry);

static const struct config_entry *find_option(char *name,
 const struct config_entry options[], int num_options)
{
  int cmpval, top = num_options - 1, middle, bottom = 0;
  const struct config_entry *base = options;

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

static const struct config_info default_options =
{
  // Video options
  FULLSCREEN_DEFAULT,           // fullscreen
  FULLSCREEN_WIDTH_DEFAULT,     // resolution_width
  FULLSCREEN_HEIGHT_DEFAULT,    // resolution_height
  640,                          // window_width
  350,                          // window_height
  1,                            // allow_resize
  VIDEO_OUTPUT_DEFAULT,         // video_output
  FORCE_BPP_DEFAULT,            // force_bpp
  RATIO_MODERN_64_35,           // video_ratio
  "linear",                     // opengl filter method
  "",                           // opengl default scaling shader
  GL_VSYNC_DEFAULT,             // opengl vsync mode

  // Audio options
  AUDIO_SAMPLE_RATE,            // output_frequency
  AUDIO_BUFFER_SAMPLES,         // audio_buffer_samples
  0,                            // oversampling_on
  1,                            // resample_mode
  2,                            // modplug_resample_mode
  -1,                           // max_simultaneous_samples
  8,                            // music_volume
  8,                            // sam_volume
  8,                            // pc_speaker_volume
  1,                            // music_on
  1,                            // pc_speaker_on

  // Game options
  "",                           // startup_path
  "caverns.mzx",                // startup_file
  "saved.sav",                  // default_save_name
  4,                            // mzx_speed
  1,                            // disassemble_extras
  10,                           // disassemble_base
  0,                            // startup_editor
  0,                            // standalone_mode
  0,                            // no_titlescreen

  1,                            // mask_midchars
  false,                        // system_mouse

#ifdef CONFIG_NETWORK
  true,                         // network_enabled
  "",                           // socks_host
  1080,                         // socks_port
#endif
#if defined(CONFIG_UPDATER)
  UPDATE_HOST_COUNT,            // update_host_count
  NULL,                         // update_hosts
#if defined(CONFIG_DEBYTECODE)
  "Debytecode",                 // update_branch_pin
#else /* !CONFIG_DEBYTECODE */
  "Stable",                     // update_branch_pin
#endif /* !CONFIG_DEBYTECODE */
  UPDATE_AUTO_CHECK_SILENT,     // update_auto_check
  0,                            // update_available
#endif /* CONFIG_UPDATER */
};

static int config_change_option(void *conf, char *name,
 char *value, char *extended_data)
{
  const struct config_entry *current_option = find_option(name,
   config_options, num_config_options);

  if(current_option)
  {
    if(current_option->allow_in_game_config || is_startup)
    {
      current_option->change_option(conf, name, value, extended_data);
      return 1;
    }
  }
  return 0;
}

__editor_maybe_static void __set_config_from_file(
 find_change_option find_change_handler, void *conf, const char *conf_file_name)
{
  char current_char, *input_position, *output_position, *use_extended_buffer;
  int line_size, extended_size, extended_allocate_size = 512;
  char line_buffer_alternate[256], line_buffer[256];
  int extended_buffer_offset, peek_char;
  char *extended_buffer;
  char *equals_position, *value;
  FILE *conf_file;

  conf_file = fopen_unsafe(conf_file_name, "rb");
  if(!conf_file)
    return;

  extended_buffer = cmalloc(extended_allocate_size);

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

        if(!isspace((int)current_char))
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
            line_size = (int)strlen(line_buffer_alternate);
            line_buffer_alternate[line_size] = '\n';
            line_size++;

            extended_size += line_size;
            if(extended_size >= extended_allocate_size)
            {
              extended_allocate_size *= 2;
              extended_buffer = crealloc(extended_buffer,
                extended_allocate_size);
            }

            strcpy(extended_buffer + extended_buffer_offset,
              line_buffer_alternate);
            extended_buffer_offset += line_size;
          }

          peek_char = fgetc(conf_file);
        }
        ungetc(peek_char, conf_file);

        find_change_handler(conf, line_buffer, value, use_extended_buffer);
      }
    }
  }

  free(extended_buffer);
  fclose(conf_file);
}

__editor_maybe_static void __set_config_from_command_line(
 find_change_option find_change_handler, void *conf, int *argc, char *argv[])
{
  char current_char, *input_position, *output_position;
  char *equals_position, line_buffer[256], *value;
  int i = 1;
  int j;

  while(i < *argc)
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

    if(equals_position && line_buffer[0])
    {
      *equals_position = 0;
      value = equals_position + 1;

      if(find_change_handler(conf, line_buffer, value, NULL))
      {
        // Found the option; remove it from argv and make sure i stays the same
        for(j = i; j < *argc - 1; j++)
          argv[j] = argv[j + 1];
        (*argc)--;
        i--;
      }
      // Otherwise, leave it for the editor config.
    }

    i++;
  }
}

void set_config_from_file(struct config_info *conf, const char *conf_file_name)
{
  __set_config_from_file(config_change_option, conf, conf_file_name);
}

void set_config_from_file_startup(struct config_info *conf,
 const char *conf_file_name)
{
  is_startup = true;
  set_config_from_file(conf, conf_file_name);
  is_startup = false;
}

void default_config(struct config_info *conf)
{
  memcpy(conf, &default_options, sizeof(struct config_info));

#if defined(CONFIG_UPDATER)
  // No update hosts?  Add defaults!
  if(!conf->update_hosts)
  {
    char **update_hosts = cmalloc(sizeof(char *) * UPDATE_HOST_COUNT);
    const char *def_hosts[] = UPDATE_HOSTS;
    int i;

    for(i = 0; i < UPDATE_HOST_COUNT; i++)
    {
      update_hosts[i] = cmalloc(strlen(def_hosts[i]) + 1);
      strcpy(update_hosts[i], def_hosts[i]);
    }

    conf->update_host_count = UPDATE_HOST_COUNT;
    conf->update_hosts = update_hosts;
  }
#endif //CONFIG_UPDATER
}

void set_config_from_command_line(struct config_info *conf,
 int *argc, char *argv[])
{
  is_startup = true;
  __set_config_from_command_line(config_change_option, conf, argc, argv);
  is_startup = false;
}

void free_config(struct config_info *conf)
{
#ifdef CONFIG_UPDATER
  // Auto-Updater hosts
  if(conf->update_hosts)
  {
    int i;

    for(i = 0; i < conf->update_host_count; i++)
      free(conf->update_hosts[i]);

    free(conf->update_hosts);
    conf->update_hosts = NULL;
  }
#endif
}
