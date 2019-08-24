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
#include "counter.h"
#include "event.h"
#include "rasm.h"
#include "fsafeopen.h"
#include "util.h"
#include "sys/stat.h"

// Arch-specific config.
#ifdef CONFIG_NDS
#define VIDEO_OUTPUT_DEFAULT "nds"
#define VIDEO_RATIO_DEFAULT RATIO_CLASSIC_4_3
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
#define VIDEO_RATIO_DEFAULT RATIO_CLASSIC_4_3
#ifdef CONFIG_SDL
#define VIDEO_OUTPUT_DEFAULT "software"
#define FULLSCREEN_WIDTH_DEFAULT 640
#define FULLSCREEN_HEIGHT_DEFAULT 480
#define FORCE_BPP_DEFAULT 16
#endif
#endif

#ifdef CONFIG_3DS
#define FORCE_BPP_DEFAULT 16
#define VIDEO_RATIO_DEFAULT RATIO_CLASSIC_4_3
#endif

#ifdef CONFIG_SWITCH
// Switch SDL needs this resolution or else weird things start happening...
#define FULLSCREEN_WIDTH_DEFAULT 1920
#define FULLSCREEN_HEIGHT_DEFAULT 1080
#define FULLSCREEN_DEFAULT 1
#endif

#ifdef ANDROID
#define FULLSCREEN_DEFAULT 1
#endif

#ifdef __EMSCRIPTEN__
#define AUDIO_SAMPLE_RATE 48000
#endif

// End arch-specific config.

#ifndef FORCE_BPP_DEFAULT
#define FORCE_BPP_DEFAULT 32
#endif

#ifndef GL_VSYNC_DEFAULT
#define GL_VSYNC_DEFAULT 0
#endif

#ifndef VIDEO_OUTPUT_DEFAULT
#if defined(CONFIG_RENDER_GL_PROGRAM)
#define VIDEO_OUTPUT_DEFAULT "auto_glsl"
#elif defined(CONFIG_RENDER_SOFTSCALE)
#define VIDEO_OUTPUT_DEFAULT "softscale"
#else
#define VIDEO_OUTPUT_DEFAULT "software"
#endif
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

#ifndef VIDEO_RATIO_DEFAULT
#define VIDEO_RATIO_DEFAULT RATIO_MODERN_64_35
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
  false,                        // fullscreen_windowed
  FULLSCREEN_WIDTH_DEFAULT,     // resolution_width
  FULLSCREEN_HEIGHT_DEFAULT,    // resolution_height
  640,                          // window_width
  350,                          // window_height
  1,                            // allow_resize
  VIDEO_OUTPUT_DEFAULT,         // video_output
  FORCE_BPP_DEFAULT,            // force_bpp
  VIDEO_RATIO_DEFAULT,          // video_ratio
  "linear",                     // opengl filter method
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
  false,                        // grab_mouse

  // Editor options
  false,                        // test_mode
  NO_BOARD,                     // test_mode_start_board
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

typedef void (*config_function)(struct config_info *conf, char *name,
 char *value, char *extended_data);

struct config_entry
{
  char option_name[OPTION_NAME_LEN];
  config_function change_option;
  boolean allow_in_game_config;
};

#ifdef CONFIG_NETWORK

static void config_set_network_enabled(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
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
  // FIXME sloppy validation
  conf->socks_port = strtoul(value, NULL, 10);
}

#endif // CONFIG_NETWORK

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

static void config_update_branch_pin(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  strncpy(conf->update_branch_pin, value, 256);
  conf->update_branch_pin[256 - 1] = 0;
}

static void config_update_auto_check(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  if(!strcasecmp(value, "off"))
  {
    conf->update_auto_check = UPDATE_AUTO_CHECK_OFF;
  }
  else

  if(!strcasecmp(value, "on"))
  {
    conf->update_auto_check = UPDATE_AUTO_CHECK_ON;
  }
  else

  if(!strcasecmp(value, "silent"))
  {
    conf->update_auto_check = UPDATE_AUTO_CHECK_SILENT;
  }
}

#endif // CONFIG_UPDATER

static void config_set_audio_buffer(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->audio_buffer_samples = strtoul(value, NULL, 10);
}

static void config_set_resolution(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  char *next;
  conf->resolution_width = strtoul(value, &next, 10);
  conf->resolution_height = strtoul(next + 1, NULL, 10);
}

static void config_set_fullscreen(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->fullscreen = strtoul(value, NULL, 10);
}

static void config_set_fullscreen_windowed(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->fullscreen_windowed = strtoul(value, NULL, 10);
}

static void config_set_music(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->music_on = strtoul(value, NULL, 10);
}

static void config_set_mod_volume(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  unsigned long new_volume = strtoul(value, NULL, 10);
  conf->music_volume = MIN(new_volume, 10);
}

static void config_set_mzx_speed(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  unsigned long new_speed = strtoul(value, NULL, 10);
  conf->mzx_speed = CLAMP(new_speed, 1, 16);
}

static void config_set_pc_speaker(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->pc_speaker_on = strtol(value, NULL, 10);
}

static void config_set_sam_volume(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  unsigned long new_volume = strtoul(value, NULL, 10);
  conf->sam_volume = MIN(new_volume, 10);
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
  if(stat(value, &stat_info) || !S_ISDIR(stat_info.st_mode))
    return;

  snprintf(conf->startup_path, 256, "%s", value);
}

static void config_system_mouse(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->system_mouse = strtol(value, NULL, 10);
}

static void config_grab_mouse(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->grab_mouse = strtol(value, NULL, 10);
}

static void config_enable_oversampling(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
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

#define JOY_ENUM "%15[0-9A-Za-z_]"

static boolean joy_num(char **name, unsigned int *first, unsigned int *last)
{
  int next = 0;
  if(!strncasecmp(*name, "joy", 3))
  {
    *name += 3;

    if(sscanf(*name, "[%u,%u]%n", first, last, &next) == 2)
    {
      *name += next;
      return true;
    }
    else

    if(sscanf(*name, "%u%n", first, &next) == 1)
    {
      *last = *first;
      *name += next;
      return true;
    }
  }
  return false;
}

static boolean joy_button_name(const char *rest, unsigned int *button_num)
{
  int next = 0;
  if(sscanf(rest, "button%u%n", button_num, &next) != 1 || (rest[next] != 0))
    return false;
  return true;
}

static boolean joy_axis_name(const char *rest, unsigned int *axis_num)
{
  int next = 0;
  if(sscanf(rest, "axis%u%n", axis_num, &next) != 1 || (rest[next] != 0))
    return false;
  return true;
}

static boolean joy_axis_value(const char *value, char min[16], char max[16])
{
  int next = 0;
  if(sscanf(value, JOY_ENUM ", " JOY_ENUM "%n", min, max, &next) != 2
   || (value[next] != 0))
    return false;
  return true;
}

static boolean joy_hat_value(const char *value, char up[16], char down[16],
 char left[16], char right[16])
{
  int next = 0;
  if(sscanf(value, JOY_ENUM ", " JOY_ENUM ", " JOY_ENUM ", " JOY_ENUM "%n",
   up, down, left, right, &next) != 4 || (value[next] != 0))
    return false;
  return true;
}

static void joy_axis_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int first, last, axis;
  char min[16], max[16];

  if(joy_num(&name, &first, &last) && joy_axis_name(name, &axis) &&
   joy_axis_value(value, min, max))
  {
    // Right now do a global binding at startup and a game binding otherwise.
    joystick_map_axis(first - 1, last - 1, axis - 1, min, max, is_startup);
  }
}

static void joy_button_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int first, last, button;

  if(joy_num(&name, &first, &last) && joy_button_name(name, &button))
  {
    // Right now do a global binding at startup and a game binding otherwise.
    joystick_map_button(first - 1, last - 1, button - 1, value, is_startup);
  }
}

static void joy_hat_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int first, last;
  char up[16], down[16], left[16], right[16];

  if(joy_num(&name, &first, &last) && !strcasecmp(name, "hat") &&
   joy_hat_value(value, up, down, left, right))
  {
    // Right now do a global binding at startup and a game binding otherwise.
    joystick_map_hat(first - 1, last - 1, up, down, left, right, is_startup);
  }
}

static void joy_action_set(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  unsigned int first, last;

  if(joy_num(&name, &first, &last) && (*name == '.'))
  {
    // Right now do a global binding at startup and a game binding otherwise.
    joystick_map_action(first - 1, last - 1, name + 1, value, is_startup);
  }
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

static void config_sdl_gc_enable(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  if(!strcmp(value, "0"))
    gamecontroller_set_enabled(false);

  else if(!strcmp(value, "1"))
    gamecontroller_set_enabled(true);
}

#endif // SDL_VERSION_ATLEAST(2,0,0)
#endif // CONFIG_SDL

static void pause_on_unfocus(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  set_unfocus_pause(strtoul(value, NULL, 10) > 0);
}

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

static void config_set_pcs_volume(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  unsigned long new_volume = strtoul(value, NULL, 10);
  conf->pc_speaker_volume = MIN(new_volume, 10);
}

static void config_mask_midchars(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // TODO move to editor config when non-editor code stops relying on it
  // FIXME sloppy validation
  conf->mask_midchars = strtoul(value, NULL, 10);
}

static void config_set_audio_freq(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->output_frequency = strtoul(value, NULL, 10);
}

static void config_force_bpp(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->force_bpp = strtoul(value, NULL, 10);
}

static void config_window_resolution(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  char *next;
  conf->window_width = strtoul(value, &next, 10);
  conf->window_height = strtoul(next + 1, NULL, 10);
}

static void config_set_video_output(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  strncpy(conf->video_output, value, 16);
  conf->video_output[15] = 0;
}

static void config_enable_resizing(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->allow_resize = strtoul(value, NULL, 10);
}

static void config_set_gl_filter_method(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  snprintf(conf->gl_filter_method, 16, "%s", value);
}

static void config_set_gl_scaling_shader(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  snprintf(conf->gl_scaling_shader, 32, "%s", value);
}

static void config_gl_vsync(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation (note: negative value has special meaning...)
  conf->gl_vsync = strtol(value, NULL, 10);
}

static void config_set_allow_screenshots(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->allow_screenshots = strtol(value, NULL, 10);
}

static void config_startup_editor(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->startup_editor = strtoul(value, NULL, 10);
}

static void config_standalone_mode(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->standalone_mode = strtoul(value, NULL, 10);
}

static void config_no_titlescreen(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  // FIXME sloppy validation
  conf->no_titlescreen = strtoul(value, NULL, 10);
}

static void config_set_allow_cheats(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  if(!strcmp(value, "0"))
  {
    conf->allow_cheats = ALLOW_CHEATS_NEVER;
  }
  else

  if(!strcasecmp(value, "mzxrun"))
  {
    conf->allow_cheats = ALLOW_CHEATS_MZXRUN;
  }
  else

  if(!strcmp(value, "1"))
  {
    conf->allow_cheats = ALLOW_CHEATS_ALWAYS;
  }
}

static void config_set_video_ratio(struct config_info *conf, char *name,
 char *value, char *extended_data)
{
  if(!strcasecmp(value, "classic"))
  {
    conf->video_ratio = RATIO_CLASSIC_4_3;
  }
  else

  if(!strcasecmp(value, "modern"))
  {
    conf->video_ratio = RATIO_MODERN_64_35;
  }

  else
  {
    conf->video_ratio = RATIO_STRETCH;
  }
}

static void config_set_num_buffered_events(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  // FIXME sloppy validation also wtf?
  Uint8 v = (Uint8)strtoul(value, NULL, 10);
  set_num_buffered_events(v);
}

static void config_max_simultaneous_samples(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  // FIXME less sloppy validation but still needs more
  int v = MAX( strtol(value, NULL, 10), -1 );
  conf->max_simultaneous_samples = v;
}

static void config_test_mode(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  // FIXME sloppy validation
  unsigned long test_mode = strtoul(value, NULL, 10);
  conf->test_mode = !!test_mode;
}

static void config_test_mode_start_board(struct config_info *conf,
 char *name, char *value, char *extended_data)
{
  // FIXME sloppy validation
  unsigned long start_board = MIN(strtoul(value, NULL, 10), MAX_BOARDS-1);
  conf->test_mode_start_board = start_board;
}

/* NOTE: This is searched as a binary tree, the nodes must be
 *       sorted alphabetically, or they risk being ignored.
 */
static const struct config_entry config_options[] =
{
  { "allow_cheats", config_set_allow_cheats, false },
  { "allow_screenshots", config_set_allow_screenshots, false },
  { "audio_buffer", config_set_audio_buffer, false },
  { "audio_buffer_samples", config_set_audio_buffer, false },
  { "audio_sample_rate", config_set_audio_freq, false },
  { "enable_oversampling", config_enable_oversampling, false },
  { "enable_resizing", config_enable_resizing, false },
  { "force_bpp", config_force_bpp, false },
  { "fullscreen", config_set_fullscreen, false },
  { "fullscreen_resolution", config_set_resolution, false },
  { "fullscreen_windowed", config_set_fullscreen_windowed, false },
#ifdef CONFIG_SDL
#if SDL_VERSION_ATLEAST(2,0,0)
  { "gamecontroller.*", config_sdl_gc_set, false },
  { "gamecontroller_add", config_sdl_gc_add, false },
  { "gamecontroller_enable", config_sdl_gc_enable, false },
#endif
#endif
  { "gl_filter_method", config_set_gl_filter_method, false },
  { "gl_scaling_shader", config_set_gl_scaling_shader, true },
  { "gl_vsync", config_gl_vsync, false },
  { "grab_mouse", config_grab_mouse, false },
  { "include", include2_config, true },
  { "include*", include_config, true },
  { "joy!.*", joy_action_set, true },
  { "joy!axis!", joy_axis_set, true },
  { "joy!button!", joy_button_set, true },
  { "joy!hat", joy_hat_set, true },
  { "joy[!,!].*", joy_action_set, true },
  { "joy[!,!]axis!", joy_axis_set, true },
  { "joy[!,!]button!", joy_button_set, true },
  { "joy[!,!]hat", joy_hat_set, true },
  { "joy_axis_threshold", config_set_joy_axis_threshold, false },
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
  { "pc_speaker_volume", config_set_pcs_volume, false },
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
  { "test_mode", config_test_mode, false },
  { "test_mode_start_board", config_test_mode_start_board, false },
#ifdef CONFIG_UPDATER
  { "update_auto_check", config_update_auto_check, false },
  { "update_branch_pin", config_update_branch_pin, false },
  { "update_host", config_update_host, false },
#endif
  { "video_output", config_set_video_output, false },
  { "video_ratio", config_set_video_ratio, false },
  { "window_resolution", config_window_resolution, false }
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
      current_option->change_option(conf, name, value, extended_data);
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
