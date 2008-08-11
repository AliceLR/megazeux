/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2002 B.D.A. (Koji) - Koji_Takeo@worldmailer.com
 * Copyright (C) 2002 Gilead Kutnick - exophase@adelphia.net
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

#include "configure.h"
#include "event.h"
#include "helpsys.h"
#include "sfx.h"
#include "edit.h"
#include "graphics.h"
#include "window.h"
#include "main.h"
#include "data.h"
#include "game.h"
#include "error.h"
#include "world.h"
#include "idput.h"
#include "audio.h"
#include "robo_ed.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define SAVE_INDIVIDUAL

char *reg_exit_mesg =
  "Thank you for playing MegaZeux.\n\r"
  "Read the files help.txt, megazeux.doc and readme.1st if you need help.\n\n\r"
  "MegaZeux, by Greg Janson\n\r"
  "Additional contributors:\n\r"
  "\tSpider124, CapnKev, MenTaLguY, JZig, Akwende, Koji, Exophase\n\n\r"
  "Check the whatsnew file for new additions.\n\n\r"
  "Check http://www.digitalmzx.net/ for MZX source and binary distributions.\n\r$";

int main(int argc, char **argv)
{
  int i;
  World mzx_world;

#ifdef __WIN32__
  //freopen("CON", "wb", stdout);
#endif

  memset(&mzx_world, 0, sizeof(World));
  default_config(&(mzx_world.conf));
  set_config_from_file(&(mzx_world.conf), CONFIG_TXT);
  set_config_from_command_line(&(mzx_world.conf), argc, argv);

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
  SDL_EnableUNICODE(1);
  
  initialize_joysticks();

  warp_mouse(39, 12);

  // Setup directory strings
  // Get megazeux directory

  strcpy(help_file, argv[0]);

  // Chop at last backslash
  for(i = strlen(help_file); i >= 0; i--)
  {
    if(help_file[i] == '\\')
      break;
  }

  help_file[i + 1] = 0;

  strcat(help_file, MZX_HELP_FIL);
  // Get current directory and drive (form- C:\DIR\SUBDIR)
  getcwd(current_dir, PATHNAME_SIZE);
  strcpy(megazeux_dir, current_dir);
  megazeux_drive = current_drive;

  strcpy(curr_file, mzx_world.conf.startup_file);
  strcpy(curr_sav, mzx_world.conf.default_save_name);
  music_on = mzx_world.conf.music_on;
  music_gvol = mzx_world.conf.music_volume;
  sound_gvol = mzx_world.conf.sam_volume;
  sfx_on = mzx_world.conf.pc_speaker_on;
  overall_speed = mzx_world.conf.mzx_speed;

  memcpy(macros, mzx_world.conf.default_macros, 5 * 64);

  // Init video (will init palette too)
  init_video(&(mzx_world.conf));
  init_audio(&(mzx_world.conf));
  cursor_off();
  default_scroll_values(&mzx_world);

  // Run main game (mouse is hidden and palette is faded)
  title_screen(&mzx_world);

  vquick_fadeout();

  if(mzx_world.active)
  {
    clear_world(&mzx_world);
    clear_global_data(&mzx_world);
  }

  SDL_Quit();

  return 0;
}
