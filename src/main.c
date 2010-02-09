/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2002 B.D.A. (Koji) - Koji_Takeo@worldmailer.com
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "compat.h"
#include "platform.h"

#include "configure.h"
#include "event.h"
#include "helpsys.h"
#include "sfx.h"
#include "graphics.h"
#include "window.h"
#include "data.h"
#include "game.h"
#include "error.h"
#include "idput.h"
#include "audio.h"
#include "util.h"
#include "world.h"
#include "counter.h"
#include "run_stubs.h"
#include "network/network.h"

#ifndef VERSION
#error Must define VERSION for MegaZeux version string
#endif

#ifndef VERSION_DATE
#define VERSION_DATE
#endif

#define CAPTION "MegaZeux " VERSION VERSION_DATE

#ifdef __amigaos__
#define __libspec LIBSPEC
#else
#define __libspec
#endif

// The world structure used to be pretty big (around 7.2k) which
// caused some platforms grief. Early hacks moved it entirely onto
// the heap, but this will reduce performance of a constantly
// accessed data structure in multiple hot paths.

// As a compromise, move out stuff that's accessed less frequently,
// or which is simply too large to be worth it. This means we need
// a couple of functions for allocating/freeing MZX worlds.

static void allocate_world(struct world *mzx_world)
{
  memset(mzx_world, 0, sizeof(struct world));

  mzx_world->real_mod_playing = malloc(256);
  memset(mzx_world->real_mod_playing, 0, 256);

  mzx_world->input_file_name = malloc(512);
  memset(mzx_world->input_file_name, 0, 512);

  mzx_world->output_file_name = malloc(512);
  memset(mzx_world->output_file_name, 0, 512);

  mzx_world->global_robot = malloc(sizeof(struct robot));
  memset(mzx_world->global_robot, 0, sizeof(struct robot));

  mzx_world->custom_sfx = calloc(69, NUM_SFX);
}

static void free_world(struct world *mzx_world)
{
  // allocated once by world.c:set_update_done()
  if(mzx_world->update_done)
    free(mzx_world->update_done);

  // could be a nop in an editor-free build
  free_extended_macros(mzx_world);

  free(mzx_world->real_mod_playing);
  free(mzx_world->input_file_name);
  free(mzx_world->output_file_name);
  free(mzx_world->global_robot);
  free(mzx_world->custom_sfx);
}

__libspec int main(int argc, char *argv[])
{
  struct world mzx_world;

  if(!platform_init())
    return 1;

  editor_init();

  // We need to store the current working directory so it's
  // always possible to get back to it..
  getcwd(current_dir, MAX_PATH);

  if(mzx_res_init(argv[0]))
    goto err_free_res;

  allocate_world(&mzx_world);
  init_macros(&mzx_world);

  // Figure out where all configuration files should be loaded
  // form. For game.cnf, et al this should eventually be wrt
  // the game directory, not the config.txt's path.
  get_path(mzx_res_get_by_id(CONFIG_TXT), config_dir, MAX_PATH);

  // Move into the configuration file's directory so that any
  // "include" statements are done wrt this path. Move back
  // into the "current" directory after loading.
  chdir(config_dir);

  default_config(&mzx_world.conf);
  set_config_from_file(&mzx_world.conf, mzx_res_get_by_id(CONFIG_TXT));
  set_config_from_command_line(&mzx_world.conf, argc, argv);

  load_editor_config(&mzx_world, argc, argv);

  chdir(current_dir);

  counter_fsg();

  initialize_joysticks();

  set_mouse_mul(8, 14);

  if(network_layer_init(&mzx_world.conf))
  {
    if(!updater_init(argv))
      info("Updater disabled.\n");
  }
  else
    info("Network layer disabled.\n");

  init_event();

  if(!init_video(&mzx_world.conf, CAPTION))
    goto err_network_layer_exit;
  init_audio(&(mzx_world.conf));

  warp_mouse(39, 12);
  cursor_off();
  default_scroll_values(&mzx_world);

#ifdef CONFIG_HELPSYS
  help_open(&mzx_world, mzx_res_get_by_id(MZX_HELP_FIL));
#endif

  strncpy(curr_file, mzx_world.conf.startup_file, MAX_PATH - 1);
  curr_file[MAX_PATH - 1] = '\0';
  strncpy(curr_sav, mzx_world.conf.default_save_name, MAX_PATH - 1);
  curr_sav[MAX_PATH - 1] = '\0';

  mzx_world.mzx_speed = mzx_world.conf.mzx_speed;
  mzx_world.default_speed = mzx_world.mzx_speed;

  // Run main game (mouse is hidden and palette is faded)
  title_screen(&mzx_world);

  vquick_fadeout();

  if(mzx_world.active)
  {
    clear_world(&mzx_world);
    clear_global_data(&mzx_world);
  }

#ifdef CONFIG_HELPSYS
  help_close(&mzx_world);
#endif

  quit_audio();

err_network_layer_exit:
  network_layer_exit(&mzx_world.conf);
  free_world(&mzx_world);
err_free_res:
  mzx_res_free();
  platform_quit();
  return 0;
}
