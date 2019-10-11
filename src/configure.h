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

#ifndef __CONFIGURE_H
#define __CONFIGURE_H

#include "compat.h"

__M_BEGIN_DECLS

#define OPTION_NAME_LEN 33

enum ratio_type
{
  RATIO_CLASSIC_4_3,
  RATIO_MODERN_64_35,
  RATIO_STRETCH
};

enum allow_cheats_type
{
  ALLOW_CHEATS_NEVER,
  ALLOW_CHEATS_MZXRUN,
  ALLOW_CHEATS_ALWAYS
};

enum update_auto_check_mode
{
  UPDATE_AUTO_CHECK_OFF = 0,
  UPDATE_AUTO_CHECK_ON,
  UPDATE_AUTO_CHECK_SILENT
};

struct config_info
{
  // Video options
  boolean fullscreen;
  boolean fullscreen_windowed;
  int resolution_width;
  int resolution_height;
  int window_width;
  int window_height;
  boolean allow_resize;
  char video_output[16];
  int force_bpp;
  enum ratio_type video_ratio;
  char gl_filter_method[16];
  char gl_scaling_shader[32];
  int gl_vsync;
  boolean allow_screenshots;

  // Audio options
  int output_frequency;
  int audio_buffer_samples;
  int oversampling_on;
  int resample_mode;
  int modplug_resample_mode;
  int max_simultaneous_samples;
  int music_volume;
  int sam_volume;
  int pc_speaker_volume;
  boolean music_on;
  boolean pc_speaker_on;

  // Game options
  char startup_path[256];
  char startup_file[256];
  char default_save_name[256];
  int mzx_speed;
  enum allow_cheats_type allow_cheats;
  boolean auto_decrypt_worlds;
  boolean startup_editor;
  boolean standalone_mode;
  boolean no_titlescreen;
  boolean system_mouse;
  boolean grab_mouse;

  // Editor options
  boolean test_mode;
  unsigned char test_mode_start_board;
  // TODO: two places outside of the editor currently require access to this.
  boolean mask_midchars;

  // Network layer options
#ifdef CONFIG_NETWORK
  boolean network_enabled;
  char socks_host[256];
  int socks_port;
#endif

#ifdef CONFIG_UPDATER
  int update_host_count;
  char **update_hosts;
  char update_branch_pin[256];
  int update_auto_check;
#endif
};

CORE_LIBSPEC struct config_info *get_config(void);
CORE_LIBSPEC void default_config(void);
CORE_LIBSPEC void set_config_from_file(const char *conf_file_name);
CORE_LIBSPEC void set_config_from_file_startup(const char *conf_file_name);
CORE_LIBSPEC void set_config_from_command_line(int *argc, char *argv[]);
CORE_LIBSPEC void free_config(void);

typedef int (*find_change_option)(void *conf, char *name, char *value,
 char *extended_data);

#ifdef CONFIG_EDITOR

CORE_LIBSPEC void __set_config_from_file(find_change_option find_change_handler,
 void *conf, const char *conf_file_name);
CORE_LIBSPEC void __set_config_from_command_line(
 find_change_option find_change_handler, void *conf, int *argc, char *argv[]);

#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __CONFIGURE_H
