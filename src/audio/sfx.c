/* MegaZeux
 *
 * Copyright (C) 1996  Greg Janson
 * Copyright (C) 2004  Gilead Kutnick <exophase@adelphia.net>
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

/* Sound effects system- PLAY, SFX, and bkground code */

#include <string.h>
#include <stdlib.h>

#include "audio.h"
#include "audio_pcs.h"
#include "sfx.h"

#include "../data.h"
#include "../world_struct.h"

#if defined(CONFIG_AUDIO) || defined(CONFIG_EDITOR)

__editor_maybe_static char sfx_strs[NUM_SFX][SFX_SIZE] =
{
  "5c-gec-gec", // Gem
  "5c-gec-gec", // Magic Gem
  "cge-zcge", // Health
  "-c+c+c-g-g-g", // Ammo
  "6a#a#zx", // Coin
  "-cegeg+c-g+cecegeg+c", // Life
  "-cc#dd#e", // Lo Bomb
  "cc#dd#e", // Hi Bomb
  "4gec-g+ec-ge+c-gec", // Key
  "-gc#-a#a", // Full Keys
  "ceg+c-eg+ce-g+ceg", // Unlock
  "s1a#z+a#-a#x", // Can't Unlock
  "-a-a+e-e+c-c", // Invis. Wall
  "zax", // Forest
  "0a#+a#-a#b+b-b", // Gate Locked
  "0a+a-a+a-a", // Opening Gate
  "-g+g-g+g+g-g+g-g-g+g-g-tg+g", // Invinco Start
  "sc-cqxsg-g+a#-a#xx+g-g+a#-a#", // Invinco Beat
  "sc-cqxsg-g+f-f+d#-d#+c-ca#-a#2c-c", // Invinco End
  "0g+g-gd#+d#-d#", // 19-Door locked
  "0g+gd#+d#", // Door opening
  "c-zgd#c-gd#c", // Hurt
  "a#e-a#e-a#e-a#e", // AUGH!
  "+c-gd#cd#c-gd#gd#c-g+c-gd#cd#c-gd#gd#c", // Death
  "ic1c+ca#+c1c+cx+d#1c+ca#+c1c+cx"
  "+c1c+ca#+c1c+cx+d#1c+c+fc1c+cx", // Game over
  "0c+c-c+c-c", //25 - Gate closing
  "1d#x", //26-Push
  "2c+c+c2d#+d#+d#2g+g+g3cd#g", // 27-Transport
  "z+c-c",// 28-shoot
  "1a+a+a", // 29-break
  "-af#-f#a", // 30-out of ammo
  "+f#", // 31-ricochet
  "s-d-d-d", // 32-out of bombs
  "c-aec-a", // 33-place bomb (lo)
  "+c-aec-a", // 34-place bomb (hi)
  "+g-ege-ege", // 35-switch bomb type
  "1c+c0d#+d#-g+g-c#", // 36-explosion
  "2cg+e+c2c#g#+f+c#2da+f#+d2d#a#+g+d#", // 37-Entrance
  "cge+c-g+ecge+c-g+ec", // 38-Pouch
  "zcd#gd#cd#gd#cd#gd#cd#gd#"
  "cfg#fcfg#fcfg#fcfg#f"
  "cga#gcga#gcga#gcga#g"
  "s+c",//39-ring/potion
  "z-a-a-aa-aa", // 40-Empty chest
  "1c+c-c#+c#-d+d-d#+d#-e+e-ec", // 41-Chest
  "c-gd#c-zd#gd#c", // 42-Out of time
  "zc-d#g-cd#", // 43-Fire ouch
  "cd#g+cd#g", // 44-Stolen gem
  "z1d#+d#+d#", // 45-Enemy HP down
  "z0ca#f+d#cf#", // 46-Dragon fire
  "cg+c-eda+d-f#eb+e-g#", // 47-Scroll/sign
  "1c+c+c+c+c", // 48-Goop
  "" // 49-Unused
};

#endif // CONFIG_AUDIO || CONFIG_EDITOR

#ifdef CONFIG_AUDIO

// Special freqs
#define F_REST          1

static int topindex = 0;  // Marks the top of the queue
static int backindex = 0; // Marks bottom of queue

struct noise
{
  int duration;//This is the struc of the sound queue
  int freq;
};

// Frequencies of 6C thru 6B
static const int note_freq[12] =
 { 2032, 2152, 2280, 2416, 2560, 2712, 2880, 3048, 3232, 3424, 3624, 3840 };

// Sample frequencies of 0C thru 0B
static const int sam_freq[12] =
 { 3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812 };

static struct noise background[NOISEMAX]; // The sound queue itself
static int sound_in_queue = 0;     // Tells if sound in queue

#ifdef CONFIG_DEBYTECODE
static const char open_char  = '(';
static const char close_char = ')';
#else
static const char open_char  = '&';
static const char close_char = '&';
#endif

static void submit_sound(int freq, int delay)
{
  if((backindex == 0) && (topindex == NOISEMAX))
    return;
  if(topindex != (backindex - 1))
  {
    // Queue full?
    background[topindex].freq = freq; // No, put it in queue
    background[topindex++].duration = delay;
    if(topindex == NOISEMAX)
      topindex = 0; // Wraparound
    sound_in_queue = 1; // Note sound in queue
  }
}

static void play_note(int note, int octave, int delay)
{
  // Note is # 1-12
  // Octave #0-6
  submit_sound(note_freq[note - 1] >> (6 - octave), delay);
}

void play_sfx(struct world *mzx_world, enum sfx_id sfxn)
{
  if(sfxn < NUM_SFX)
  {
    if(mzx_world->custom_sfx_on)
    {
      play_string(mzx_world->custom_sfx + (sfxn * SFX_SIZE), 1);
    }
    else
    {
      play_string(sfx_strs[sfxn], 1);
    }
  }
}

// sfx_play = 1 to NOT play non-digi unless queue empty

void play_string(char *str, int sfx_play)
{
  int t1, oct = 3, note = 1, dur = 18, t2, last_note = -1,
   digi_st = -1, digi_end = -1, digi_played = 0;
  char chr;
  // Note trans. table from 1-7 (a-z) to 1-12
  char nn[7] = { 10, 12, 1, 3, 5, 6, 8 };

  if(!audio_get_pcs_on())
  {
    sfx_play = 1; // SFX off
  }
  else
  {
    if(!sound_in_queue)
      sfx_play = 0;
  }

  // Now, if sfx_play, only play digi
  for(t1 = 0; (unsigned int)t1 < strlen((char *)str); t1++)
  {
    chr = str[t1];
    if((chr >= 'a') && (chr <= 'z'))
      chr -= 32;

    if(chr == '-')
    {
      // Octave down
      if(oct > 0) oct--;
      continue;
    }
    if(chr == '+')
    {
      // Octave up
      if(oct < 6) oct++;
      continue;
    }
    if(chr == 'X')
    {
      // Rest
      if(!sfx_play && !(digi_st > 0))
        submit_sound(F_REST,dur);

      continue;
    }
    if((chr > 47) && (chr < 55))
      oct = chr - 48; // Set octave

    if(chr == '.')
      dur += (dur >> 1); // Dot (duration 150%)

    if(chr == '!')
      dur /= 3; // Triplet (cut duration to 33%)

    if((chr > 64) && (chr < 72))
    {
      // Note
      note = nn[chr - 65]; // Convert to 1-12
      t2 = oct; // Save old octave in case # or $ changes it
      if(str[t1 + 1] == '#')
      {
        // Increase if sharp
        t1++;
        if(++note == 13)
        {
          // "B#" := "+C-"
          note = 1;
          if(++oct == 7) oct = 6;
        }
      }
      else

      if(str[t1 + 1] == '$')
      {
        // Decrease if flat
        t1++;
        if(--note == 0)
        {
          // "C$" := "-B+"
          note = 12;
          if(--oct == -1) oct = 0;
        }
      }
      // Digi
      if(digi_st > 0)
      {
        if(audio_get_music_on())
        {
          str[digi_end] = 0;

          audio_play_sample(str + digi_st, true, sam_freq[note - 1] >> oct);

          str[digi_end] = close_char;
          digi_played = 1;
          continue;
        }
      }
      else

      if(sfx_play)
      {
        continue;
      }
      else

      if(last_note == note)
      {
        if(dur <= 9)
        {
          submit_sound(F_REST, 1);
          play_note(note, oct, dur - 1);
        }
        else
        {
          submit_sound(F_REST, 2);
          play_note(note, oct, dur - 2);
        }
      }
      else
      {
        play_note(note, oct, dur);
        last_note = note;
      }
      oct = t2; // Restore old octave
    }

    switch(chr)
    {
      case 'Z':
        dur = 9;
        break;

      case 'T':
        dur = 18;
        break;

      case 'S':
        dur = 36;
        break;

      case 'I':
        dur = 72;
        break;

      case 'Q':
        dur = 144;
        break;

      case 'H':
        dur = 288;
        break;

      case 'W':
        dur = 576;
        break;
    }

    if(chr == open_char)
    {
      // Digitized
      t1++;
      digi_st = t1;

      if(str[t1] == 0)
        break;

      do
      {
        t1++;
        if(str[t1] == 0)
          break;

        if(str[t1] == close_char)
          break;

      } while(1);

      digi_end = t1;
      digi_played = 0;
    }

    if(chr == '_')
    {
      if(audio_get_music_on())
        break;

      digi_st = -1;
    }
  }

  // Pending digital music?
  if((digi_st > 0) && (digi_played == 0))
  {
    str[digi_end] = 0;

    audio_play_sample(str + digi_st, true, 0);

    str[digi_end] = close_char;
  }
}

void sfx_clear_queue(void)
{
  backindex = topindex = sound_in_queue = 0; // queue pointers
}

// Called by audio_pcs.c under lock.
void sfx_next_note(int *is_playing, int *freq, int *duration)
{
  int sfx_on = audio_get_pcs_on();

  if(sound_in_queue)
  {
    // Current sound finished, more?
    if(background[backindex].freq == F_REST)
    {
      if(sfx_on)
      {
        *is_playing = false;
        *duration = background[backindex].duration + 1; // freq == 1 for rest
      }
    }
    else

    if(sfx_on)
    {
      // Start sound
      *is_playing = true;
      *freq = background[backindex].freq;
      *duration = background[backindex].duration + 1;
    }

    backindex++;
    if(backindex == NOISEMAX)
      backindex = 0; // Wraparound
    if(backindex == topindex) // If last sound reset queue pointers
      backindex = topindex = sound_in_queue = 0;
  }
  else
  {
    if(sfx_on)
    {
      *is_playing = false;
      *duration = 1;
    }
  }

  // Silence the emulated PC speaker when sounds are disabled.
  // NOTE: there was not a corresponding nosound() call here in DOS versions.
  if(!sfx_on)
  {
    *is_playing = false;
    *duration = 1;
  }
}

char sfx_is_playing(void)
{
  return sound_in_queue;
}

int sfx_length_left(void)
{
  int left = topindex - backindex;

  if(left < 0)
    left = topindex + (NOISEMAX - backindex);

  return left;
}

#endif // CONFIG_AUDIO
