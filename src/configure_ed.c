/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
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

#include "configure.h"
#include "core.h"
#include "event.h"
#include "game.h"
#include "graphics.h"
#include "window.h"
#include "world_struct.h"

#include "audio/audio.h"

// Settings dialog-

//----------------------------
//
//    Speed- [   NN][^][v]
//
//   ( ) Digitized music on
//   ( ) Digitized music off
//
//   ( ) PC speaker SFX on
//   ( ) PC speaker SFX off
//
//  Sound card volumes-       //
//  Overall volume- 12345678  //
//  SoundFX volume- 12345678  //
//  PC Speaker SFX- 12345678  //
//
//      [OK]     [Cancel]
//
//----------------------------//

//  [OK]  [Cancel]  [Shader]  //

#ifdef CONFIG_RENDER_GL_PROGRAM
// Note- we search for .frags, then load the matching .vert too if present.
static const char *shader_exts[] = { ".frag", NULL };
static char shader_default_text[20];
static char shader_path[MAX_PATH];
static char shader_name[MAX_PATH];
#endif

void game_settings(struct world *mzx_world)
{
  struct board *src_board = mzx_world->current_board;
  int mzx_speed, music, pcs;
  int music_volume, sound_volume, pcs_volume;
  struct dialog di;
  int dialog_result;
  int speed_option = 0;
  int shader_option = 0;
  int num_elements = 8;
  int start_option = 0;

  int ok_pos;
  int cancel_pos;
  int speed_pos;

  const char *radio_strings_1[2] =
  {
    "Digitized music on", "Digitized music off"
  };
  const char *radio_strings_2[2] =
  {
    "PC speaker SFX on", "PC speaker SFX off"
  };
  struct element *elements[10];

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_CONFIGURE);

#ifdef CONFIG_RENDER_GL_PROGRAM
  if(!strcmp(mzx_world->conf.video_output, "glsl"))
  {
    if(!shader_default_text[0])
    {
      if(mzx_world->conf.gl_scaling_shader[0])
        snprintf(shader_default_text, 20, "<conf: %.11s>",
         mzx_world->conf.gl_scaling_shader);

      else
        strcpy(shader_default_text, "<default: semisoft>");
    }

    shader_option = 3;
    num_elements++;
  }
#endif

  if(!mzx_world->lock_speed)
  {
    speed_option = 2;
    start_option = num_elements;
    num_elements++;
  }

  mzx_speed = mzx_world->mzx_speed;
  music = !audio_get_music_on();
  pcs = !audio_get_pcs_on();
  music_volume = audio_get_music_volume();
  sound_volume = audio_get_sound_volume();
  pcs_volume = audio_get_pcs_volume();

  do
  {
    ok_pos = 6;
    cancel_pos = 7;
    speed_pos = 8;

#ifdef CONFIG_RENDER_GL_PROGRAM
    if(!strcmp(mzx_world->conf.video_output, "glsl"))
    {
      strcpy(shader_path, mzx_res_get_by_id(SHADERS_SCALER_DIRECTORY));

      elements[6] = construct_file_selector(3, 13 + speed_option,
       "Scaling shader-", "Choose a scaling shader...", shader_exts,
       shader_default_text, 21, 0, shader_path, shader_name, 2);

      ok_pos++;
      cancel_pos++;
      speed_pos++;
    }
#endif

    if(!mzx_world->lock_speed)
    {
      elements[speed_pos] = construct_number_box(5, 2, "Speed- ", 1, 16,
       0, &mzx_speed);
    }

    elements[0] = construct_radio_button(4, 2 + speed_option,
     radio_strings_1, 2, 19, &music);
    elements[1] = construct_radio_button(4, 5 + speed_option,
     radio_strings_2, 2, 18, &pcs);
    elements[2] = construct_label(3, 8 + speed_option,
     "Audio volumes-");
    elements[3] = construct_number_box(3, 9 + speed_option,
     "Overall volume- ", 1, 8, 0, &music_volume);
    elements[4] = construct_number_box(3, 10 + speed_option,
     "SoundFX volume- ", 1, 8, 0, &sound_volume);
    elements[5] = construct_number_box(3, 11 + speed_option,
     "PC Speaker SFX- ", 1, 8, 0, &pcs_volume);

    elements[ok_pos] = construct_button(6, 13 + speed_option + shader_option,
     "OK", 0);

    elements[cancel_pos] = construct_button(15, 13 + speed_option + shader_option,
     "Cancel", 1);

    construct_dialog(&di, "Game Settings", 25, 4 - ((speed_option+shader_option) / 2),
     30, 16 + speed_option + shader_option, elements, num_elements, start_option);

    dialog_result = run_dialog(mzx_world, &di);
    start_option = di.current_element;
    destruct_dialog(&di);

    // Prevent UI keys from carrying through.
    force_release_all_keys();

    if(!dialog_result)
    {
      audio_set_pcs_volume(pcs_volume);

      if(music_volume != audio_get_music_volume())
      {
        audio_set_music_volume(music_volume);

        if(mzx_world->active)
          audio_set_module_volume(src_board->volume);
      }

      if(sound_volume != audio_get_sound_volume())
        audio_set_sound_volume(sound_volume);

      music ^= 1;
      pcs ^= 1;

      audio_set_pcs_on(pcs);

      if(!music)
      {
        // Turn off music.
        if(audio_get_music_on() && (mzx_world->active))
          audio_end_module();
      }
      else

      if(!audio_get_music_on() && (mzx_world->active))
      {
        // Turn on music.
        load_board_module(mzx_world);
      }

      audio_set_music_on(music);
      mzx_world->mzx_speed = mzx_speed;
    }

#ifdef CONFIG_RENDER_GL_PROGRAM
    else

    // Shader selection
    if(dialog_result == 2 && shader_name[0])
    {
      size_t offset = strlen(shader_path) + 1;
      bool shader_res;
      char *pos;

      if(strlen(shader_name) > offset)
      {
        pos = strrchr(shader_name + offset, '.');

        if(pos) *pos = 0;
        shader_res = switch_shader(shader_name + offset);
        if(pos) *pos = '.';

        if(!shader_res)
        {
          // If the shader failed to load, the default should have been loaded.
          strcpy(shader_default_text, "<default: semisoft>");
          shader_name[0] = 0;
        }
      }
    }
#endif
  }
  while(dialog_result > 1);

  pop_context();
}
