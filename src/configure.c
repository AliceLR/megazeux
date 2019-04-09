/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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
#include "configure_option.h"
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

static char *default_update_hosts[] =
{
  (char *)"updates.digitalmzx.net",
  (char *)"updates.megazeux.org",
  (char *)"updates.megazeux.net",
};

#ifndef UPDATE_BRANCH_PIN
#ifdef CONFIG_DEBYTECODE
#define UPDATE_BRANCH_PIN "Debytecode"
#else
#define UPDATE_BRANCH_PIN "Stable"
#endif
#endif

#endif /* CONFIG_UPDATER */

static boolean is_startup = false;

static struct config_info user_conf;

static const struct config_info user_conf_default =
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
  CONFIG_GL_FILTER_LINEAR,      // opengl filter method
  "",                           // opengl default scaling shader
  GL_VSYNC_DEFAULT,             // opengl vsync mode
  true,                         // allow screenshots

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
  true,                         // music_on
  true,                         // pc_speaker_on

  // Event options
  true,                         // allow_gamecontroller
  false,                        // pause_on_unfocus
  1,                            // num_buffered_events

  // Game options
  "",                           // startup_path
  "caverns.mzx",                // startup_file
  "saved.sav",                  // default_save_name
  4,                            // mzx_speed
  ALLOW_CHEATS_NEVER,           // allow_cheats
  false,                        // startup_editor
  false,                        // standalone_mode
  false,                        // no_titlescreen
  false,                        // system_mouse

  // Editor options
  true,                         // mask_midchars

#ifdef CONFIG_NETWORK
  true,                         // network_enabled
  "",                           // socks_host
  1080,                         // socks_port
#endif
#if defined(CONFIG_UPDATER)
  UPDATE_HOST_COUNT,            // update_host_count
  default_update_hosts,         // update_hosts
  UPDATE_BRANCH_PIN,            // update_branch_pin
  UPDATE_AUTO_CHECK_SILENT,     // update_auto_check
#endif /* CONFIG_UPDATER */
};

/**
 * These arrays contain allowed values for certain options. Pointers to these
 * arrays are generated automatically with the option name in the config struct.
 */

static const struct mapped_enum_entry boolean_values[] =
{
  { "0", false },
  { "1", true }
};

static const struct mapped_enum_entry allow_cheats_values[] =
{
  { "0", ALLOW_CHEATS_NEVER },
  { "1", ALLOW_CHEATS_ALWAYS },
  { "mzxrun", ALLOW_CHEATS_MZXRUN }
};

static const struct mapped_enum_entry force_bpp_values[] =
{
  { "8", 8 },
  { "16", 16 },
  { "32", 32 }
};

static const struct mapped_enum_entry gl_filter_method_values[] =
{
  { "nearest", CONFIG_GL_FILTER_NEAREST },
  { "linear", CONFIG_GL_FILTER_LINEAR }
};

static const struct mapped_enum_entry gl_vsync_values[] =
{
  { "-1", -1 },
  { "0", 0 },
  { "1", 1 },
  { "default", -1 }
};

static const struct mapped_enum_entry modplug_resample_mode_values[] =
{
  { "none", 0 },
  { "linear", 1 },
  { "cubic", 2 },
  { "fir", 3 }
};

static const struct mapped_enum_entry resample_mode_values[] =
{
  { "none", 0 },
  { "linear", 1 },
  { "cubic", 2 }
};

static const struct mapped_enum_entry system_mouse_values[] =
{
  { "0", 0 },
  { "1", 1 }
};

static const struct mapped_enum_entry update_auto_check_values[] =
{
  { "0", UPDATE_AUTO_CHECK_OFF },
  { "1", UPDATE_AUTO_CHECK_ON },
  { "off", UPDATE_AUTO_CHECK_OFF },
  { "on", UPDATE_AUTO_CHECK_ON },
  { "silent", UPDATE_AUTO_CHECK_SILENT }
};

static const struct mapped_enum_entry video_ratio_values[] =
{
  { "classic", RATIO_CLASSIC_4_3 },
  { "modern", RATIO_MODERN_64_35 },
  { "stretch", RATIO_STRETCH }
};

struct config_entry
{
  char option_name[OPTION_NAME_LEN];
  struct mapped_field dest;
  boolean allow_in_game_config;
};

typedef void (*config_function)(struct config_info *conf, char *name,
 char *value, char *extended_data);

#ifdef CONFIG_UPDATER

static void config_update_host(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  if(!conf->update_hosts || conf->update_hosts == default_update_hosts)
  {
    conf->update_hosts = ccalloc(MAX_UPDATE_HOSTS, sizeof(char *));
    conf->update_host_count = 0;
  }

  if(conf->update_host_count < MAX_UPDATE_HOSTS)
  {
    conf->update_hosts[conf->update_host_count] = cmalloc(strlen(value) + 1);
    strcpy(conf->update_hosts[conf->update_host_count], value);
    conf->update_host_count++;
  }
}

#endif // CONFIG_UPDATER

static void config_set_resolution(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  char *next;
  conf->resolution_width = strtoul(value, &next, 10);
  conf->resolution_height = strtoul(next + 1, NULL, 10);
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
  if(stat(value, &stat_info) || !S_ISDIR(stat_info.st_mode))
    return;

  snprintf(conf->startup_path, 256, "%s", value);
}

#define JOY_ENUM "%15[0-9A-Za-z_]"

static void joy_axis_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int joy_num, joy_axis;
  char key_min[16];
  char key_max[16];
  int read = 0;

  if(sscanf(name, "joy%uaxis%u%n", &joy_num, &joy_axis, &read) != 2)
    return;

  if(sscanf(value, JOY_ENUM ", " JOY_ENUM "%n", key_min, key_max, &read) != 2
   || (value[read] != 0))
    return;

  // Right now do a global binding at startup and a game binding otherwise.
  joystick_map_axis(joy_num - 1, joy_axis - 1, key_min, key_max, is_startup);
}

static void joy_button_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int joy_num, joy_button;
  char key[16];
  int read = 0;

  if(sscanf(name, "joy%ubutton%u", &joy_num, &joy_button) != 2)
    return;

  if(sscanf(value, JOY_ENUM "%n", key, &read) != 1 || (value[read] != 0))
    return;

  // Right now do a global binding at startup and a game binding otherwise.
  joystick_map_button(joy_num - 1, joy_button - 1, key, is_startup);
}

static void joy_hat_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int joy_num;
  char key_up[16];
  char key_down[16];
  char key_left[16];
  char key_right[16];
  int read = 0;

  if(sscanf(name, "joy%uhat", &joy_num) != 1)
    return;

  if(sscanf(value, JOY_ENUM ", " JOY_ENUM ", " JOY_ENUM ", " JOY_ENUM "%n",
   key_up, key_down, key_left, key_right, &read) != 4 || (value[read] != 0))
    return;

  // Right now do a global binding at startup and a game binding otherwise.
  joystick_map_hat(joy_num - 1, key_up, key_down, key_left, key_right,
   is_startup);
}

static void joy_action_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int joy_num;
  unsigned int key;
  char joy_action[16];
  int read = 0;

  if(sscanf(name, "joy%u." JOY_ENUM "%n", &joy_num, joy_action, &read) != 2
   || (name[read] != 0))
    return;

  read = 0;
  if(sscanf(value, "%u%n", &key, &read) != 1 || (value[read] != 0))
    return;

  // Right now do a global binding at startup and a game binding otherwise.
  joystick_map_action(joy_num - 1, joy_action, key, is_startup);
}

static void config_set_joy_axis_threshold(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int threshold;
  int read = 0;

  if(sscanf(value, "%u%n", &threshold, &read) != 1 || (value[read] != 0) ||
   threshold > 32767)
    return;

  joystick_set_axis_threshold(threshold);
}

#ifdef CONFIG_SDL
#if SDL_VERSION_ATLEAST(2,0,0)
#define GC_ENUM "%15[-+0-9A-Za-z_]"

static void config_sdl_gc_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  char gc_sym[16];
  char key[16];
  int read = 0;

  if(sscanf(name, "gamecontroller." GC_ENUM "%n", gc_sym, &read) != 1
   || (name[read] != 0))
    return;

  read = 0;
  if(sscanf(value, JOY_ENUM "%n", key, &read) != 1 || (value[read] != 0))
    return;

  gamecontroller_map_sym(gc_sym, key);
}

static void config_sdl_gc_add(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  gamecontroller_add_mapping(value);
}

#endif // SDL_VERSION_ATLEAST(2,0,0)
#endif // CONFIG_SDL

static void include_config(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // This one's for the original include N form
  set_config_from_file(name + 7);
}

static void include2_config(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // This one's for the include = N form
  set_config_from_file(value);
}

static void config_window_resolution(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  char *next;
  conf->window_width = strtoul(value, &next, 10);
  conf->window_height = strtoul(next + 1, NULL, 10);
}

#define Int(field, min, max) MAP_INT(struct config_info, field, min, max)
#define Enum(field) MAP_ENUM(struct config_info, field, field ## _values)
#define Bool(field) MAP_ENUM(struct config_info, field, boolean_values)
#define Str(field) MAP_STR(struct config_info, field)
#define Fn(field) MAP_FN(struct config_info, field)

/* NOTE: This is searched as a binary tree, the nodes must be
 *       sorted alphabetically, or they risk being ignored.
 *
 * For values that can easily be written directly to the struct,
 * use the Int/Enum/Bool/Str macro functions. These generate offset
 * and bounding information that allows config_option_set to reuse
 * the same code for similar options. For more complex options, the
 * Fn macro can be used to provide a config_function instead.
 */
static const struct config_entry config_options[] =
{
  { "allow_cheats",             Enum(allow_cheats), false },
  { "allow_screenshots",        Bool(allow_screenshots), false },
  { "audio_buffer",             Int(audio_buffer_samples, 16, INT_MAX), false },
  { "audio_buffer_samples",     Int(audio_buffer_samples, 16, INT_MAX), false },
  { "audio_sample_rate",        Int(output_frequency, 1, INT_MAX), false },
  { "enable_oversampling",      Bool(oversampling_on), false },
  { "enable_resizing",          Bool(allow_resize), false },
  { "force_bpp",                Enum(force_bpp), false },
  { "fullscreen",               Bool(fullscreen), false },
  { "fullscreen_resolution",    Fn(config_set_resolution), false },
#ifdef CONFIG_SDL
#if SDL_VERSION_ATLEAST(2,0,0)
  { "gamecontroller.*",         Fn(config_sdl_gc_set), false },
  { "gamecontroller_add",       Fn(config_sdl_gc_add), false },
  { "gamecontroller_enable",    Bool(allow_gamecontroller), false },
#endif
#endif
  { "gl_filter_method",         Enum(gl_filter_method), false },
  { "gl_scaling_shader",        Str(gl_scaling_shader), true },
  { "gl_vsync",                 Enum(gl_vsync), false },
  { "include",                  Fn(include2_config), true },
  { "include*",                 Fn(include_config), true },
  { "joy!.*",                   Fn(joy_action_set), true },
  { "joy!axis!",                Fn(joy_axis_set), true },
  { "joy!button!",              Fn(joy_button_set), true },
  { "joy!hat",                  Fn(joy_hat_set), true },
  { "joy_axis_threshold",       Fn(config_set_joy_axis_threshold), false },
  { "mask_midchars",            Bool(mask_midchars), false },
  { "max_simultaneous_samples", Int(max_simultaneous_samples, -1, INT_MAX), false },
  { "modplug_resample_mode",    Enum(modplug_resample_mode), false },
  { "music_on",                 Bool(music_on), false },
  { "music_volume",             Int(music_volume, 0, 10), false },
  { "mzx_speed",                Int(mzx_speed, 1, 16), true },
#ifdef CONFIG_NETWORK
  { "network_enabled",          Bool(network_enabled), false },
#endif
  { "no_titlescreen",           Bool(no_titlescreen), false },
  { "num_buffered_events",      Int(num_buffered_events, 1, 256), false },
  { "pause_on_unfocus",         Bool(pause_on_unfocus), false },
  { "pc_speaker_on",            Bool(pc_speaker_on), false },
  { "pc_speaker_volume",        Int(pc_speaker_volume, 0, 10), false },
  { "resample_mode",            Enum(resample_mode), false },
  { "sample_volume",            Int(sam_volume, 0, 10), false },
  { "save_file",                Str(default_save_name), false },
#ifdef CONFIG_NETWORK
  { "socks_host",               Str(socks_host), false },
  { "socks_port",               Int(socks_port, 0, 65535), false },
#endif
  { "standalone_mode",          Bool(standalone_mode), false },
  { "startup_editor",           Bool(startup_editor), false },
  { "startup_file",             Fn(config_startup_file), false },
  { "startup_path",             Fn(config_startup_path), false },
  { "system_mouse",             Enum(system_mouse), false },
#ifdef CONFIG_UPDATER
  { "update_auto_check",        Enum(update_auto_check), false },
  { "update_branch_pin",        Str(update_branch_pin), false },
  { "update_host",              Fn(config_update_host), false },
#endif
  { "video_output",             Str(video_output), false },
  { "video_ratio",              Enum(video_ratio), false },
  { "window_resolution",        Fn(config_window_resolution), false },
};

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

static int config_change_option(void *conf, char *name,
 char *value, char *extended_data)
{
  const struct config_entry *current_option = find_option(name,
   config_options, ARRAY_SIZE(config_options));

  if(current_option)
  {
    if(current_option->allow_in_game_config || is_startup)
    {
      fn_ptr fn = mapped_field_set(&(current_option->dest), conf, name, value);

      if(fn)
        ((config_function)fn)(conf, name, value, extended_data);
      return 1;
    }
  }
  return 0;
}

#define LINE_BUFFER_SIZE 512

__editor_maybe_static void __set_config_from_file(
 find_change_option find_change_handler, void *conf, const char *conf_file_name)
{
  char current_char, *input_position, *output_position, *use_extended_buffer;
  int line_size, extended_size, extended_allocate_size = 512;
  char line_buffer_alternate[LINE_BUFFER_SIZE], line_buffer[LINE_BUFFER_SIZE];
  int extended_buffer_offset, peek_char;
  char *extended_buffer;
  char *equals_position, *value;
  char *output_end_position = line_buffer + LINE_BUFFER_SIZE;
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

          if(output_position < output_end_position)
          {
            if((current_char == '=') && (equals_position == NULL))
              equals_position = output_position;

            *output_position = current_char;
            output_position++;
          }
        }
        input_position++;
      } while(current_char);

      line_buffer[LINE_BUFFER_SIZE - 1] = 0;

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
  char *equals_position, line_buffer[LINE_BUFFER_SIZE], *value;
  char *output_end_position = line_buffer + LINE_BUFFER_SIZE;
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

      if(output_position < output_end_position)
      {
        if((current_char == '=') && (equals_position == NULL))
          equals_position = output_position;

        *output_position = current_char;
        output_position++;
      }
      input_position++;
    } while(current_char);

    line_buffer[LINE_BUFFER_SIZE - 1] = 0;

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

struct config_info *get_config(void)
{
  return &user_conf;
}

void default_config(void)
{
  memcpy(&user_conf, &user_conf_default, sizeof(struct config_info));
}

void set_config_from_file(const char *conf_file_name)
{
  __set_config_from_file(config_change_option, &user_conf, conf_file_name);
}

void set_config_from_file_startup(const char *conf_file_name)
{
  is_startup = true;
  set_config_from_file(conf_file_name);
  is_startup = false;
}

void set_config_from_command_line(int *argc, char *argv[])
{
  is_startup = true;
  __set_config_from_command_line(config_change_option, &user_conf, argc, argv);
  is_startup = false;
}

void free_config(void)
{
#ifdef CONFIG_UPDATER
  // Custom updater hosts might have been allocated
  if(user_conf.update_hosts != default_update_hosts)
  {
    int i;

    for(i = 0; i < user_conf.update_host_count; i++)
      free(user_conf.update_hosts[i]);

    free(user_conf.update_hosts);
    user_conf.update_hosts = default_update_hosts;
  }
#endif
}
