/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <algorithm>
#include <climits>
#include <cstdio>
#include <limits>

#include "Unit.hpp"

#include "../src/configure.h"
#include "../src/const.h"
#include "../src/util.h"

#ifdef CONFIG_SDL
#include <SDL_version.h>
#endif

#ifdef CONFIG_EDITOR
#include "../src/editor/configure.h"
#include "../src/editor/robo_ed.h"
#endif

static const int DEFAULT = 255;
static const int IGNORE = INT_MAX;
static boolean game_allowed;

struct config_test_single
{
  const char * const value;
  int expected;
};

struct config_test_pair
{
  const char * const value;
  int expected_a;
  int expected_b;
};

struct config_test_string
{
  const char * const value;
  const char * const expected;
};

#ifdef CONFIG_EDITOR
struct config_test_board_size_viewport
{
  const char * const setting;
  const char * const value;
  int width;
  int height;
  int viewport_x;
  int viewport_y;
  int viewport_w;
  int viewport_h;
};

struct config_test_saved_position
{
  int which;
  const char * const value;
  struct saved_position expected;
};
#endif /* CONFIG_EDITOR */

static void load_arg(char *arg)
{
  char *tmp_args[2];
  int argc = 0;

  tmp_args[argc++] = nullptr;
  tmp_args[argc++] = arg;

  set_config_from_command_line(&argc, tmp_args);
}

static boolean write_config(const char *path, const char *config)
{
  FILE *fp = fopen_unsafe(path, "wb");
  assert(fp);
  if(fp)
  {
    int ret = fwrite(config, strlen(config), 1, fp);
    assert(ret == 1);
    ret = fclose(fp);
    assert(!ret);
    return true;
  }
  return false;
}

static void load_arg_file(const char *arg, boolean is_game_config)
{
  static const char CONFIG_FILENAME[] = "_config_tmp";

  if(write_config(CONFIG_FILENAME, arg))
  {
    config_type type = is_game_config ? GAME_EDITOR_CNF : SYSTEM_CNF;
    set_config_from_file(type, CONFIG_FILENAME);
  }
}

template<int SIZE>
static void load_args(const char * const (&args)[SIZE])
{
  char *tmp_args[SIZE + 1];
  int argc = 0;
  int i;

  tmp_args[argc++] = nullptr;

  for(i = 0; i < SIZE; i++)
  {
    if(!args[i])
      break;

    tmp_args[argc++] = (char *)args[i];
  }

  set_config_from_command_line(&argc, tmp_args);
}

template<class T>
void TEST_INT(const char *setting_name, T &setting, ssize_t min, ssize_t max)
{
  constexpr T default_value = static_cast<T>(DEFAULT);
  char arg[512];

  for(int i = -16; i < 32; i++)
  {
    ssize_t tmp = (max - min) * i / 16 + min;
    ssize_t expected = (tmp >= min && tmp <= max) ? tmp : default_value;

    if(tmp < (ssize_t)std::numeric_limits<T>::min() ||
     tmp > (ssize_t)std::numeric_limits<T>::max())
      continue;

    snprintf(arg, arraysize(arg), "%s=%zd", setting_name, tmp);

    setting = default_value;
    load_arg(arg);
    ASSERTEQX((ssize_t)setting, expected, arg);

    setting = default_value;
    load_arg_file(arg, game_allowed);
    ASSERTEQX((ssize_t)setting, expected, arg);

    if(!game_allowed)
    {
      setting = default_value;
      load_arg_file(arg, true);
      ASSERTEQX(setting, default_value, arg);
    }
  }
}

template<class T, int NUM_TESTS>
void TEST_ENUM(const char *setting_name, T &setting,
 const config_test_single (&data)[NUM_TESTS])
{
  constexpr T default_value = static_cast<T>(DEFAULT);
  char arg[512];

  for(int i = 0; i < NUM_TESTS; i++)
  {
    snprintf(arg, arraysize(arg), "%s=%s", setting_name, data[i].value);

    setting = default_value;
    load_arg(arg);
    ASSERTEQX((int)setting, data[i].expected, arg);

    setting = default_value;
    load_arg_file(arg, game_allowed);
    ASSERTEQX((int)setting, data[i].expected, arg);

    if(!game_allowed)
    {
      setting = default_value;
      load_arg_file(arg, true);
      ASSERTEQX(setting, default_value, arg);
    }
  }
}

template<class T, int NUM_TESTS>
void TEST_PAIR(const char *setting_name, T &setting_a, T &setting_b,
 const config_test_pair (&data)[NUM_TESTS])
{
  constexpr T default_value = static_cast<T>(DEFAULT);
  char arg[512];

  for(int i = 0; i < NUM_TESTS; i++)
  {
    snprintf(arg, 512, "%s=%s", setting_name, data[i].value);

    setting_a = default_value;
    setting_b = default_value;
    load_arg(arg);
    ASSERTEQX((int)setting_a, data[i].expected_a, arg);
    ASSERTEQX((int)setting_b, data[i].expected_b, arg);

    setting_a = default_value;
    setting_b = default_value;
    load_arg_file(arg, game_allowed);
    ASSERTEQX((int)setting_a, data[i].expected_a, arg);
    ASSERTEQX((int)setting_b, data[i].expected_b, arg);

    if(!game_allowed)
    {
      setting_a = default_value;
      setting_b = default_value;
      load_arg_file(arg, true);
      ASSERTEQX(setting_a, default_value, arg);
      ASSERTEQX(setting_b, default_value, arg);
    }
  }
}

template<size_t S, int NUM_TESTS>
void TEST_STRING(const char *setting_name, char (&setting)[S],
 const config_test_string (&data)[NUM_TESTS])
{
  char arg[512];

  for(int i = 0; i < NUM_TESTS; i++)
  {
    size_t len = std::min(strlen(data[i].expected), S - 1);
    snprintf(arg, arraysize(arg), "%s=%s", setting_name, data[i].value);

    setting[0] = '\0';
    load_arg(arg);
    ASSERTXNCMP(setting, data[i].expected, len, arg);
    ASSERTEQX(setting[len], '\0', arg);

    setting[0] = '\0';
    load_arg_file(arg, game_allowed);
    ASSERTXNCMP(setting, data[i].expected, len, arg);
    ASSERTEQX(setting[len], '\0', arg);

    if(!game_allowed)
    {
      setting[0] = '\0';
      load_arg_file(arg, true);
      ASSERTEQX(setting[0], '\0', arg);
    }
  }
}

UNITTEST(Settings)
{
  struct config_info *conf = get_config();

  static const config_test_single boolean_data[] =
  {
    { "0", false },
    { "1", true },
    { "2", DEFAULT },
    { "23", DEFAULT },
    { "-12", DEFAULT },
    { "-1", DEFAULT },
    { "yes", DEFAULT },
    { "ABCD", DEFAULT },
    { "0\\s", DEFAULT },
  };

  static const config_test_pair pair_data[] =
  {
    { "-1,-1", -1, -1 },
    { "-1,0", -1, 0 },
    { "0,-1", 0, -1 },
    { "640,350", 640, 350 },
    { "640,480", 640, 480 },
    { "1280,700", 1280, 700 },
    { "1920,1080", 1920, 1080 },
    { "2560,1440", 2560, 1440 },
    { "3840,2160", 3840, 2160 },
    { "640, 350", 640, 350 },
    { "1600,  1200", 1600, 1200 },
    { "640,a", DEFAULT, DEFAULT },
    { "a,480", DEFAULT, DEFAULT },
    { "a,b", DEFAULT, DEFAULT },
    { "640px,350px", DEFAULT, DEFAULT },
    { "-2,4154", DEFAULT, DEFAULT },
    { "1233,-13513", DEFAULT, DEFAULT },
  };

  static const char LONGSTRING[] =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  static const config_test_string string_data[] =
  {
    { "", "" },
    { "software", "software" },
    { "glsl", "glsl" },
    { "crt", "crt" },
    { "greyscale", "greyscale" },
    { "really_long_setting_string_wtf_don't_do_this",
      "really_long_setting_string_wtf_don't_do_this" },
    { LONGSTRING, LONGSTRING },
    // NOTE: Both the command line and the config file translate \s to a single
    // space. Whitespace can't really be tested the same between the two as it
    // isn't consumed from the command line but is unconditionally consumed
    // from the config file. This leads to situations where "test thing = a"
    // tries to set the option "testthing" from the file but "test thing " from
    // the command line. Both of these are wrong but not really worth fixing.
    { "space\\sspace", "space space" },
    { "copy\\soverlay\\sblock\\sat\\s0\\s0\\sfor\\s1\\sby\\s1\\sto\\s10\\s10",
      "copy overlay block at 0 0 for 1 by 1 to 10 10" },
    { "triple\\s\\s\\sspace", "triple   space" },
  };

  game_allowed = false;
  default_config();

  // Video options.

  SECTION(fullscreen)
  {
    TEST_ENUM("fullscreen", conf->fullscreen, boolean_data);
  }

  SECTION(fullscreen_windowed)
  {
    TEST_ENUM("fullscreen_windowed", conf->fullscreen_windowed, boolean_data);
  }

  SECTION(fullscreen_resolution)
  {
    TEST_PAIR("fullscreen_resolution", conf->resolution_width,
     conf->resolution_height, pair_data);
  }

  SECTION(window_resolution)
  {
    TEST_PAIR("window_resolution", conf->window_width,
     conf->window_height, pair_data);
  }

  SECTION(allow_resize)
  {
    TEST_ENUM("enable_resizing", conf->allow_resize, boolean_data);
  }

  SECTION(video_output)
  {
    TEST_STRING("video_output", conf->video_output, string_data);
  }

  SECTION(force_bpp)
  {
    static const config_test_single data[] =
    {
      { "8", 8 },
      { "16", 16 },
      { "32", 32 },
      { "-1", DEFAULT },
      { "231", DEFAULT },
      { "0", DEFAULT },
      { "asdfsdf", DEFAULT },
    };
    TEST_ENUM("force_bpp", conf->force_bpp, data);
  }

  SECTION(video_ratio)
  {
    static const config_test_single data[] =
    {
      { "classic", RATIO_CLASSIC_4_3 },
      { "modern", RATIO_MODERN_64_35 },
      { "stretch", RATIO_STRETCH },
      { "-1", DEFAULT },
      { "12312342", DEFAULT },
      { "classiccc", DEFAULT },
    };
    TEST_ENUM("video_ratio", conf->video_ratio, data);
  }

  SECTION(gl_filter_method)
  {
    static const config_test_single data[] =
    {
      { "nearest", CONFIG_GL_FILTER_NEAREST },
      { "linear", CONFIG_GL_FILTER_LINEAR },
      { "213123", DEFAULT },
      { "nearets", DEFAULT },
    };
    TEST_ENUM("gl_filter_method", conf->gl_filter_method, data);
  }

  SECTION(gl_scaling_shader)
  {
    game_allowed = true;
    TEST_STRING("gl_scaling_shader", conf->gl_scaling_shader, string_data);
  }

  SECTION(gl_vsync)
  {
    static const config_test_single data[] =
    {
      { "-1", -1 },
      { "0", 0 },
      { "1", 1 },
      { "default", -1 },
      { "-2", DEFAULT },
      { "-12312", DEFAULT },
      { "2", DEFAULT },
      { "6456", DEFAULT },
      { "1a", DEFAULT },
    };
    TEST_ENUM("gl_vsync", conf->gl_vsync, data);
  }

  SECTION(cursor_hint_mode)
  {
    static const config_test_single data[] =
    {
      { "0", CURSOR_MODE_INVISIBLE },
      { "1", CURSOR_MODE_HINT },
      { "off", CURSOR_MODE_INVISIBLE },
      { "hidden", CURSOR_MODE_HINT },
      { "underline", CURSOR_MODE_UNDERLINE },
      { "solid",  CURSOR_MODE_SOLID },
      { "-1", DEFAULT },
      { "-24124124", DEFAULT },
      { "2", DEFAULT },
      { "938019831", DEFAULT },
      { "1a", DEFAULT },
      { "umderl1ne", DEFAULT },
    };
    TEST_ENUM("dialog_cursor_hints", conf->cursor_hint_mode, data);
  }

  SECTION(allow_screenshots)
  {
    TEST_ENUM("allow_screenshots", conf->allow_screenshots, boolean_data);
  }

  // Audio options.

  SECTION(audio_sample_rate)
  {
    TEST_INT("audio_sample_rate", conf->output_frequency, 1, INT_MAX);
  }

  SECTION(audio_buffer_samples)
  {
    TEST_INT("audio_buffer_samples", conf->audio_buffer_samples, 1, INT_MAX);
  }

  SECTION(enable_oversampling)
  {
    TEST_ENUM("enable_oversampling", conf->oversampling_on, boolean_data);
  }

  SECTION(resample_mode)
  {
    static const config_test_single data[] =
    {
      { "none", RESAMPLE_MODE_NONE },
      { "linear", RESAMPLE_MODE_LINEAR },
      { "cubic", RESAMPLE_MODE_CUBIC },
      { "fir", DEFAULT },
      { "sdfsedfsd", DEFAULT },
      { "0", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("resample_mode", conf->resample_mode, data);
  }

  SECTION(module_resample_mode)
  {
    static const config_test_single data[] =
    {
      { "none", RESAMPLE_MODE_NONE },
      { "linear", RESAMPLE_MODE_LINEAR },
      { "cubic", RESAMPLE_MODE_CUBIC },
      { "fir", RESAMPLE_MODE_FIR },
      { "sdfsedfsd", DEFAULT },
      { "0", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("module_resample_mode", conf->module_resample_mode, data);
  }

  SECTION(max_simultaneous_samples)
  {
    TEST_INT("max_simultaneous_samples", conf->max_simultaneous_samples, -1, INT_MAX);
  }

  SECTION(music_volume)
  {
    TEST_INT("music_volume", conf->music_volume, 0, 10);
  }

  SECTION(sam_volume)
  {
    TEST_INT("sample_volume", conf->sam_volume, 0, 10);
  }

  SECTION(pc_speaker_volume)
  {
    TEST_INT("pc_speaker_volume", conf->pc_speaker_volume, 0, 10);
  }

  SECTION(music_on)
  {
    TEST_ENUM("music_on", conf->music_on, boolean_data);
  }

  SECTION(pc_speaker_on)
  {
    TEST_ENUM("pc_speaker_on", conf->pc_speaker_on, boolean_data);
  }

  // Event options.

  // TODO Most joystick and gamecontroller options are stored elsewhere and
  // too hard to test right now.

#ifdef CONFIG_SDL
#if SDL_VERSION_ATLEAST(2,0,0)
  SECTION(allow_gamecontroller)
  {
    TEST_ENUM("gamecontroller_enable", conf->allow_gamecontroller, boolean_data);
  }
#endif
#endif

  SECTION(pause_on_unfocus)
  {
    TEST_ENUM("pause_on_unfocus", conf->pause_on_unfocus, boolean_data);
  }

  SECTION(num_buffered_events)
  {
    TEST_INT("num_buffered_events", conf->num_buffered_events, 1, 256);
  }

  // Game options.

  // TODO startup_path relies on stat.
  // TODO startup_file relies on stat.

  SECTION(save_file)
  {
    TEST_STRING("save_file", conf->default_save_name, string_data);
  }

  SECTION(mzx_speed)
  {
    game_allowed = true;
    TEST_INT("mzx_speed", conf->mzx_speed, 1, 16);
  }

  SECTION(allow_cheats)
  {
    static const config_test_single data[] =
    {
      { "0", ALLOW_CHEATS_NEVER },
      { "1", ALLOW_CHEATS_ALWAYS },
      { "never", ALLOW_CHEATS_NEVER },
      { "always", ALLOW_CHEATS_ALWAYS },
      { "mzxrun", ALLOW_CHEATS_MZXRUN },
      { "asdbfnsd", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("allow_cheats", conf->allow_cheats, data);
  }

  SECTION(auto_decrypt_worlds)
  {
    TEST_ENUM("auto_decrypt_worlds", conf->auto_decrypt_worlds, boolean_data);
  }

  SECTION(startup_editor)
  {
    TEST_ENUM("startup_editor", conf->startup_editor, boolean_data);
  }

  SECTION(standalone_mode)
  {
    TEST_ENUM("standalone_mode", conf->standalone_mode, boolean_data);
  }

  SECTION(no_titlescreen)
  {
    TEST_ENUM("no_titlescreen", conf->no_titlescreen, boolean_data);
  }

  SECTION(system_mouse)
  {
    TEST_ENUM("system_mouse", conf->system_mouse, boolean_data);
  }

  SECTION(grab_mouse)
  {
    TEST_ENUM("grab_mouse", conf->grab_mouse, boolean_data);
  }

  SECTION(save_slots)
  {
    TEST_ENUM("save_slots", conf->save_slots, boolean_data);
  }

  SECTION(save_slots_name)
  {
    TEST_STRING("save_slots_name", conf->save_slots_name, string_data);
  }

  SECTION(save_slots_ext)
  {
    TEST_STRING("save_slots_ext", conf->save_slots_ext, string_data);
  }

  // Editor options used by core.

  SECTION(test_mode)
  {
    TEST_ENUM("test_mode", conf->test_mode, boolean_data);
  }

  SECTION(test_mode_start_board)
  {
    TEST_INT("test_mode_start_board", conf->test_mode_start_board, 0, MAX_BOARDS - 1);
  }

  SECTION(mask_midchars)
  {
    TEST_ENUM("mask_midchars", conf->mask_midchars, boolean_data);
  }

#ifdef CONFIG_NETWORK

  // Network options.

  SECTION(network_enabled)
  {
    TEST_ENUM("network_enabled", conf->network_enabled, boolean_data);
  }

  SECTION(network_address_family)
  {
    static const config_test_single data[] =
    {
      { "0", HOST_FAMILY_ANY },
      { "any", HOST_FAMILY_ANY },
      { "ipv4", HOST_FAMILY_IPV4 },
      { "ipv6", HOST_FAMILY_IPV6 },
      { "", DEFAULT },
      { "1", DEFAULT },
      { "sdfasdf", DEFAULT },
      { "-1213", DEFAULT }
    };
    TEST_ENUM("network_address_family", conf->network_address_family, data);
  }

  SECTION(socks_host)
  {
    TEST_STRING("socks_host", conf->socks_host, string_data);
  }

  SECTION(socks_port)
  {
    TEST_INT("socks_port", conf->socks_port, 0, 65535);
  }

  SECTION(socks_username)
  {
    TEST_STRING("socks_username", conf->socks_username, string_data);
  }

  SECTION(socks_password)
  {
    TEST_STRING("socks_password", conf->socks_password, string_data);
  }

#endif /* CONFIG_NETWORK */

#ifdef CONFIG_UPDATER

  // Updater options.

  SECTION(update_host)
  {
    static const char * const option = "update_host";
    static const char * const hosts[] =
    {
      "updates.doot.rip",
      "updates.digitalmzx.com",
      "updates.megazuex.org",
      "looooo,.l",
      "a website",
      "sdjfjklsdfsdsdfsd",
      "",
      "PADDING"
    };
    char buffer[512];

    for(int i = 0; i < arraysize(hosts); i++)
    {
      snprintf(buffer, sizeof(buffer), "%s=%s", option, hosts[i]);
      load_arg(buffer);

      ASSERTEQ(conf->update_host_count, i+1);
      for(int j = 0; j <= i; j++)
        ASSERTCMP(conf->update_hosts[j], hosts[j]);
    }
    free_config();
  }

  SECTION(update_branch_pin)
  {
    TEST_STRING("update_branch_pin", conf->update_branch_pin, string_data);
  }

  SECTION(update_auto_check)
  {
    static const config_test_single data[] =
    {
      { "0", UPDATE_AUTO_CHECK_OFF },
      { "1", UPDATE_AUTO_CHECK_ON },
      { "off", UPDATE_AUTO_CHECK_OFF },
      { "on", UPDATE_AUTO_CHECK_ON },
      { "silent", UPDATE_AUTO_CHECK_SILENT },
      { "2", DEFAULT },
      { "asdfasdfasdf", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("update_auto_check", conf->update_auto_check, data);
  }

  SECTION(updater_enabled)
  {
    TEST_ENUM("updater_enabled", conf->updater_enabled, boolean_data);
  }

#endif /* CONFIG_UPDATER */

#ifdef CONFIG_EDITOR

  struct editor_config_info *econf = get_editor_config();
  default_editor_config();
  game_allowed = true;

  // Board editor options.

  SECTION(board_editor_hide_help)
  {
    TEST_ENUM("board_editor_hide_help", econf->board_editor_hide_help, boolean_data);
  }

  SECTION(editor_space_toggles)
  {
    TEST_ENUM("editor_space_toggles", econf->editor_space_toggles, boolean_data);
  }

  SECTION(editor_tab_focuses_view)
  {
    TEST_ENUM("editor_tab_focuses_view", econf->editor_tab_focuses_view, boolean_data);
  }

  SECTION(editor_load_board_assets)
  {
    TEST_ENUM("editor_load_board_assets", econf->editor_load_board_assets, boolean_data);
  }

  SECTION(editor_thing_menu_places)
  {
    TEST_ENUM("editor_thing_menu_places", econf->editor_thing_menu_places, boolean_data);
  }

  // Board defaults.

  SECTION(viewport_and_size)
  {
    static const config_test_board_size_viewport data[] =
    {
      // Setting      Value     w       h       vx      vy      vw      vh
      { "width",      "100",    100,    IGNORE, IGNORE, IGNORE, IGNORE, IGNORE },
      { "height",     "100",    100,    100,    IGNORE, IGNORE, IGNORE, IGNORE },
      { "viewport_w", "74",     100,    100,    IGNORE, IGNORE, 74,     IGNORE },
      { "viewport_h", "21",     100,    100,    IGNORE, IGNORE, 74,     21 },
      { "viewport_x", "3",      100,    100,    3,      IGNORE, 74,     21 },
      { "viewport_y", "2",      100,    100,    3,      2,      74,     21 },

      // Ignore invalid values...
      { "width",      "0",      100,    100,    3,      2,      74,     21 },
      { "width",      "32768",  100,    100,    3,      2,      74,     21 },
      { "height",     "0",      100,    100,    3,      2,      74,     21 },
      { "height",     "32768",  100,    100,    3,      2,      74,     21 },
      { "viewport_x", "-1",     100,    100,    3,      2,      74,     21 },
      { "viewport_x", "80",     100,    100,    3,      2,      74,     21 },
      { "viewport_y", "-1",     100,    100,    3,      2,      74,     21 },
      { "viewport_y", "25",     100,    100,    3,      2,      74,     21 },
      { "viewport_w", "0",      100,    100,    3,      2,      74,     21 },
      { "viewport_w", "81",     100,    100,    3,      2,      74,     21 },
      { "viewport_h", "0",      100,    100,    3,      2,      74,     21 },
      { "viewport_h", "26",     100,    100,    3,      2,      74,     21 },

      // Width * height is capped to 16M.
      { "width",      "32767",  32767,  100,    IGNORE, IGNORE, IGNORE, IGNORE },
      { "height",     "32767",  512,    32767,  IGNORE, IGNORE, IGNORE, IGNORE },
      { "width",      "4097",   4097,   4095,   IGNORE, IGNORE, IGNORE, IGNORE },
      { "height",     "4097",   4095,   4097,   IGNORE, IGNORE, IGNORE, IGNORE },

      // Width and height cap the viewport dimensions.
      { "width",      "40",     40,     IGNORE, IGNORE, IGNORE, 40,     IGNORE },
      { "height",     "15",     IGNORE, 15,     IGNORE, IGNORE, IGNORE, 15 },

      // Viewport size values over the board size and under the cap work (capped).
      { "width",      "60",     60,     IGNORE, IGNORE, IGNORE, IGNORE, IGNORE },
      { "viewport_w", "75",     60,     IGNORE, IGNORE, IGNORE, 60,     IGNORE },
      { "height",     "20",     IGNORE, 20,     IGNORE, IGNORE, IGNORE, IGNORE },
      { "viewport_h", "25",     IGNORE, 20,     IGNORE, IGNORE, IGNORE, 20 },

      // Viewport size overrides a previously set viewport position.
      { "width",      "100",    100,    IGNORE, IGNORE, IGNORE, IGNORE, IGNORE },
      { "height",     "100",    100,    100,    IGNORE, IGNORE, IGNORE, IGNORE },
      { "viewport_x", "10",     IGNORE, IGNORE, 10,     IGNORE, IGNORE, IGNORE },
      { "viewport_y", "5",      IGNORE, IGNORE, 10,     5,      IGNORE, IGNORE },
      { "viewport_w", "80",     IGNORE, IGNORE, 0,      IGNORE, 80,     IGNORE },
      { "viewport_h", "25",     IGNORE, IGNORE, IGNORE, 0,      IGNORE, 25 },

      // Viewport position respects previously set viewport size.
      { "viewport_w", "70",     IGNORE, IGNORE, IGNORE, IGNORE, 70,     IGNORE },
      { "viewport_h", "20",     IGNORE, IGNORE, IGNORE, IGNORE, 70,     20 },
      { "viewport_x", "5",      IGNORE, IGNORE, 5,      IGNORE, 70,     IGNORE },
      { "viewport_x", "15",     IGNORE, IGNORE, 10,     IGNORE, 70,     IGNORE },
      { "viewport_y", "2",      IGNORE, IGNORE, IGNORE, 2,      IGNORE, 20 },
      { "viewport_y", "10",     IGNORE, IGNORE, IGNORE, 5,      IGNORE, 20 },
    };
    char buffer[512];

    for(int i = 0; i < arraysize(data); i++)
    {
      const config_test_board_size_viewport &current = data[i];
      snprintf(buffer, sizeof(buffer), "board_default_%s=%s",
       current.setting, current.value);

      load_arg(buffer);

      if(current.width != IGNORE)
        ASSERTEQX(econf->board_width, current.width, buffer);
      if(current.height != IGNORE)
        ASSERTEQX(econf->board_height, current.height, buffer);
      if(current.viewport_x != IGNORE)
        ASSERTEQX(econf->viewport_x, current.viewport_x, buffer);
      if(current.viewport_y != IGNORE)
        ASSERTEQX(econf->viewport_y, current.viewport_y, buffer);
      if(current.viewport_w != IGNORE)
        ASSERTEQX(econf->viewport_w, current.viewport_w, buffer);
      if(current.viewport_h != IGNORE)
        ASSERTEQX(econf->viewport_h, current.viewport_h, buffer);
    }
  }

  SECTION(can_shoot)
  {
    TEST_ENUM("board_default_can_shoot", econf->can_shoot, boolean_data);
  }

  SECTION(can_bomb)
  {
    TEST_ENUM("board_default_can_bomb", econf->can_bomb, boolean_data);
  }

  SECTION(fire_burns_spaces)
  {
    TEST_ENUM("board_default_fire_burns_spaces", econf->fire_burns_spaces, boolean_data);
  }

  SECTION(fire_burns_fakes)
  {
    TEST_ENUM("board_default_fire_burns_fakes", econf->fire_burns_fakes, boolean_data);
  }

  SECTION(fire_burns_trees)
  {
    TEST_ENUM("board_default_fire_burns_trees", econf->fire_burns_trees, boolean_data);
  }

  SECTION(fire_burns_brown)
  {
    TEST_ENUM("board_default_fire_burns_brown", econf->fire_burns_brown, boolean_data);
  }

  SECTION(fire_burns_forever)
  {
    TEST_ENUM("board_default_fire_burns_forever", econf->fire_burns_forever, boolean_data);
  }

  SECTION(forest_to_floor)
  {
    TEST_ENUM("board_default_forest_to_floor", econf->forest_to_floor, boolean_data);
  }

  SECTION(collect_bombs)
  {
    TEST_ENUM("board_default_collect_bombs", econf->collect_bombs, boolean_data);
  }

  SECTION(restart_if_hurt)
  {
    TEST_ENUM("board_default_restart_if_hurt", econf->restart_if_hurt, boolean_data);
  }

  SECTION(reset_on_entry)
  {
    TEST_ENUM("board_default_reset_on_entry", econf->reset_on_entry, boolean_data);
  }

  SECTION(player_locked_ns)
  {
    TEST_ENUM("board_default_player_locked_ns", econf->player_locked_ns, boolean_data);
  }

  SECTION(player_locked_ew)
  {
    TEST_ENUM("board_default_player_locked_ew", econf->player_locked_ew, boolean_data);
  }

  SECTION(player_locked_att)
  {
    TEST_ENUM("board_default_player_locked_att", econf->player_locked_att, boolean_data);
  }

  SECTION(time_limit)
  {
    TEST_INT("board_default_time_limit", econf->time_limit, 0, 32767);
  }

  SECTION(explosions_leave)
  {
    static const config_test_single data[] =
    {
      { "space", EXPL_LEAVE_SPACE },
      { "ash", EXPL_LEAVE_ASH },
      { "fire", EXPL_LEAVE_FIRE },
      { "0", DEFAULT },
      { "1", DEFAULT },
      { "2", DEFAULT },
      { "1a", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("board_default_explosions_leave", econf->explosions_leave, data);
  }

  SECTION(saving_enabled)
  {
    static const config_test_single data[] =
    {
      { "disabled", CANT_SAVE },
      { "enabled", CAN_SAVE },
      { "sensoronly", CAN_SAVE_ON_SENSOR },
      { "0", DEFAULT },
      { "1", DEFAULT },
      { "2", DEFAULT },
      { "sdfbv", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("board_default_saving", econf->saving_enabled, data);
  }

  SECTION(overlay_enabled)
  {
    static const config_test_single data[] =
    {
      { "disabled", OVERLAY_OFF },
      { "enabled", OVERLAY_ON },
      { "static", OVERLAY_STATIC },
      { "transparent", OVERLAY_TRANSPARENT },
      { "-1", DEFAULT },
      { "1", DEFAULT },
      { "2", DEFAULT },
      { "disabledb", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("board_default_overlay", econf->overlay_enabled, data);
  }

  SECTION(charset_path)
  {
    TEST_STRING("board_default_charset_path", econf->charset_path, string_data);
  }

  SECTION(palette_path)
  {
    TEST_STRING("board_default_palette_path", econf->palette_path, string_data);
  }

  // Palette editor options.

  SECTION(palette_editor_hide_help)
  {
    TEST_ENUM("palette_editor_hide_help", econf->palette_editor_hide_help, boolean_data);
  }

  // Robot editor options.

  SECTION(editor_enter_splits)
  {
    TEST_ENUM("editor_enter_splits", econf->editor_enter_splits, boolean_data);
  }

  SECTION(color_codes)
  {
    static const struct
    {
      const char * const opt;
      unsigned int idx;
    } color_code_opts[] =
    {
      { "ccode_chars", ROBO_ED_CC_CHARACTERS },
      { "ccode_colors", ROBO_ED_CC_COLORS },
      { "ccode_commands", ROBO_ED_CC_COMMANDS },
      { "ccode_conditions", ROBO_ED_CC_CONDITIONS },
      { "ccode_current_line", ROBO_ED_CC_CURRENT_LINE },
      { "ccode_directions", ROBO_ED_CC_DIRECTIONS },
      { "ccode_equalities", ROBO_ED_CC_EQUALITIES },
      { "ccode_extras", ROBO_ED_CC_EXTRAS },
      { "ccode_immediates", ROBO_ED_CC_IMMEDIATES },
      { "ccode_items", ROBO_ED_CC_ITEMS },
      { "ccode_params", ROBO_ED_CC_PARAMS },
      { "ccode_strings", ROBO_ED_CC_STRINGS },
      { "ccode_things", ROBO_ED_CC_THINGS },
    };
    static const config_test_single color_data[] =
    {
      { "255", 255 },
    };

    for(int i = 0; i < arraysize(color_code_opts); i++)
    {
      auto opt = color_code_opts[i];
      TEST_INT(opt.opt, econf->color_codes[opt.idx], 0, 15);
    }

    // Test the colors special case...
    TEST_ENUM("ccode_colors", econf->color_codes[ROBO_ED_CC_COLORS], color_data);
  }

  SECTION(color_coding_on)
  {
    TEST_ENUM("color_coding_on", econf->color_coding_on, boolean_data);
  }

#ifndef CONFIG_DEBYTECODE
  SECTION(default_invalid_status)
  {
    static const config_test_single data[] =
    {
      { "ignore", invalid_uncertain },
      { "delete", invalid_discard },
      { "comment", invalid_comment },
      { "-1", DEFAULT },
      { "1", DEFAULT },
      { "2", DEFAULT },
      { "disabledb", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("default_invalid_status", econf->default_invalid_status, data);
  }
#endif

  SECTION(disassemble_extras)
  {
    TEST_ENUM("disassemble_extras", econf->disassemble_extras, boolean_data);
  }

  SECTION(disassemble_base)
  {
    static const config_test_single data[] =
    {
      { "10", 10 },
      { "16", 16 },
      { "2", DEFAULT },
      { "4", DEFAULT },
      { "8", DEFAULT },
      { "10bfd", DEFAULT },
      { "11", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("disassemble_base", econf->disassemble_base, data);
  }

  SECTION(robot_editor_hide_help)
  {
    TEST_ENUM("robot_editor_hide_help", econf->robot_editor_hide_help, boolean_data);
  }

  // Backup options.

  SECTION(backup_count)
  {
    TEST_INT("backup_count", econf->backup_count, 0, INT_MAX);
  }

  SECTION(backup_interval)
  {
    TEST_INT("backup_interval", econf->backup_interval, 0, INT_MAX);
  }

  SECTION(backup_name)
  {
    TEST_STRING("backup_name", econf->backup_name, string_data);
  }

  SECTION(backup_ext)
  {
    TEST_STRING("backup_ext", econf->backup_ext, string_data);
  }

  // Macro options.

  // TODO extended macros should be a separate test.

  SECTION(default_macros)
  {
    char buffer[16];
    for(int i = 0; i < arraysize(econf->default_macros); i++)
    {
      snprintf(buffer, sizeof(buffer), "macro_%d", i+1);
      TEST_STRING(buffer, econf->default_macros[i], string_data);
    }
  }

  // Saved positions.

  SECTION(saved_position)
  {
    static const config_test_saved_position data[] =
    {
      { 0, "1,2,3,4,5,0",       { 1, 2, 3, 4, 5, 0 } },
      { 0, "",                  { 1, 2, 3, 4, 5, 0 } },
      { 1, "9,8,7,6,5,60",      { 9, 8, 7, 6, 5, 60 } },
      { 2, "249,32767,32767,32767,32767,60", { 249, 32767, 32767, 32767, 32767, 60 } },
      { 3, "3,3,3,3,3,0",       { 3, 3, 3, 3, 3, 0 } },
      { 4, "4,4,4,4,4,0",       { 4, 4, 4, 4, 4, 0 } },
      { 5, "5,5,5,5,5,0",       { 5, 5, 5, 5, 5, 0 } },
      { 6, "6,6,6,6,6,0",       { 6, 6, 6, 6, 6, 0 } },
      { 7, "7,7,7,7,7,0",       { 7, 7, 7, 7, 7, 0 } },
      { 8, "8,8,8,8,8,0",       { 8, 8, 8, 8, 8, 0 } },
      { 9, "9,9,9,9,9,0",       { 9, 9, 9, 9, 9, 0 } },
      { 8, "",                  { 8, 8, 8, 8, 8, 0 } },
      { 7, "",                  { 7, 7, 7, 7, 7, 0 } },
      { 6, "",                  { 6, 6, 6, 6, 6, 0 } },
      { 5, "",                  { 5, 5, 5, 5, 5, 0 } },
      { 4, "",                  { 4, 4, 4, 4, 4, 0 } },
      { 3, "",                  { 3, 3, 3, 3, 3, 0 } },
      // Make sure this at least has rudimentary bounding for invalid things...
      { 0, "0,0,0,0,0,0",       { 0, 0, 0, 0, 0, 0 } },
      { 0, "250,0,0,0,0,0",     { 0, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE } },
      { 0, "-1,0,0,0,0,0",      { 0, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE } },
      { 0, "9,32768,0,0,0,0,0", { 0, 0,      IGNORE, IGNORE, IGNORE, IGNORE } },
      { 0, "9,-1,0,0,0,0",      { 0, 0,      IGNORE, IGNORE, IGNORE, IGNORE } },
      { 0, "9,0,32768,0,0,0",   { 0, IGNORE, 0,      IGNORE, IGNORE, IGNORE } },
      { 0, "9,0,-1,0,0,0",      { 0, IGNORE, 0,      IGNORE, IGNORE, IGNORE } },
      { 0, "9,0,0,32768,0,0",   { 0, IGNORE, IGNORE, 0,      IGNORE, IGNORE } },
      { 0, "9,0,0,-1,0,0",      { 0, IGNORE, IGNORE, 0,      IGNORE, IGNORE } },
      { 0, "9,0,0,0,32768,0",   { 0, IGNORE, IGNORE, IGNORE, 0,      IGNORE } },
      { 0, "9,0,0,0,-1,0",      { 0, IGNORE, IGNORE, IGNORE, 0,      IGNORE } },
      { 0, "9,0,0,0,0,80",      { 0, IGNORE, IGNORE, IGNORE, IGNORE, 0 } },
      { 0, "9,0,0,0,0,-1",      { 0, IGNORE, IGNORE, IGNORE, IGNORE, 0 } },
    };
    const struct saved_position *dest;
    const struct saved_position *expected;
    char buffer[256];

    for(int i = 0; i < arraysize(data); i++)
    {
      snprintf(buffer, sizeof(buffer), "saved_position%d=%s",
       data[i].which, data[i].value);

      load_arg(buffer);

      dest = &(econf->saved_positions[data[i].which]);
      expected = &(data[i].expected);

      if(expected->board_id != IGNORE)
        ASSERTEQX(dest->board_id, expected->board_id, buffer);
      if(expected->cursor_x != IGNORE)
        ASSERTEQX(dest->cursor_x, expected->cursor_x, buffer);
      if(expected->cursor_y != IGNORE)
        ASSERTEQX(dest->cursor_y, expected->cursor_y, buffer);
      if(expected->scroll_x != IGNORE)
        ASSERTEQX(dest->scroll_x, expected->scroll_x, buffer);
      if(expected->scroll_y != IGNORE)
        ASSERTEQX(dest->scroll_y, expected->scroll_y, buffer);
      if(expected->debug_x != IGNORE)
        ASSERTEQX(dest->debug_x, expected->debug_x, buffer);
    }
  }

#endif /* CONFIG_EDITOR */
}

UNITTEST(Include)
{
  struct config_info *conf = get_config();
  default_config();

#ifdef CONFIG_EDITOR
  struct editor_config_info *econf = get_editor_config();
  default_editor_config();
#endif

  SECTION(Include)
  {
    // This version only works from a config file.
    boolean ret = write_config("a.cnf", "include b.cnf");
    ret &= write_config("b.cnf", "mzx_speed = 4");
    ASSERTEQ(ret, true);

    conf->mzx_speed = 2;

    set_config_from_file(SYSTEM_CNF, "a.cnf");
    ASSERTEQ(conf->mzx_speed, 4);
  }

  SECTION(IncludeEquals)
  {
    // This version works from both the config file and the command line.
    char include_conf[] = "include=c.cnf";
    boolean ret = write_config("c.cnf", "mzx_speed = 6");
    ASSERTEQ(ret, true);

    conf->mzx_speed = 1;

    load_arg(include_conf);
    ASSERTEQX(conf->mzx_speed, 6, include_conf);

    conf->mzx_speed = 2;

    load_arg_file(include_conf, false);
    ASSERTEQX(conf->mzx_speed, 6, include_conf);
  }

  SECTION(Recursion)
  {
    // Make sure a sane amount of recursive includes work.
    // Also make sure this works for both the main and editor configuration.
    boolean ret = write_config("a.cnf", "include b.cnf");
    ret &= write_config("b.cnf", "include c.cnf");
    ret &= write_config("c.cnf", "mzx_speed=5\nboard_editor_hide_help=1");
    ASSERTEQ(ret, true);

    conf->mzx_speed = 1;
#ifdef CONFIG_EDITOR
    econf->board_editor_hide_help = false;
#endif

    load_arg((char *)"include=a.cnf");
    ASSERTEQ(conf->mzx_speed, 5);
#ifdef CONFIG_EDITOR
    ASSERTEQ(econf->board_editor_hide_help, true);
#endif
  }

  SECTION(RecursionLimit)
  {
    // Make sure recursive include fails gracefully. Even before the hard limit
    // was added this would happen due to MZX hitting the maximum allowed number
    // of file descriptors prior to running out of stack. This is pretty much
    // a freebie, it just needs to not crash or run out of memory.
    boolean ret = write_config("a.cnf", "include a.cnf");
    ASSERTEQ(ret, true);

    set_config_from_file(SYSTEM_CNF, "a.cnf");
  }
}

#ifdef CONFIG_EDITOR

/*
UNITTEST(ExtendedMacros)
{
  // FIXME
  UNIMPLEMENTED();

  free_editor_config();
}
*/

#endif /* CONFIG_EDITOR */
