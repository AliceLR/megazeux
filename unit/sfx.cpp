/* MegaZeux
 *
 * Copyright (C) 2023-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "Unit.hpp"
#include "../src/configure.h"
#include "../src/sfx.h"
#include "../src/world.h"
#include "../src/audio/audio.h"
#include "../src/audio/audio_sfx.h"
#include "../src/audio/audio_struct.h"

#include <ctype.h>
#include <functional>

#ifdef CONFIG_DEBYTECODE
#define SAM_S "("
#define SAM_E ")"
#else
#define SAM_S "&"
#define SAM_E "&"
#endif

struct auto_free
{
  struct sfx_list &sfx_list;
  auto_free(struct sfx_list &s): sfx_list(s) {}
  ~auto_free()
  {
    sfx_free(&sfx_list);
  }
};

struct input_data
{
  int index;
  char label[16];
  const char *string;
};

struct io_data
{
  char label_in[16];
  const char *string_in;
  const char *label_out;
  const char *string_out;
};

UNITTEST(GetSet)
{
  static constexpr io_data data[] =
  {
    { "",         "",             nullptr,      nullptr },
    { "Gem",      "5c-gec-gec",   "Gem",        "5c-gec-gec" },
    { "EmptyStr", "",             "EmptyStr",   nullptr },
    { "",         "EmptyLabel",   nullptr,      "EmptyLabel" },
    { "TooLongLabel", "abc",      "TooLongLabe","abc", },
    // No SFX conversions are performed by the core SFX functions.
    { "duhhh",    "&lmao.sam&3c", "duhhh",      "&lmao.sam&3c", },
    { "a",        "&a.sam&3c_abc","a",          "&a.sam&3c_abc", },
  };
  static constexpr input_data invalid[] =
  {
    { -1, "negative", "negative", },
    { -1000000000, "negative2", "negative2", },
    { MAX_NUM_SFX, "past max", "past max", },
    { 1<<30, "large", "large", },
  };

  struct sfx_list sfx_list{};
  auto_free a(sfx_list);
  boolean ret;
  int i = 0;

  SECTION(SetString)
  {
    for(auto &d : data)
    {
      ret = sfx_set_string(&sfx_list, i, d.string_in, strlen(d.string_in));
      ASSERT(ret, "%d: '%s'", i, d.string_in);
      if(d.string_out)
      {
        ASSERT(sfx_list.list[i], "%d: '%s'", i, d.string_in);
        ASSERTCMP(sfx_list.list[i]->string, d.string_out, "%d", i);
      }
      else
        ASSERT(!sfx_list.list || !sfx_list.list[i], "%d should be null", i);

      i++;
    }
  }

  SECTION(SetLabel)
  {
    for(auto &d : data)
    {
      ret = sfx_set_label(&sfx_list, i, d.label_in, strlen(d.label_in));
      ASSERT(ret, "%d: '%s'", i, d.label_in);
      if(d.label_out)
      {
        ASSERT(sfx_list.list[i], "%d: '%s'", i, d.label_in);
        ASSERTCMP(sfx_list.list[i]->label, d.label_out, "%d", i);
        ASSERTCMP(sfx_list.list[i]->string, "", "%d", i);
      }
      // Setting the string should retain the label.
      ret = sfx_set_string(&sfx_list, i, d.string_in, strlen(d.string_in));
      ASSERT(ret, "%d: '%s'", i, d.label_in);
      if(d.label_out || d.string_out)
      {
        ASSERT(sfx_list.list[i], "%d: '%s'", i, d.label_in);
        ASSERTCMP(sfx_list.list[i]->label, d.label_out ? d.label_out : "", "%d", i);
        ASSERTCMP(sfx_list.list[i]->string, d.string_out ? d.string_out : "", "%d", i);
      }
      else
        ASSERT(!sfx_list.list || !sfx_list.list[i], "%d: '%s'", i, d.label_in);

      i++;
    }
  }

  SECTION(Get)
  {
    for(auto &d : data)
    {
      ret = sfx_set_label(&sfx_list, i, d.label_in, strlen(d.label_in));
      ASSERT(ret, "%d: '%s'", i, d.label_in);
      ret = sfx_set_string(&sfx_list, i, d.string_in, strlen(d.string_in));
      ASSERT(ret, "%d: '%s'", i, d.label_in);

      if(sfx_list.list)
      {
        auto *p = sfx_get(&sfx_list, i);
        ASSERTEQ(sfx_list.list[i], p, "%d: '%s'", i, d.label_in);
      }
      i++;
    }
  }

  SECTION(Unset)
  {
    // Only set strings.
    for(i = 0; i < 32; i++)
    {
      ret = sfx_set_string(&sfx_list, i, "abc", 3);
      ASSERT(ret, "%d", i);
      ASSERT(sfx_list.list, "%d", i);
      ASSERT(sfx_list.list[i], "%d", i);
    }
    // Only set labels.
    for(; i < 64; i++)
    {
      ret = sfx_set_label(&sfx_list, i, "def", 3);
      ASSERT(ret, "%d", i);
      ASSERT(sfx_list.list, "%d", i);
      ASSERT(sfx_list.list[i], "%d", i);
    }
    // Should unset either...
    for(i = 0; i < 64; i++)
    {
      sfx_unset(&sfx_list, i);
      ASSERT(!sfx_list.list[i], "%d", i);
    }
  }

  SECTION(Invalid)
  {
    auto *n = sfx_get(&sfx_list, 0);
    ASSERT(!n, "get from NULL sfx list");

    for(auto &d : invalid)
    {
      ret = sfx_set_string(&sfx_list, d.index, d.string, strlen(d.string));
      ASSERTEQ(ret, 0, "%d: '%s'", d.index, d.string);
      ret = sfx_set_label(&sfx_list, d.index, d.label, strlen(d.label));
      ASSERTEQ(ret, 0, "%d: '%s'", d.index, d.string);
      auto *p = sfx_get(&sfx_list, d.index);
      ASSERT(!p, "%d: '%s'", d.index, d.string);
    }
  }

  sfx_free(&sfx_list);
  ASSERT(!sfx_list.list, "");
  ASSERTEQ(sfx_list.num_alloc, 0, "");
}


static constexpr io_data save_data[] =
{
  { "0",  "abc",              "0",  "abc", },
  { "1",  "def",              "1",  "def", },
  { "2",  "123515",           "2",  "123515", },
  { "3",  "!!zc",             "3",  "!!zc", },
  { "4",  "[[[%%",            "4",  "[[[%%", },
  // SFX conversions are performed by the loading functions.
  { "5",  "&shot.sam&3c",     "5",  SAM_S "shot.sam" SAM_E "3c", },
  { "6",  SAM_S "a.sam" SAM_E,"6",  SAM_S "a.sam" SAM_E, },
  { "7",  "&a.sam&3c&a.sam&", "7",  SAM_S "a.sam" SAM_E "3c" SAM_S "a.sam" SAM_E, },
  { "8",  "&a.sam&3c_abc",    "8",  SAM_S "a.sam" SAM_E "3c_abc", },
  { "9",  "(((())(()",        "9",  "(((())(()", },
  { "10", "&&&&&&&",          "10", SAM_S SAM_E SAM_S SAM_E SAM_S SAM_E "&", },
  // Don't save empty fields...
  { "11", "",                 "11", "", },
  { "",   "hhhhhhh",          "",   "hhhhhhh", },
  { "",   "",                 "",   "", },
  // The rest should be omitted entirely.
};
static constexpr size_t save_data_sfx_length = 92u;

static constexpr char v251_data[NUM_BUILTIN_SFX][LEGACY_SFX_SIZE] =
{
  "abc",
  "def",
  "123515",
  "!!zc",
  "[[[%%",
  "&shot.sam&3c",
  SAM_S "a.sam" SAM_E,
  "&a.sam&3c&a.sam&",
  "&a.sam&3c_abc",
  "(((())(()",
  "&&&&&&&",
  "",
  "hhhhhhh",
  "",
};
static constexpr size_t v251_num_loaded = 12u;
static constexpr size_t v251_loaded_size =
 sizeof(struct custom_sfx *) * NUM_BUILTIN_SFX +
 sizeof(struct custom_sfx) * v251_num_loaded + save_data_sfx_length;

static constexpr char v293_data[] =
{
  "MZFX\x1a\x00\x02\x5d"
  "\x01\x00\x01\x00\x00\x00\x00"
  "\x02\x00\x03\x00\x00\x00" "abc"
  "\x03\x00\x01\x00\x00\x00" "0"
  "\x01\x00\x01\x00\x00\x00\x01"
  "\x02\x00\x03\x00\x00\x00" "def"
  "\x03\x00\x01\x00\x00\x00" "1"
  "\x01\x00\x01\x00\x00\x00\x02"
  "\x02\x00\x06\x00\x00\x00" "123515"
  "\x03\x00\x01\x00\x00\x00" "2"
  "\x01\x00\x01\x00\x00\x00\x03"
  "\x02\x00\x04\x00\x00\x00" "!!zc"
  "\x03\x00\x01\x00\x00\x00" "3"
  "\x01\x00\x01\x00\x00\x00\x04"
  "\x02\x00\x05\x00\x00\x00" "[[[%%"
  "\x03\x00\x01\x00\x00\x00" "4"
  "\x01\x00\x01\x00\x00\x00\x05"
  "\x02\x00\x0c\x00\x00\x00" "&shot.sam&3c"
  "\x03\x00\x01\x00\x00\x00" "5"
  "\x01\x00\x01\x00\x00\x00\x06"
  "\x02\x00\x07\x00\x00\x00" SAM_S "a.sam" SAM_E
  "\x03\x00\x01\x00\x00\x00" "6"
  "\x01\x00\x01\x00\x00\x00\x07"
  "\x02\x00\x10\x00\x00\x00" "&a.sam&3c&a.sam&"
  "\x03\x00\x01\x00\x00\x00" "7"
  "\x01\x00\x01\x00\x00\x00\x08"
  "\x02\x00\x0d\x00\x00\x00" "&a.sam&3c_abc"
  "\x03\x00\x01\x00\x00\x00" "8"
  "\x01\x00\x01\x00\x00\x00\x09"
  "\x02\x00\x09\x00\x00\x00" "(((())(()"
  "\x03\x00\x01\x00\x00\x00" "9"
  "\x01\x00\x01\x00\x00\x00\x0a"
  "\x02\x00\x07\x00\x00\x00" "&&&&&&&"
  "\x03\x00\x02\x00\x00\x00" "10"
  "\x01\x00\x01\x00\x00\x00\x0b"
  "\x03\x00\x02\x00\x00\x00" "11"
  "\x01\x00\x01\x00\x00\x00\x0c"
  "\x02\x00\x07\x00\x00\x00" "hhhhhhh"
  "\x00" // + nul completes terminator
};
static constexpr size_t v293_num_loaded = 13u; /* 11 label-only, is loaded now */
static constexpr size_t v293_loaded_size =
 sizeof(struct custom_sfx *) * NUM_BUILTIN_SFX +
 sizeof(struct custom_sfx) * v293_num_loaded + save_data_sfx_length;

#ifdef CONFIG_DEBYTECODE
static constexpr char v300_data[] =
{
  "MZFX\x1a\x00\x03\x00"
  "\x01\x00\x01\x00\x00\x00\x00"
  "\x02\x00\x03\x00\x00\x00" "abc"
  "\x03\x00\x01\x00\x00\x00" "0"
  "\x01\x00\x01\x00\x00\x00\x01"
  "\x02\x00\x03\x00\x00\x00" "def"
  "\x03\x00\x01\x00\x00\x00" "1"
  "\x01\x00\x01\x00\x00\x00\x02"
  "\x02\x00\x06\x00\x00\x00" "123515"
  "\x03\x00\x01\x00\x00\x00" "2"
  "\x01\x00\x01\x00\x00\x00\x03"
  "\x02\x00\x04\x00\x00\x00" "!!zc"
  "\x03\x00\x01\x00\x00\x00" "3"
  "\x01\x00\x01\x00\x00\x00\x04"
  "\x02\x00\x05\x00\x00\x00" "[[[%%"
  "\x03\x00\x01\x00\x00\x00" "4"
  "\x01\x00\x01\x00\x00\x00\x05"
  "\x02\x00\x0c\x00\x00\x00" SAM_S "shot.sam" SAM_E "3c"
  "\x03\x00\x01\x00\x00\x00" "5"
  "\x01\x00\x01\x00\x00\x00\x06"
  "\x02\x00\x07\x00\x00\x00" SAM_S "a.sam" SAM_E
  "\x03\x00\x01\x00\x00\x00" "6"
  "\x01\x00\x01\x00\x00\x00\x07"
  "\x02\x00\x10\x00\x00\x00" SAM_S "a.sam" SAM_E "3c" SAM_S "a.sam" SAM_E
  "\x03\x00\x01\x00\x00\x00" "7"
  "\x01\x00\x01\x00\x00\x00\x08"
  "\x02\x00\x0d\x00\x00\x00" SAM_S "a.sam" SAM_E "3c_abc"
  "\x03\x00\x01\x00\x00\x00" "8"
  "\x01\x00\x01\x00\x00\x00\x09"
  "\x02\x00\x09\x00\x00\x00" "(((())(()"
  "\x03\x00\x01\x00\x00\x00" "9"
  "\x01\x00\x01\x00\x00\x00\x0a"
  "\x02\x00\x07\x00\x00\x00" SAM_S SAM_E SAM_S SAM_E SAM_S SAM_E "&"
  "\x03\x00\x02\x00\x00\x00" "10"
  "\x01\x00\x01\x00\x00\x00\x0b"
  "\x03\x00\x02\x00\x00\x00" "11"
  "\x01\x00\x01\x00\x00\x00\x0c"
  "\x02\x00\x07\x00\x00\x00" "hhhhhhh"
  "\x00" // + nul completes terminator
};
#endif

UNITTEST(SaveSFX)
{
  struct sfx_list sfx_list{};
  auto_free a(sfx_list);
  char buffer[NUM_BUILTIN_SFX * LEGACY_SFX_SIZE];
  size_t required = 0;
  size_t sz;
  boolean ret;
  int i = 0;

  for(auto &d : save_data)
  {
    ret = sfx_set_label(&sfx_list, i, d.label_out, strlen(d.label_out));
    ASSERT(ret, "%d", i);
    ret = sfx_set_string(&sfx_list, i, d.string_out, strlen(d.string_out));
    ASSERT(ret, "%d", i);
    i++;
  }

// FIXME: 3.x should either back-convert the SFX string or drop this feature entirely.
#ifndef CONFIG_DEBYTECODE
  SECTION(Array)
  {
    sz = sfx_save_to_memory(&sfx_list, V251, buffer, sizeof(buffer), nullptr);
    ASSERTEQ(sz, sizeof(v251_data), "");
    ASSERTMEM(buffer, v251_data, sizeof(buffer), "");
  }

  SECTION(Properties293)
  {
    sz = sfx_save_to_memory(&sfx_list, V293, nullptr, 0, &required);
    ASSERTEQ(sz, 0, "");
    ASSERTEQ(required, sizeof(v293_data), "");
    sz = sfx_save_to_memory(&sfx_list, V293, buffer, required, nullptr);
    ASSERTEQ(sz, required, "");
    ASSERTMEM(buffer, v293_data, required, "");
  }
#endif

#ifdef CONFIG_DEBYTECODE
  SECTION(Properties300)
  {
    sz = sfx_save_to_memory(&sfx_list, V300, nullptr, 0, &required);
    ASSERTEQ(sz, 0, "");
    ASSERTEQ(required, sizeof(v300_data), "");
    sz = sfx_save_to_memory(&sfx_list, V300, buffer, required, nullptr);
    ASSERTEQ(sz, required, "");
    ASSERTMEM(buffer, v300_data, required, "");
  }
#endif
}

template<int N>
static void compare_load(const struct sfx_list *sfx_list, const io_data (&data)[N], boolean skip_labels = false)
{
  int i = 0;
  for(auto &d : data)
  {
    const struct custom_sfx *sfx = sfx_get(sfx_list, i);
    size_t a = strlen(d.string_out);
    size_t b = strlen(d.label_out);

    if(a || (b && !skip_labels))
    {
      ASSERT(sfx, "%d", i);
      ASSERTCMP(sfx->string, d.string_out, "%d", i);
      if(!skip_labels)
        ASSERTCMP(sfx->label, d.label_out, "%d", i);
    }
    else
      ASSERTEQ(sfx, nullptr, "%d", i);

    i++;
  }
}

UNITTEST(LoadSFX)
{
  struct sfx_list sfx_list{};
  auto_free a(sfx_list);
  boolean ret;

  SECTION(Array)
  {
    ret = sfx_load_from_memory(&sfx_list, V251, (const char *)v251_data, sizeof(v251_data));
    ASSERT(ret, "");
    compare_load(&sfx_list, save_data, true);
  }

  SECTION(Properties293)
  {
    ret = sfx_load_from_memory(&sfx_list, V293, v293_data, sizeof(v293_data));
    ASSERT(ret, "");
    compare_load(&sfx_list, save_data);
  }

#ifdef CONFIG_DEBYTECODE
  SECTION(Properties300)
  {
    ret = sfx_load_from_memory(&sfx_list, V300, v300_data, sizeof(v300_data));
    ASSERT(ret, "");
    compare_load(&sfx_list, save_data);
  }
#endif
}

UNITTEST(sfx_ram_usage)
{
#ifndef CONFIG_EDITOR
  SKIP();
#else /* CONFIG_EDITOR */
  struct sfx_list sfx_list{};
  auto_free a(sfx_list);
  boolean ret;

  SECTION(Array)
  {
    ret = sfx_load_from_memory(&sfx_list, V251, (const char *)v251_data, sizeof(v251_data));
    ASSERT(ret, "");
    ASSERTEQ(sfx_ram_usage(&sfx_list), v251_loaded_size, "");
  }

  SECTION(Properties293)
  {
    ret = sfx_load_from_memory(&sfx_list, V293, v293_data, sizeof(v293_data));
    ASSERT(ret, "");
    ASSERTEQ(sfx_ram_usage(&sfx_list), v293_loaded_size, "");
  }

#ifdef CONFIG_DEBYTECODE
  SECTION(Properties300)
  {
    ret = sfx_load_from_memory(&sfx_list, V300, v300_data, sizeof(v300_data));
    ASSERT(ret, "");
    ASSERTEQ(sfx_ram_usage(&sfx_list), v293_loaded_size, "");
  }
#endif
#endif /* CONFIG_EDITOR */
}


static struct config_info conf{};

class sfx_play_test_setup
{
public:
  sfx_play_test_setup()
  {
    /* These tests require the audio system to be capable of receiving notes.
     * Setting up the audio system requires audio to be enabled. */
#ifndef CONFIG_AUDIO
    SKIP();
#endif
    memset(&conf, 0, sizeof(struct config_info));
    snprintf(conf.audio_driver, sizeof(conf.audio_driver), "none");
    conf.music_volume = 8;
    conf.sam_volume = 8;
    conf.pc_speaker_volume = 8;
    conf.music_on = true;
    conf.pc_speaker_on = true;
    init_audio(&conf);
  }

  ~sfx_play_test_setup()
  {
    quit_audio();
  }

  void reinit(boolean music_on, boolean pc_speaker_on)
  {
    quit_audio();
    conf.music_on = music_on;
    conf.pc_speaker_on = pc_speaker_on;
    init_audio(&conf);
  }
};

struct test_note
{
  uint16_t freq; /* A428 */
  uint16_t duration_classic; /* +1 duration bug emulation */
  uint16_t duration_fixed;
};

struct test_sample
{
  const char *filename;
  uint16_t period;
};

struct play_test_data
{
  const char *str;
  size_t num_notes;
  struct test_note notes[50];
  struct test_sample samples[4];
};

static constexpr struct play_test_data play_notes[] =
{
  { "",       0,  {}, {}},
  { " ",      0,  {}, {}},
  { "++",     0,  {}, {}},
  { "-----",  0,  {}, {}},
  { "z",      0,  {}, {}},
  { "zx",     1,  {{ AUDIO_SFX_FREQ_REST, 10, 9 }}, {}},
  { "zX",     1,  {{ AUDIO_SFX_FREQ_REST, 10, 9 }}, {}},
  { "z---x",  1,  {{ AUDIO_SFX_FREQ_REST, 10, 9 }}, {}},
  { "z+++x",  1,  {{ AUDIO_SFX_FREQ_REST, 10, 9 }}, {}},
  { "z+++x#", 1,  {{ AUDIO_SFX_FREQ_REST, 10, 9 }}, {}},
  { "z+++x$", 1,  {{ AUDIO_SFX_FREQ_REST, 10, 9 }}, {}},
  /* Absolute octaves */
  { "z0c",    1,  {{ 31, 10, 9 }}, {}},
  { "z1c",    1,  {{ 63, 10, 9 }}, {}},
  { "z2c",    1,  {{ 127, 10, 9 }}, {}},
  { "z3c",    1,  {{ 254, 10, 9 }}, {}},
  { "z4c",    1,  {{ 508, 10, 9 }}, {}},
  { "z5c",    1,  {{ 1016, 10, 9 }}, {}},
  { "z6c",    1,  {{ 2032, 10, 9 }}, {}},
  /* Relative octaves */
  { "zc",     1,  {{ 254, 10, 9 }}, {}}, /* Default octave is 3 */
  { "z----c", 1,  {{ 31, 10, 9 }}, {}}, /* Can't decrease octave below 0 */
  { "z---c",  1,  {{ 31, 10, 9 }}, {}}, /* Normal note */
  { "z---#c", 1,  {{ 31, 10, 9 }}, {}}, /* Accidentals can not precede */
  { "z---$c", 1,  {{ 31, 10, 9 }}, {}},
  { "z---c #",1,  {{ 31, 10, 9 }}, {}}, /* Accidentals must immediately follow */
  { "z---c $",1,  {{ 31, 10, 9 }}, {}},
  { "z---c#", 1,  {{ 33, 10, 9 }}, {}}, /* Normal note */
  { "z---c$", 1,  {{ 60, 10, 9 }}, {}}, /* Gets wrapped on octave 0 to B-flat 0 */
  { "z---c##",1,  {{ 33, 10, 9 }}, {}}, /* No double-sharp or double-flat */
  { "z---c$$",1,  {{ 60, 10, 9 }}, {}},
  /* Octave 0 */
  { "z---d$", 1,  {{ 33, 10, 9 }}, {}},
  { "z---d",  1,  {{ 35, 10, 9 }}, {}},
  { "z---d#", 1,  {{ 37, 10, 9 }}, {}},
  { "z---e$", 1,  {{ 37, 10, 9 }}, {}},
  { "z---e",  1,  {{ 40, 10, 9 }}, {}},
  { "z---f$", 1,  {{ 40, 10, 9 }}, {}},
  { "z---e#", 1,  {{ 42, 10, 9 }}, {}},
  { "z---f",  1,  {{ 42, 10, 9 }}, {}},
  { "z---f#", 1,  {{ 45, 10, 9 }}, {}},
  { "z---g$", 1,  {{ 45, 10, 9 }}, {}},
  { "z---g",  1,  {{ 47, 10, 9 }}, {}},
  { "z---g#", 1,  {{ 50, 10, 9 }}, {}},
  { "z---a$", 1,  {{ 50, 10, 9 }}, {}},
  { "z---a",  1,  {{ 53, 10, 9 }}, {}},
  { "z---a#", 1,  {{ 56, 10, 9 }}, {}},
  { "z---b$", 1,  {{ 56, 10, 9 }}, {}},
  { "z---b",  1,  {{ 60, 10, 9 }}, {}},
  { "z---b#", 1,  {{ 63, 10, 9 }}, {}},
  /* Octave 1 */
  { "z--c$",  1,  {{ 60, 10, 9 }}, {}},
  { "z--c",   1,  {{ 63, 10, 9 }}, {}},
  { "z--c#",  1,  {{ 67, 10, 9 }}, {}},
  { "z--d$",  1,  {{ 67, 10, 9 }}, {}},
  { "z--d",   1,  {{ 71, 10, 9 }}, {}},
  { "z--d#",  1,  {{ 75, 10, 9 }}, {}},
  { "z--e$",  1,  {{ 75, 10, 9 }}, {}},
  { "z--e",   1,  {{ 80, 10, 9 }}, {}},
  { "z--f$",  1,  {{ 80, 10, 9 }}, {}},
  { "z--e#",  1,  {{ 84, 10, 9 }}, {}},
  { "z--f",   1,  {{ 84, 10, 9 }}, {}},
  { "z--f#",  1,  {{ 90, 10, 9 }}, {}},
  { "z--g$",  1,  {{ 90, 10, 9 }}, {}},
  { "z--g",   1,  {{ 95, 10, 9 }}, {}},
  { "z--g#",  1,  {{ 101, 10, 9 }}, {}},
  { "z--a$",  1,  {{ 101, 10, 9 }}, {}},
  { "z--a",   1,  {{ 107, 10, 9 }}, {}},
  { "z--a#",  1,  {{ 113, 10, 9 }}, {}},
  { "z--b$",  1,  {{ 113, 10, 9 }}, {}},
  { "z--b",   1,  {{ 120, 10, 9 }}, {}},
  { "z--b#",  1,  {{ 127, 10, 9 }}, {}},
  /* Octave 2 */
  { "z-c$",   1,  {{ 120, 10, 9 }}, {}},
  { "z-c",    1,  {{ 127, 10, 9 }}, {}},
  { "z-c#",   1,  {{ 134, 10, 9 }}, {}},
  { "z-d$",   1,  {{ 134, 10, 9 }}, {}},
  { "z-d",    1,  {{ 142, 10, 9 }}, {}},
  { "z-d#",   1,  {{ 151, 10, 9 }}, {}},
  { "z-e$",   1,  {{ 151, 10, 9 }}, {}},
  { "z-e",    1,  {{ 160, 10, 9 }}, {}},
  { "z-f$",   1,  {{ 160, 10, 9 }}, {}},
  { "z-e#",   1,  {{ 169, 10, 9 }}, {}},
  { "z-f",    1,  {{ 169, 10, 9 }}, {}},
  { "z-f#",   1,  {{ 180, 10, 9 }}, {}},
  { "z-g$",   1,  {{ 180, 10, 9 }}, {}},
  { "z-g",    1,  {{ 190, 10, 9 }}, {}},
  { "z-g#",   1,  {{ 202, 10, 9 }}, {}},
  { "z-a$",   1,  {{ 202, 10, 9 }}, {}},
  { "z-a",    1,  {{ 214, 10, 9 }}, {}},
  { "z-a#",   1,  {{ 226, 10, 9 }}, {}},
  { "z-b$",   1,  {{ 226, 10, 9 }}, {}},
  { "z-b",    1,  {{ 240, 10, 9 }}, {}},
  { "z-b#",   1,  {{ 254, 10, 9 }}, {}},
  /* Octave 3 */
  { "zc$",    1,  {{ 240, 10, 9 }}, {}},
  { "zc",     1,  {{ 254, 10, 9 }}, {}},
  { "zc#",    1,  {{ 269, 10, 9 }}, {}},
  { "zd$",    1,  {{ 269, 10, 9 }}, {}},
  { "zd",     1,  {{ 285, 10, 9 }}, {}},
  { "zd#",    1,  {{ 302, 10, 9 }}, {}},
  { "ze$",    1,  {{ 302, 10, 9 }}, {}},
  { "ze",     1,  {{ 320, 10, 9 }}, {}},
  { "zf$",    1,  {{ 320, 10, 9 }}, {}},
  { "ze#",    1,  {{ 339, 10, 9 }}, {}},
  { "zf",     1,  {{ 339, 10, 9 }}, {}},
  { "zf#",    1,  {{ 360, 10, 9 }}, {}},
  { "zg$",    1,  {{ 360, 10, 9 }}, {}},
  { "zg",     1,  {{ 381, 10, 9 }}, {}},
  { "zg#",    1,  {{ 404, 10, 9 }}, {}},
  { "za$",    1,  {{ 404, 10, 9 }}, {}},
  { "za",     1,  {{ 428, 10, 9 }}, {}},
  { "za#",    1,  {{ 453, 10, 9 }}, {}},
  { "zb$",    1,  {{ 453, 10, 9 }}, {}},
  { "zb",     1,  {{ 480, 10, 9 }}, {}},
  { "zb#",    1,  {{ 508, 10, 9 }}, {}},
  /* Octave 4 */
  { "z+c$",   1,  {{ 480, 10, 9 }}, {}},
  { "z+c",    1,  {{ 508, 10, 9 }}, {}},
  { "z+c#",   1,  {{ 538, 10, 9 }}, {}},
  { "z+d$",   1,  {{ 538, 10, 9 }}, {}},
  { "z+d",    1,  {{ 570, 10, 9 }}, {}},
  { "z+d#",   1,  {{ 604, 10, 9 }}, {}},
  { "z+e$",   1,  {{ 604, 10, 9 }}, {}},
  { "z+e",    1,  {{ 640, 10, 9 }}, {}},
  { "z+f$",   1,  {{ 640, 10, 9 }}, {}},
  { "z+e#",   1,  {{ 678, 10, 9 }}, {}},
  { "z+f",    1,  {{ 678, 10, 9 }}, {}},
  { "z+f#",   1,  {{ 720, 10, 9 }}, {}},
  { "z+g$",   1,  {{ 720, 10, 9 }}, {}},
  { "z+g",    1,  {{ 762, 10, 9 }}, {}},
  { "z+g#",   1,  {{ 808, 10, 9 }}, {}},
  { "z+a$",   1,  {{ 808, 10, 9 }}, {}},
  { "z+a",    1,  {{ 856, 10, 9 }}, {}},
  { "z+a#",   1,  {{ 906, 10, 9 }}, {}},
  { "z+b$",   1,  {{ 906, 10, 9 }}, {}},
  { "z+b",    1,  {{ 960, 10, 9 }}, {}},
  { "z+b#",   1,  {{ 1016, 10, 9 }}, {}},
  /* Octave 5 */
  { "z++c$",  1,  {{ 960, 10, 9 }}, {}},
  { "z++c",   1,  {{ 1016, 10, 9 }}, {}},
  { "z++c#",  1,  {{ 1076, 10, 9 }}, {}},
  { "z++d$",  1,  {{ 1076, 10, 9 }}, {}},
  { "z++d",   1,  {{ 1140, 10, 9 }}, {}},
  { "z++d#",  1,  {{ 1208, 10, 9 }}, {}},
  { "z++e$",  1,  {{ 1208, 10, 9 }}, {}},
  { "z++e",   1,  {{ 1280, 10, 9 }}, {}},
  { "z++f$",  1,  {{ 1280, 10, 9 }}, {}},
  { "z++e#",  1,  {{ 1356, 10, 9 }}, {}},
  { "z++f",   1,  {{ 1356, 10, 9 }}, {}},
  { "z++f#",  1,  {{ 1440, 10, 9 }}, {}},
  { "z++g$",  1,  {{ 1440, 10, 9 }}, {}},
  { "z++g",   1,  {{ 1524, 10, 9 }}, {}},
  { "z++g#",  1,  {{ 1616, 10, 9 }}, {}},
  { "z++a$",  1,  {{ 1616, 10, 9 }}, {}},
  { "z++a",   1,  {{ 1712, 10, 9 }}, {}},
  { "z++a#",  1,  {{ 1812, 10, 9 }}, {}},
  { "z++b$",  1,  {{ 1812, 10, 9 }}, {}},
  { "z++b",   1,  {{ 1920, 10, 9 }}, {}},
  { "z++b#",  1,  {{ 2032, 10, 9 }}, {}},
  /* Octave 6 */
  { "z+++c$", 1,  {{ 1920, 10, 9 }}, {}},
  { "z+++c",  1,  {{ 2032, 10, 9 }}, {}},
  { "z+++c#", 1,  {{ 2152, 10, 9 }}, {}},
  { "z+++d$", 1,  {{ 2152, 10, 9 }}, {}},
  { "z+++d",  1,  {{ 2280, 10, 9 }}, {}},
  { "z+++d#", 1,  {{ 2416, 10, 9 }}, {}},
  { "z+++e$", 1,  {{ 2416, 10, 9 }}, {}},
  { "z+++e",  1,  {{ 2560, 10, 9 }}, {}},
  { "z+++f$", 1,  {{ 2560, 10, 9 }}, {}},
  { "z+++e#", 1,  {{ 2712, 10, 9 }}, {}},
  { "z+++f",  1,  {{ 2712, 10, 9 }}, {}},
  { "z+++f#", 1,  {{ 2880, 10, 9 }}, {}},
  { "z+++g$", 1,  {{ 2880, 10, 9 }}, {}},
  { "z+++g",  1,  {{ 3048, 10, 9 }}, {}},
  { "z+++g#", 1,  {{ 3232, 10, 9 }}, {}},
  { "z+++a$", 1,  {{ 3232, 10, 9 }}, {}},
  { "z+++a",  1,  {{ 3424, 10, 9 }}, {}},
  { "z+++a#", 1,  {{ 3624, 10, 9 }}, {}},
  { "z+++b$", 1,  {{ 3624, 10, 9 }}, {}},
  { "z+++b",  1,  {{ 3840, 10, 9 }}, {}},

  { "z+++b#", 1,  {{ 2032, 10, 9 }}, {}}, /* Gets wrapped on octave 6 to C6 */
  { "z++++c", 1,  {{ 2032, 10, 9 }}, {}}, /* Can't increase octave above 6 */

  /* Case-insensitive notes */
  { "zC$",    1,  {{ 240, 10, 9 }}, {}},
  { "zC",     1,  {{ 254, 10, 9 }}, {}},
  { "zC#",    1,  {{ 269, 10, 9 }}, {}},
  { "zD$",    1,  {{ 269, 10, 9 }}, {}},
  { "zD",     1,  {{ 285, 10, 9 }}, {}},
  { "zD#",    1,  {{ 302, 10, 9 }}, {}},
  { "zE$",    1,  {{ 302, 10, 9 }}, {}},
  { "zE",     1,  {{ 320, 10, 9 }}, {}},
  { "zF$",    1,  {{ 320, 10, 9 }}, {}},
  { "zE#",    1,  {{ 339, 10, 9 }}, {}},
  { "zF",     1,  {{ 339, 10, 9 }}, {}},
  { "zF#",    1,  {{ 360, 10, 9 }}, {}},
  { "zG$",    1,  {{ 360, 10, 9 }}, {}},
  { "zG",     1,  {{ 381, 10, 9 }}, {}},
  { "zG#",    1,  {{ 404, 10, 9 }}, {}},
  { "zA$",    1,  {{ 404, 10, 9 }}, {}},
  { "zA",     1,  {{ 428, 10, 9 }}, {}},
  { "zA#",    1,  {{ 453, 10, 9 }}, {}},
  { "zB$",    1,  {{ 453, 10, 9 }}, {}},
  { "zB",     1,  {{ 480, 10, 9 }}, {}},
  { "zB#",    1,  {{ 508, 10, 9 }}, {}},

  /* Combined absolute/relative */
  { "z---3c", 1,  {{ 254, 10, 9 }}, {}},  /* Absolute unaffected by preceding relative */
  { "z+++3c", 1,  {{ 254, 10, 9 }}, {}},
  { "z-5+c",  1,  {{ 2032, 10, 9 }}, {}}, /* 5+1 -> 6 */
  { "z+1-c",  1,  {{ 31, 10, 9 }}, {}},   /* 1-1 -> 0 */
};

static constexpr struct play_test_data play_durations[] =
{
  { "c",          1,  {{ 254, 19, 18 }}, {}},   /* Default duration is t */
  /* Unmodified durations */
  { "zc",         1,  {{ 254, 10, 9 }}, {}},
  { "tc",         1,  {{ 254, 19, 18 }}, {}},
  { "sc",         1,  {{ 254, 37, 36 }}, {}},
  { "ic",         1,  {{ 254, 73, 72 }}, {}},
  { "qc",         1,  {{ 254, 145, 144 }}, {}},
  { "hc",         1,  {{ 254, 289, 288 }}, {}},
  { "wc",         1,  {{ 254, 577, 576 }}, {}},
  /* Case-insensitive */
  { "Zc",         1,  {{ 254, 10, 9 }}, {}},
  { "Tc",         1,  {{ 254, 19, 18 }}, {}},
  { "Sc",         1,  {{ 254, 37, 36 }}, {}},
  { "Ic",         1,  {{ 254, 73, 72 }}, {}},
  { "Qc",         1,  {{ 254, 145, 144 }}, {}},
  { "Hc",         1,  {{ 254, 289, 288 }}, {}},
  { "Wc",         1,  {{ 254, 577, 576 }}, {}},
  /* Dotted variants (one past first truncation) */
  { "z.c",        1,  {{ 254, 14, 13 }}, {}},
  { "z..c",       1,  {{ 254, 20, 19 }}, {}},
  { ".c",         1,  {{ 254, 28, 27 }}, {}},
  { "t.c",        1,  {{ 254, 28, 27 }}, {}},
  { "t..c",       1,  {{ 254, 41, 40 }}, {}},
  { "t...c",      1,  {{ 254, 61, 60 }}, {}},
  { "s.c",        1,  {{ 254, 55, 54 }}, {}},
  { "s..c",       1,  {{ 254, 82, 81 }}, {}},
  { "s...c",      1,  {{ 254, 122, 121 }}, {}},
  { "s....c",     1,  {{ 254, 182, 181 }}, {}},
  { "i.c",        1,  {{ 254, 109, 108 }}, {}},
  { "i..c",       1,  {{ 254, 163, 162 }}, {}},
  { "i...c",      1,  {{ 254, 244, 243 }}, {}},
  { "i....c",     1,  {{ 254, 365, 364 }}, {}},
  { "i.....c",    1,  {{ 254, 547, 546 }}, {}},
  { "q.c",        1,  {{ 254, 217, 216 }}, {}},
  { "q..c",       1,  {{ 254, 325, 324 }}, {}},
  { "q...c",      1,  {{ 254, 487, 486 }}, {}},
  { "q....c",     1,  {{ 254, 730, 729 }}, {}},
  { "q.....c",    1,  {{ 254, 1094, 1093 }}, {}},
  { "q......c",   1,  {{ 254, 1640, 1639 }}, {}},
  { "h.c",        1,  {{ 254, 433, 432 }}, {}},
  { "h..c",       1,  {{ 254, 649, 648 }}, {}},
  { "h...c",      1,  {{ 254, 973, 972 }}, {}},
  { "h....c",     1,  {{ 254, 1459, 1458 }}, {}},
  { "h.....c",    1,  {{ 254, 2188, 2187 }}, {}},
  { "h......c",   1,  {{ 254, 3281, 3280 }}, {}},
  { "h.......c",  1,  {{ 254, 4921, 4920 }}, {}},
  { "w.c",        1,  {{ 254, 865, 864 }}, {}},
  { "w..c",       1,  {{ 254, 1297, 1296 }}, {}},
  { "w...c",      1,  {{ 254, 1945, 1944 }}, {}},
  { "w....c",     1,  {{ 254, 2917, 2916 }}, {}},
  { "w.....c",    1,  {{ 254, 4375, 4374 }}, {}},
  { "w......c",   1,  {{ 254, 6562, 6561 }}, {}},
  { "w.......c",  1,  {{ 254, 9842, 9841 }}, {}},
  { "w........c", 1,  {{ 254, 14762, 14761 }}, {}},

  /* Triplets (one past first truncation; minimum is 1 due to extra duration) */
  { "z!c",        1,  {{ 254, 4, 3 }}, {}},
  { "z!!c",       1,  {{ 254, 2, 1 }}, {}},
  { "z!!!c",      1,  {{ 254, 1, 1 }}, {}},
  { "z!!!!c",     1,  {{ 254, 1, 1 }}, {}},
  { "!c",         1,  {{ 254, 7, 6 }}, {}},
  { "t!c",        1,  {{ 254, 7, 6 }}, {}},
  { "t!!c",       1,  {{ 254, 3, 2 }}, {}},
  { "t!!!c",      1,  {{ 254, 1, 1 }}, {}},
  { "t!!!!c",     1,  {{ 254, 1, 1 }}, {}},
  { "s!c",        1,  {{ 254, 13, 12 }}, {}},
  { "s!!c",       1,  {{ 254, 5, 4 }}, {}},
  { "s!!!c",      1,  {{ 254, 2, 1 }}, {}},
  { "s!!!!c",     1,  {{ 254, 1, 1 }}, {}},
  { "i!c",        1,  {{ 254, 25, 24 }}, {}},
  { "i!!c",       1,  {{ 254, 9, 8 }}, {}},
  { "i!!!c",      1,  {{ 254, 3, 2 }}, {}},
  { "i!!!!c",     1,  {{ 254, 1, 1 }}, {}},
  { "q!c",        1,  {{ 254, 49, 48 }}, {}},
  { "q!!c",       1,  {{ 254, 17, 16 }}, {}},
  { "q!!!c",      1,  {{ 254, 6, 5 }}, {}},
  { "q!!!!c",     1,  {{ 254, 2, 1 }}, {}},
  { "h!c",        1,  {{ 254, 97, 96 }}, {}},
  { "h!!c",       1,  {{ 254, 33, 32 }}, {}},
  { "h!!!c",      1,  {{ 254, 11, 10 }}, {}},
  { "h!!!!c",     1,  {{ 254, 4, 3 }}, {}},
  { "w!c",        1,  {{ 254, 193, 192 }}, {}},
  { "w!!c",       1,  {{ 254, 65, 64 }}, {}},
  { "w!!!c",      1,  {{ 254, 22, 21 }}, {}},
  { "w!!!!c",     1,  {{ 254, 8, 7 }}, {}},

  /* Combined: dots/triplets can be combined, including with truncation */
  { "t.!c",       1,  {{ 254, 10, 9 }}, {}},
  { "t!.c",       1,  {{ 254, 10, 9 }}, {}},
  { "w!..c",      1,  {{ 254, 433, 432 }}, {}},
  { "z!..c",      1,  {{ 254, 7, 6 }}, {}},
  { "z.!c",       1,  {{ 254, 5, 4 }}, {}},
  { "z....!c",    1,  {{ 254, 15, 14 }}, {}},
  { "z!....c",    1,  {{ 254, 14, 13 }}, {}},

  /* New duration completely replaces previous */
  { "ztc",        1,  {{ 254, 19, 18 }}, {}},
  { "t.tc",       1,  {{ 254, 19, 18 }}, {}},
  { "t!zc",       1,  {{ 254, 10, 9 }}, {}},
  { "w.....ic",   1,  {{ 254, 73, 72 }}, {}},
  { "z!!!sc",     1,  {{ 254, 37, 36 }}, {}},

  /* Audio system clamps overflowed duration to UINT16_MAX */
  { "w...........c",  1, {{ 254, 49817, 49816 }}, {}},
  { "w............c", 1, {{ 254, 65535, 65535 }}, {}}, /* <- overflow */
  { "w.............c",1, {{ 254, 65535, 65535 }}, {}},
};

/* Multi-note behavior including injected rests etc. */
static constexpr struct play_test_data play_sequences[] =
{
  /* Simple multi-note */
  { "sabc",   3,  {{ 428, 37, 36 }, { 480, 37, 36 }, { 254, 37, 36 }}, {}},

  /* Octaves are persistent */
  { "c+dc",   3,  {{ 254, 19, 18 }, { 570, 19, 18 }, { 508, 19, 18 }}, {}},
  { "c-dc",   3,  {{ 254, 19, 18 }, { 142, 19, 18 }, { 127, 19, 18 }}, {}},
  { "c0dc",   3,  {{ 254, 19, 18 },  { 35, 19, 18 }, { 31, 19, 18 }}, {}},
  { "c6dc",   3,  {{ 254, 19, 18 }, { 2280, 19, 18 }, { 2032, 19, 18 }}, {}},

  /* Accidentals only affect preceding note */
  { "c#c",    2,  {{ 269, 19, 18 }, { 254, 19, 18 }}, {}},
  { "c$c",    2,  {{ 240, 19, 18 }, { 254, 19, 18 }}, {}},

  /* Dot and triplet persist but are reset by a new duration */
  { ".cc#td", 3,  {{ 254, 28, 27 }, { 269, 28, 27 }, { 285, 19, 18 }}, {}},
  { "!cc#td", 3,  {{ 254, 7, 6 }, { 269, 7, 6 }, { 285, 19, 18 }}, {}},

  /* Rest insertion (original duration >9 makes the first 2 ticks a rest) */
  { "cc",     3,
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 3, 2 }, { 254, 17, 16 }}, {}},
  { "c$-b",   3,
    {{ 240, 19, 18 }, { AUDIO_SFX_FREQ_REST, 3, 2 }, { 240, 17, 16 }}, {}},
  { "c-b#",   3,
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 3, 2 }, { 254, 17, 16 }}, {}},
  { "csc",    3,
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 3, 2 }, { 254, 35, 34 }}, {}},
  { "cw...c", 3,
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 3, 2 }, { 254, 1943, 1942 }}, {}},
  { "cz......!!c", 3, /* Duration 10 still uses the long rest */
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 3, 2 }, { 254, 9, 8 }}, {}},
  { "3c4c",   3, /* Affects same note between octaves TODO: fixed mode should fix */
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 3, 2 }, { 508, 17, 16 }}, {}},

  /* Rest insertion (duration <=9 only makes the first tick of the note a rest) */
  { "czc",    3,
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 2, 1 }, { 254, 9, 8 }}, {}},
  { "c$z-b",  3,
    {{ 240, 19, 18 }, { AUDIO_SFX_FREQ_REST, 2, 1 }, { 240, 9, 8 }}, {}},
  { "cz-b#",  3,
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 2, 1 }, { 254, 9, 8 }}, {}},
  { "cz!!c",  3, /* TODO: this case should probably not add a rest in fixed mode */
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 2, 1 }, { 254, 1, 1 }}, {}},
  { "cz!!!c", 3, /* TODO: see above */
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 2, 1 }, { 254, 1, 1 }}, {}},
  { "3cz4c",  3, /* Affects same note between octaves TODO: fixed mode should fix */
    {{ 254, 19, 18 }, { AUDIO_SFX_FREQ_REST, 2, 1 }, { 508, 9, 8 }}, {}},
};

/* Test all persistent durations/octaves against a single junk character. */
static constexpr struct play_test_data play_junk[] =
{
  /* Durations incl. dot/triplet */
  { "zc@d",   2,  {{ 254, 10, 9 }, { 285, 10, 9 }}, {}},
  { "z.c@d",  2,  {{ 254, 14, 13 }, { 285, 14, 13 }}, {}},
  { "z!c@d",  2,  {{ 254, 4, 3 }, { 285, 4, 3 }}, {}},
  { "tc@d",   2,  {{ 254, 19, 18 }, { 285, 19, 18 }}, {}},
  { "t.c@d",  2,  {{ 254, 28, 27 }, { 285, 28, 27 }}, {}},
  { "t!c@d",  2,  {{ 254, 7, 6 }, { 285, 7, 6 }}, {}},
  { "sc@d",   2,  {{ 254, 37, 36 }, { 285, 37, 36 }}, {}},
  { "s.c@d",  2,  {{ 254, 55, 54 }, { 285, 55, 54 }}, {}},
  { "s!c@d",  2,  {{ 254, 13, 12 }, { 285, 13, 12 }}, {}},
  { "ic@d",   2,  {{ 254, 73, 72 }, { 285, 73, 72 }}, {}},
  { "i.c@d",  2,  {{ 254, 109, 108 }, { 285, 109, 108 }}, {}},
  { "i!c@d",  2,  {{ 254, 25, 24 }, { 285, 25, 24 }}, {}},
  { "qc@d",   2,  {{ 254, 145, 144 }, { 285, 145, 144 }}, {}},
  { "q.c@d",  2,  {{ 254, 217, 216 }, { 285, 217, 216 }}, {}},
  { "q!c@d",  2,  {{ 254, 49, 48 }, { 285, 49, 48 }}, {}},
  { "hc@d",   2,  {{ 254, 289, 288 }, { 285, 289, 288 }}, {}},
  { "h.c@d",  2,  {{ 254, 433, 432 }, { 285, 433, 432 }}, {}},
  { "h!c@d",  2,  {{ 254, 97, 96 }, { 285, 97, 96 }}, {}},
  { "wc@d",   2,  {{ 254, 577, 576 }, { 285, 577, 576 }}, {}},
  { "w.c@d",  2,  {{ 254, 865, 864 }, { 285, 865, 864 }}, {}},
  { "w!c@d",  2,  {{ 254, 193, 192 }, { 285, 193, 192 }}, {}},

  /* Octaves */
  { "t0c@d",  2,  {{ 31, 19, 18 }, { 35, 19, 18 }}, {}},
  { "t1c@d",  2,  {{ 63, 19, 18 }, { 71, 19, 18 }}, {}},
  { "t2c@d",  2,  {{ 127, 19, 18 }, { 142, 19, 18 }}, {}},
  { "t3c@d",  2,  {{ 254, 19, 18 }, { 285, 19, 18 }}, {}},
  { "t4c@d",  2,  {{ 508, 19, 18 }, { 570, 19, 18 }}, {}},
  { "t5c@d",  2,  {{ 1016, 19, 18 }, { 1140, 19, 18 }}, {}},
  { "t6c@d",  2,  {{ 2032, 19, 18 }, { 2280, 19, 18 }}, {}},
  { "t---c@d",2,  {{ 31, 19, 18 }, { 35, 19, 18 }}, {}},
  { "t--c@d", 2,  {{ 63, 19, 18 }, { 71, 19, 18 }}, {}},
  { "t-c@d",  2,  {{ 127, 19, 18 }, { 142, 19, 18 }}, {}},
  { "tc@d",   2,  {{ 254, 19, 18 }, { 285, 19, 18 }}, {}},
  { "t+c@d",  2,  {{ 508, 19, 18 }, { 570, 19, 18 }}, {}},
  { "t++c@d", 2,  {{ 1016, 19, 18 }, { 1140, 19, 18 }}, {}},
  { "t+++c@d",2,  {{ 2032, 19, 18 }, { 2280, 19, 18 }}, {}},
};

/* If music is enabled, everything to the right of an underscore is skipped. */
static constexpr struct play_test_data play_underscore_on[] =
{
  { "tc_d",   1,  {{ 254, 19, 18 }}, {}},
  { "t_c",    0,  {}, {}},
  { "tc__d_", 1,  {{ 254, 19, 18 }}, {}},
};

/* If music is disabled, any prior sample is canceled (tested later) and
 * parsing continues. */
static constexpr struct play_test_data play_underscore_off[] =
{
  { "tc_d",   2,  {{ 254, 19, 18 }, { 285, 19, 18 }}, {}},
  { "t_c",    1,  {{ 254, 19, 18 }}, {}},
  { "tc__d_", 2,  {{ 254, 19, 18 }, { 285, 19, 18 }}, {}},
};

/* Samples with music on:
 * - preceding note(S) play through the PC speaker.
 * - following note(s) causes sample to play at every specified note.
 * - no note plays the sample at natural frequency (added in 2.81b).
 *   No terminating & does the same, which only affects 2.81b onward and
 *   previously caused program/SFX corruption.
 * - everything after the underscore is ignored.
 */
static constexpr struct play_test_data play_samples[] =
{
  { "&test.wav&c",        0,  {}, {{ "test.wav", 428 }}},
  { "&test.wav&",         0,  {}, {{ "test.wav", 0 }}},
  { "&test.wav",          0,  {}, {{ "test.wav", 0 }}},
  { "t&test.wav&c_cde",   0,  {}, {{ "test.wav", 428 }}},
  { "&test.wav&cdef",     0,  {}, {{ "test.wav", 428 }, { "test.wav", 381 },
                                   { "test.wav", 339 }, { "test.wav", 320 }}},
  { "&&c",                0,  {}, {{ "&c", 0 }}},
  { "&&",                 0,  {}, {{ "&", 0 }}},
  { "&&c_zcde",           0,  {}, {{ "&c_zcde", 0 }}},
  { "&",                  0,  {}, {{ "", 0 }}},
  { "owch&",              1,  {{ 254, 577, 576 }}, {{ "", 0 }}},
  { "tcde&test.wav&c",    3,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }},
                              {{ "test.wav", 428 }}},
  { "tcde&test.wav&",     3,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }},
                              {{ "test.wav", 0 }}},
  { "tcde&test.wav",      3,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }},
                              {{ "test.wav", 0 }}},
  { "tcde&test.wav&c_cde",3,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }},
                              {{ "test.wav", 428 }}},
  { "&a.wav&c&b.wav&c",   0,  {}, {{ "a.wav", 428 }, { "b.wav", 428 }}},
  { "&a.wav&&b.wav&c",    0,  {}, {{ "b.wav", 428 }}},
  { "&a.wav&c&b.wav&",    0,  {}, {{ "a.wav", 428 }, { "b.wav", 0 }}},
  { "&a.wav&c&b.wav",     0,  {}, {{ "a.wav", 428 }, { "b.wav", 0 }}},
  { "&a.wav&c_&b.wav&c",  0,  {}, {{ "a.wav", 428 }}},
};

/* Samples with music off:
 * - preceding note(s) play through the PC speaker.
 * - following note(s) are suppressed until an underscore is encounered.
 * - playback continues past underscores.
 */
static constexpr struct play_test_data play_samples_music_off[] =
{
  { "&test.wav&c",        0,  {}, {}},
  { "&test.wav&",         0,  {}, {}},
  { "&test.wav",          0,  {}, {}},
  { "z&test.wav&c_cde",   3,  {{ 254, 10, 9 }, { 285, 10, 9 }, { 320, 10, 9 }}, {}},
  { "&test.wav&cz4_cde",  3,  {{ 508, 10, 9 }, { 570, 10, 9 }, { 640, 10, 9 }}, {}},
  { "&test.wav&cdef",     0,  {}, {}},
  { "&&c",                0,  {}, {}},
  { "&&",                 0,  {}, {}},
  { "&&c_zcde",           0,  {}, {}}, /* Nothing plays because of sample parsing bug */
  { "&",                  0,  {}, {}},
  { "owch&",              1,  {{ 254, 577, 576 }}, {}},
  { "tcde&test.wav&c",    3,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }}, {}},
  { "tcde&test.wav&",     3,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }}, {}},
  { "tcde&test.wav",      3,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }}, {}},
  { "tcde&test.wav&c_cde",6,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 },
                               { 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }}, {}},
  { "&a.wav&c&b.wav&c",   0,  {}, {}},
  { "&a.wav&&b.wav&c",    0,  {}, {}},
  { "&a.wav&c&b.wav&",    0,  {}, {}},
  { "&a.wav&c&b.wav",     0,  {}, {}},
  { "c&a.wav&c_d&b.wav&c_e",3,{{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }}, {}},
};

/* No SFX mode -> string will be queued regardless.
 * Test should attempt to play the input string twice rather than once. */
static constexpr struct play_test_data play_sfxmode_off[] =
{
  { "tcde",               6,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 },
                               { 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }}, {}},
  { "tcde&test.wav&c",    6,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 },
                               { 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }},
                              {{ "test.wav", 428 }, { "test.wav", 428 }}},
};

/* SFX mode will not play if there are notes already in the queue.
 * Test should attempt to play the input string twice rather than once. */
static constexpr struct play_test_data play_sfxmode_on[] =
{
  { "tcde",               3,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }}, {}},
  { "tcde&test.wav&c",    3,  {{ 254, 19, 18 }, { 285, 19, 18 }, { 320, 19, 18 }},
                              {{ "test.wav", 428 }, { "test.wav", 428 }}},
};

/* The built-in SFX for MegaZeux 2.00 onward should always produce these notes. */
static constexpr struct play_test_data play_default_sfx_200[NUM_BUILTIN_SFX] =
{
  { "<sfx 0>", 7,
    {{ 1016, 19, 18 }, { 762, 19, 18 }, { 640, 19, 18 }, { 508, 19, 18 },
     { 381, 19, 18 }, { 320, 19, 18 }, { 254, 19, 18 }}, {}},
  { "<sfx 1>", 7,
    {{ 1016, 19, 18 }, { 762, 19, 18 }, { 640, 19, 18 }, { 508, 19, 18 },
     { 381, 19, 18 }, { 320, 19, 18 }, { 254, 19, 18 }}, {}},
  { "<sfx 2>", 6,
    {{ 254, 19, 18 }, { 381, 19, 18 }, { 320, 19, 18 }, { 127, 10, 9 },
     { 190, 10, 9 }, { 160, 10, 9 }}, {}},
  { "<sfx 3>", 10,
    {{ 127, 19, 18 }, { 1, 3, 2 }, { 254, 17, 16 }, { 1, 3, 2 },
     { 508, 17, 16 }, { 381, 19, 18 }, { 1, 3, 2 }, { 190, 17, 16 },
     { 1, 3, 2 }, { 95, 17, 16 }}, {}},
  { "<sfx 4>", 4,
    {{ 3624, 19, 18 }, { 1, 3, 2 }, { 3624, 17, 16 }, { 1, 10, 9 }}, {}},
  { "<sfx 5>", 15,
    {{ 127, 19, 18 }, { 160, 19, 18 }, { 190, 19, 18 }, { 160, 19, 18 },
     { 190, 19, 18 }, { 254, 19, 18 }, { 190, 19, 18 }, { 254, 19, 18 },
     { 320, 19, 18 }, { 254, 19, 18 }, { 320, 19, 18 }, { 381, 19, 18 },
     { 320, 19, 18 }, { 381, 19, 18 }, { 508, 19, 18 }}, {}},
  { "<sfx 6>", 5,
    {{ 127, 19, 18 }, { 134, 19, 18 }, { 142, 19, 18 }, { 151, 19, 18 },
     { 160, 19, 18 }}, {}},
  { "<sfx 7>", 5,
    {{ 254, 19, 18 }, { 269, 19, 18 }, { 285, 19, 18 }, { 302, 19, 18 },
     { 320, 19, 18 }}, {}},
  { "<sfx 8>", 12,
    {{ 762, 19, 18 }, { 640, 19, 18 }, { 508, 19, 18 }, { 381, 19, 18 },
     { 640, 19, 18 }, { 508, 19, 18 }, { 381, 19, 18 }, { 320, 19, 18 },
     { 508, 19, 18 }, { 381, 19, 18 }, { 320, 19, 18 }, { 254, 19, 18 }}, {}},
  { "<sfx 9>", 4,
    {{ 190, 19, 18 }, { 134, 19, 18 }, { 113, 19, 18 }, { 107, 19, 18 }}, {}},
  { "<sfx 10>", 12,
    {{ 254, 19, 18 }, { 320, 19, 18 }, { 381, 19, 18 }, { 508, 19, 18 },
     { 320, 19, 18 }, { 381, 19, 18 }, { 508, 19, 18 }, { 640, 19, 18 },
     { 381, 19, 18 }, { 508, 19, 18 }, { 640, 19, 18 }, { 762, 19, 18 }}, {}},
  { "<sfx 11>", 6,
    {{ 113, 37, 36 }, { 1, 2, 1 }, { 226, 9, 8 }, { 1, 2, 1 },
     { 113, 9, 8 }, { 1, 10, 9 }}, {}},
  { "<sfx 12>", 9,
    {{ 214, 19, 18 }, { 1, 3, 2 }, { 107, 17, 16 }, { 160, 19, 18 },
     { 1, 3, 2 }, { 80, 17, 16 }, { 127, 19, 18 }, { 1, 3, 2 },
     { 63, 17, 16 }}, {}},
  { "<sfx 13>", 2,
    {{ 428, 10, 9 }, { 1, 10, 9 }}, {}},
  { "<sfx 14>", 10,
    {{ 56, 19, 18 }, { 1, 3, 2 }, { 113, 17, 16 }, { 1, 3, 2 },
     { 56, 17, 16 }, { 60, 19, 18 }, { 1, 3, 2 }, { 120, 17, 16 },
     { 1, 3, 2 }, { 60, 17, 16 }}, {}},
  { "<sfx 15>", 9,
    {{ 53, 19, 18 }, { 1, 3, 2 }, { 107, 17, 16 }, { 1, 3, 2 },
     { 53, 17, 16 }, { 1, 3, 2 }, { 107, 17, 16 }, { 1, 3, 2 },
     { 53, 17, 16 }}, {}},
  { "<sfx 16>", 25,
    {{ 190, 19, 18 }, { 1, 3, 2 }, { 381, 17, 16 }, { 1, 3, 2 },
     { 190, 17, 16 }, { 1, 3, 2 }, { 381, 17, 16 }, { 1, 3, 2 },
     { 762, 17, 16 }, { 1, 3, 2 }, { 381, 17, 16 }, { 1, 3, 2 },
     { 762, 17, 16 }, { 1, 3, 2 }, { 381, 17, 16 }, { 1, 3, 2 },
     { 190, 17, 16 }, { 1, 3, 2 }, { 381, 17, 16 }, { 1, 3, 2 },
     { 190, 17, 16 }, { 1, 3, 2 }, { 95, 17, 16 }, { 1, 3, 2 },
     { 190, 17, 16 }}, {}},
  { "<sfx 17>", 18,
    {{ 254, 37, 36 }, { 1, 3, 2 }, { 127, 35, 34 }, { 1, 145, 144 },
     { 190, 37, 36 }, { 1, 3, 2 }, { 95, 35, 34 }, { 226, 37, 36 },
     { 1, 3, 2 }, { 113, 35, 34 }, { 1, 37, 36 }, { 1, 37, 36 },
     { 190, 37, 36 }, { 1, 3, 2 }, { 95, 35, 34 }, { 226, 37, 36 },
     { 1, 3, 2 }, { 113, 35, 34 }}, {}},
  { "<sfx 18>", 22,
    {{ 254, 37, 36 }, { 1, 3, 2 }, { 127, 35, 34 }, { 1, 145, 144 },
     { 190, 37, 36 }, { 1, 3, 2 }, { 95, 35, 34 }, { 169, 37, 36 },
     { 1, 3, 2 }, { 84, 35, 34 }, { 151, 37, 36 }, { 1, 3, 2 },
     { 75, 35, 34 }, { 127, 37, 36 }, { 1, 3, 2 }, { 63, 35, 34 },
     { 113, 37, 36 }, { 1, 3, 2 }, { 56, 35, 34 }, { 127, 37, 36 },
     { 1, 3, 2 }, { 63, 35, 34 }}, {}},
  { "<sfx 19>", 10,
    {{ 47, 19, 18 }, { 1, 3, 2 }, { 95, 17, 16 }, { 1, 3, 2 },
     { 47, 17, 16 }, { 37, 19, 18 }, { 1, 3, 2 }, { 75, 17, 16 },
     { 1, 3, 2 }, { 37, 17, 16 }}, {}},
  { "<sfx 20>", 6,
    {{ 47, 19, 18 }, { 1, 3, 2 }, { 95, 17, 16 }, { 75, 19, 18 },
     { 1, 3, 2 }, { 151, 17, 16 }}, {}},
  { "<sfx 21>", 7,
    {{ 254, 19, 18 }, { 190, 10, 9 }, { 151, 10, 9 }, { 127, 10, 9 },
     { 95, 10, 9 }, { 75, 10, 9 }, { 63, 10, 9 }}, {}},
  { "<sfx 22>", 8,
    {{ 453, 19, 18 }, { 320, 19, 18 }, { 226, 19, 18 }, { 160, 19, 18 },
     { 113, 19, 18 }, { 80, 19, 18 }, { 56, 19, 18 }, { 40, 19, 18 }}, {}},
  { "<sfx 23>", 23,
    {{ 508, 19, 18 }, { 381, 19, 18 }, { 302, 19, 18 }, { 254, 19, 18 },
     { 302, 19, 18 }, { 254, 19, 18 }, { 190, 19, 18 }, { 151, 19, 18 },
     { 190, 19, 18 }, { 151, 19, 18 }, { 127, 19, 18 }, { 95, 19, 18 },
     { 127, 19, 18 }, { 95, 19, 18 }, { 75, 19, 18 }, { 63, 19, 18 },
     { 75, 19, 18 }, { 63, 19, 18 }, { 47, 19, 18 }, { 37, 19, 18 },
     { 47, 19, 18 }, { 37, 19, 18 }, { 31, 19, 18 }}, {}},
  { "<sfx 24>", 47,
    {{ 254, 73, 72 }, { 1, 3, 2 }, { 63, 71, 70 }, { 1, 3, 2 },
     { 127, 71, 70 }, { 226, 73, 72 }, { 254, 73, 72 }, { 1, 3, 2 },
     { 63, 71, 70 }, { 1, 3, 2 }, { 127, 71, 70 }, { 1, 73, 72 },
     { 302, 73, 72 }, { 63, 73, 72 }, { 1, 3, 2 }, { 127, 71, 70 },
     { 226, 73, 72 }, { 254, 73, 72 }, { 1, 3, 2 }, { 63, 71, 70 },
     { 1, 3, 2 }, { 127, 71, 70 }, { 1, 73, 72 }, { 1, 3, 2 },
     { 254, 71, 70 }, { 1, 3, 2 }, { 63, 71, 70 }, { 1, 3, 2 },
     { 127, 71, 70 }, { 226, 73, 72 }, { 254, 73, 72 }, { 1, 3, 2 },
     { 63, 71, 70 }, { 1, 3, 2 }, { 127, 71, 70 }, { 1, 73, 72 },
     { 302, 73, 72 }, { 63, 73, 72 }, { 1, 3, 2 }, { 127, 71, 70 },
     { 339, 73, 72 }, { 254, 73, 72 }, { 1, 3, 2 }, { 63, 71, 70 },
     { 1, 3, 2 }, { 127, 71, 70 }, { 1, 73, 72 }}, {}},
  { "<sfx 25>", 9,
    {{ 31, 19, 18 }, { 1, 3, 2 }, { 63, 17, 16 }, { 1, 3, 2 },
     { 31, 17, 16 }, { 1, 3, 2 }, { 63, 17, 16 }, { 1, 3, 2 },
     { 31, 17, 16 }}, {}},
  { "<sfx 26>", 2,
    {{ 75, 19, 18 }, { 1, 19, 18 }}, {}},
  { "<sfx 27>", 18,
    {{ 127, 19, 18 }, { 1, 3, 2 }, { 254, 17, 16 }, { 1, 3, 2 },
     { 508, 17, 16 }, { 151, 19, 18 }, { 1, 3, 2 }, { 302, 17, 16 },
     { 1, 3, 2 }, { 604, 17, 16 }, { 190, 19, 18 }, { 1, 3, 2 },
     { 381, 17, 16 }, { 1, 3, 2 }, { 762, 17, 16 }, { 254, 19, 18 },
     { 302, 19, 18 }, { 381, 19, 18 }}, {}},
  { "<sfx 28>", 3,
    {{ 508, 10, 9 }, { 1, 2, 1 }, { 254, 9, 8 }}, {}},
  { "<sfx 29>", 5,
    {{ 107, 19, 18 }, { 1, 3, 2 }, { 214, 17, 16 }, { 1, 3, 2 },
     { 428, 17, 16 }}, {}},
  { "<sfx 30>", 5,
    {{ 214, 19, 18 }, { 180, 19, 18 }, { 1, 3, 2 }, { 90, 17, 16 },
     { 107, 19, 18 }}, {}},
  { "<sfx 31>", 1,
    {{ 720, 19, 18 }}, {}},
  { "<sfx 32>", 5,
    {{ 142, 37, 36 }, { 1, 3, 2 }, { 71, 35, 34 }, { 1, 3, 2 },
     { 35, 35, 34 }}, {}},
  { "<sfx 33>", 5,
    {{ 254, 19, 18 }, { 214, 19, 18 }, { 160, 19, 18 }, { 127, 19, 18 },
     { 107, 19, 18 }}, {}},
  { "<sfx 34>", 5,
    {{ 508, 19, 18 }, { 428, 19, 18 }, { 320, 19, 18 }, { 254, 19, 18 },
     { 214, 19, 18 }}, {}},
  { "<sfx 35>", 8,
    {{ 762, 19, 18 }, { 320, 19, 18 }, { 381, 19, 18 }, { 320, 19, 18 },
     { 1, 3, 2 }, { 160, 17, 16 }, { 190, 19, 18 }, { 160, 19, 18 }}, {}},
  { "<sfx 36>", 10,
    {{ 63, 19, 18 }, { 1, 3, 2 }, { 127, 17, 16 }, { 37, 19, 18 },
     { 1, 3, 2 }, { 75, 17, 16 }, { 47, 19, 18 }, { 1, 3, 2 },
     { 95, 17, 16 }, { 33, 19, 18 }}, {}},
  { "<sfx 37>", 16,
    {{ 127, 19, 18 }, { 190, 19, 18 }, { 320, 19, 18 }, { 508, 19, 18 },
     { 134, 19, 18 }, { 202, 19, 18 }, { 339, 19, 18 }, { 538, 19, 18 },
     { 142, 19, 18 }, { 214, 19, 18 }, { 360, 19, 18 }, { 570, 19, 18 },
     { 151, 19, 18 }, { 226, 19, 18 }, { 381, 19, 18 }, { 604, 19, 18 }}, {}},
  { "<sfx 38>", 13,
    {{ 254, 19, 18 }, { 381, 19, 18 }, { 320, 19, 18 }, { 508, 19, 18 },
     { 381, 19, 18 }, { 640, 19, 18 }, { 508, 19, 18 }, { 762, 19, 18 },
     { 640, 19, 18 }, { 1016, 19, 18 }, { 762, 19, 18 }, { 1280, 19, 18 },
     { 1016, 19, 18 }}, {}},
  { "<sfx 39>", 49,
    {{ 254, 10, 9 }, { 302, 10, 9 }, { 381, 10, 9 }, { 302, 10, 9 },
     { 254, 10, 9 }, { 302, 10, 9 }, { 381, 10, 9 }, { 302, 10, 9 },
     { 254, 10, 9 }, { 302, 10, 9 }, { 381, 10, 9 }, { 302, 10, 9 },
     { 254, 10, 9 }, { 302, 10, 9 }, { 381, 10, 9 }, { 302, 10, 9 },
     { 254, 10, 9 }, { 339, 10, 9 }, { 404, 10, 9 }, { 339, 10, 9 },
     { 254, 10, 9 }, { 339, 10, 9 }, { 404, 10, 9 }, { 339, 10, 9 },
     { 254, 10, 9 }, { 339, 10, 9 }, { 404, 10, 9 }, { 339, 10, 9 },
     { 254, 10, 9 }, { 339, 10, 9 }, { 404, 10, 9 }, { 339, 10, 9 },
     { 254, 10, 9 }, { 381, 10, 9 }, { 453, 10, 9 }, { 381, 10, 9 },
     { 254, 10, 9 }, { 381, 10, 9 }, { 453, 10, 9 }, { 381, 10, 9 },
     { 254, 10, 9 }, { 381, 10, 9 }, { 453, 10, 9 }, { 381, 10, 9 },
     { 254, 10, 9 }, { 381, 10, 9 }, { 453, 10, 9 }, { 381, 10, 9 },
     { 508, 37, 36 }}, {}},
  { "<sfx 40>", 11,
    {{ 214, 10, 9 }, { 1, 2, 1 }, { 107, 9, 8 }, { 1, 2, 1 },
     { 53, 9, 8 }, { 1, 2, 1 }, { 53, 9, 8 }, { 1, 2, 1 },
     { 53, 9, 8 }, { 1, 2, 1 }, { 53, 9, 8 }}, {}},
  { "<sfx 41>", 18,
    {{ 63, 19, 18 }, { 1, 3, 2 }, { 127, 17, 16 }, { 67, 19, 18 },
     { 1, 3, 2 }, { 134, 17, 16 }, { 71, 19, 18 }, { 1, 3, 2 },
     { 142, 17, 16 }, { 75, 19, 18 }, { 1, 3, 2 }, { 151, 17, 16 },
     { 80, 19, 18 }, { 1, 3, 2 }, { 160, 17, 16 }, { 1, 3, 2 },
     { 80, 17, 16 }, { 63, 19, 18 }}, {}},
  { "<sfx 42>", 8,
    {{ 254, 19, 18 }, { 190, 19, 18 }, { 151, 19, 18 }, { 127, 19, 18 },
     { 75, 10, 9 }, { 95, 10, 9 }, { 75, 10, 9 }, { 63, 10, 9 }}, {}},
  { "<sfx 43>", 5,
    {{ 254, 10, 9 }, { 151, 10, 9 }, { 190, 10, 9 }, { 63, 10, 9 },
     { 75, 10, 9 }}, {}},
  { "<sfx 44>", 6,
    {{ 254, 19, 18 }, { 302, 19, 18 }, { 381, 19, 18 }, { 508, 19, 18 },
     { 604, 19, 18 }, { 762, 19, 18 }}, {}},
  { "<sfx 45>", 5,
    {{ 75, 10, 9 }, { 1, 2, 1 }, { 151, 9, 8 }, { 1, 2, 1 },
     { 302, 9, 8 }}, {}},
  { "<sfx 46>", 6,
    {{ 31, 10, 9 }, { 56, 10, 9 }, { 42, 10, 9 }, { 75, 10, 9 },
     { 63, 10, 9 }, { 90, 10, 9 }}, {}},
  { "<sfx 47>", 12,
    {{ 254, 19, 18 }, { 381, 19, 18 }, { 508, 19, 18 }, { 320, 19, 18 },
     { 285, 19, 18 }, { 428, 19, 18 }, { 570, 19, 18 }, { 360, 19, 18 },
     { 320, 19, 18 }, { 480, 19, 18 }, { 640, 19, 18 }, { 404, 19, 18 }}, {}},
  { "<sfx 48>", 9,
    {{ 63, 19, 18 }, { 1, 3, 2 }, { 127, 17, 16 }, { 1, 3, 2 },
     { 254, 17, 16 }, { 1, 3, 2 }, { 508, 17, 16 }, { 1, 3, 2 },
     { 1016, 17, 16 }}, {}},
  { "<sfx 49>", 0, {}, {}},
};

#ifndef CONFIG_AUDIO
/* Fix compilation with audio disabled (tests should be skipped regardless). */
static inline boolean _audio_sfx_get_next_sound(int *freq, int *duration)
{
  *freq = AUDIO_SFX_FREQ_REST;
  *duration = 1;
  return false;
}
#define audio_sfx_get_next_sound _audio_sfx_get_next_sound
#endif

static void check_notes(const struct play_test_data &d, boolean sfx_mode)
{
  /* TODO: can't verify played samples currently */
  size_t i;
  int freq;
  int duration;
  boolean ret;

  boolean is_playing = sfx_is_playing();
  int length_left = sfx_length_left();

#define CHECK_MSG "%s[%zu/%zu] sfx:%d freq:%d dur:%d is_playing:%d length_left:%d", \
 d.str, i, d.num_notes, sfx_mode, freq, duration, is_playing, length_left

  for(i = 0; i < d.num_notes; i++)
  {
    ret = audio_sfx_get_next_sound(&freq, &duration);
    ASSERTEQ(ret, (d.notes[i].freq > AUDIO_SFX_FREQ_REST), CHECK_MSG);
    ASSERTEQ(freq, d.notes[i].freq, CHECK_MSG);
    ASSERTEQ(duration, d.notes[i].duration_classic, CHECK_MSG);
    int expected_fix = (duration == 1 || duration == UINT16_MAX) ? duration : duration - 1;
    ASSERTEQ(expected_fix, d.notes[i].duration_fixed, CHECK_MSG);
  }
  /* No remaining notes on queue -> return false, duration 1, rest note. */
  ret = audio_sfx_get_next_sound(&freq, &duration);
  ASSERTEQ(ret, false, CHECK_MSG);
  ASSERTEQ(freq, AUDIO_SFX_FREQ_REST, CHECK_MSG);
  ASSERTEQ(duration, 1, CHECK_MSG);

  /* These functions are easier to check as part of this test... */
  ASSERTEQ(is_playing, d.num_notes != 0, CHECK_MSG);
  ASSERTEQ((size_t)length_left, d.num_notes, CHECK_MSG);
  ASSERTEQ(sfx_is_playing(), false, CHECK_MSG);
  ASSERTEQ(sfx_length_left(), 0, CHECK_MSG);
#undef CHECK_MSG
}

static void check_pcsoff(const struct play_test_data &d, boolean sfx_mode)
{
  /* TODO: samples should still play */
  int freq;
  int duration;
  boolean ret;

  boolean is_playing = sfx_is_playing();
  int length_left = sfx_length_left();

#define CHECK_MSG "%s[%zu/%zu] sfx:%d freq:%d dur:%d is_playing:%d length_left:%d", \
 d.str, (size_t)0, d.num_notes, sfx_mode, freq, duration, is_playing, length_left

  ret = audio_sfx_get_next_sound(&freq, &duration);
  ASSERTEQ(ret, false, CHECK_MSG);
  ASSERTEQ(freq, AUDIO_SFX_FREQ_REST, CHECK_MSG);
  ASSERTEQ(duration, 1, CHECK_MSG);

  ASSERTEQ(is_playing, false, CHECK_MSG);
  ASSERTEQ(length_left, 0, CHECK_MSG);
#undef CHECK_MSG
}

static inline boolean special_sfx_char(size_t t)
{
  t = tolower(t & 0xff);
  if((t >= '0' && t <= '9') || (t >= 'a' && t <= 'g') || t == 'x' ||
   t == 'z' || t == 't' || t == 's' || t == 'i' || t == 'q' || t == 'h' ||
   t == 'w' || t == '.' || t == '!' || t == SAM_S[0] || t == SAM_E[0] ||
   t == '-' || t == '+' || t == '#' || t == '$' || t == '_' || t == '\0')
    return true;

  return false;
}

template<int N, class... args>
static void play_check(const struct play_test_data (&data)[N],
 std::function<void (const struct play_test_data &, args...)> &f, args... fargs)
{
  for(const play_test_data &d : data)
    f(d, fargs...);
}

/* Test ALL junk chars against a single duration/octave combo. */
template<class... args>
static void play_junk2_check(
 std::function<void (const struct play_test_data &, args...)> &f, args... fargs)
{
  for(size_t i = 0; i < 256; i++)
  {
    if(special_sfx_char(i))
      continue;

    char str[8] = { 't', '4', '+', 'c', static_cast<char>(i), 'd' };
    play_test_data tmp{ str, 2, {{ 1016, 19, 18 }, { 1140, 19, 18 }}, {}};
    f(tmp, fargs...);
  }
}

/* Requires sfx_play_string to at least push something to the queue to verify... */
UNITTEST(sfx_clear_queue)
{
  sfx_play_test_setup au;

  int freq;
  int duration;

  ASSERTEQ(audio_sfx_get_next_sound(&freq, &duration), false, "");

  sfx_play_string("sabc", false);
  ASSERTEQ(audio_sfx_get_next_sound(&freq, &duration), true, "");

  sfx_clear_queue();
  ASSERTEQ(audio_sfx_get_next_sound(&freq, &duration), false, "");

  sfx_play_string("wabcabcabcabcabcabc", false);
  ASSERTEQ(audio_sfx_get_next_sound(&freq, &duration), true, "");

  sfx_clear_queue();
  ASSERTEQ(audio_sfx_get_next_sound(&freq, &duration), false, "");
}

UNITTEST(sfx_play_string)
{
  sfx_play_test_setup au;

  auto sfx_play_string_check_fn =
   std::function<void (const play_test_data &)>([](const play_test_data &d)
  {
    sfx_play_string(d.str, false);
    check_notes(d, false);
    sfx_play_string(d.str, true);
    check_notes(d, true);
  });

  auto sfx_play_string_sfxmode_check_fn =
   std::function<void (const play_test_data &, boolean)>(
    [](const play_test_data &d, boolean sfx_mode)
  {
    sfx_play_string(d.str, sfx_mode);
    sfx_play_string(d.str, sfx_mode);
    check_notes(d, sfx_mode);
  });

  auto sfx_play_string_pcsoff_check_fn =
   std::function<void (const play_test_data &)>([](const play_test_data &d)
  {
    sfx_play_string(d.str, false);
    check_pcsoff(d, false);
    sfx_play_string(d.str, true);
    check_pcsoff(d, true);
  });

  SECTION(Notes)
    play_check(play_notes, sfx_play_string_check_fn);

  SECTION(Durations)
    play_check(play_durations, sfx_play_string_check_fn);

  SECTION(Sequences)
    play_check(play_sequences, sfx_play_string_check_fn);

  SECTION(JunkChars1)
    play_check(play_junk, sfx_play_string_check_fn);

  SECTION(JunkChars2)
    play_junk2_check(sfx_play_string_check_fn);

  SECTION(Underscore)
    play_check(play_underscore_on, sfx_play_string_check_fn);

  SECTION(UnderscoreMusicOff)
  {
    au.reinit(false, true);
    play_check(play_underscore_off, sfx_play_string_check_fn);
  }

  SECTION(PCSOff)
  {
    au.reinit(true, false);
    play_check(play_sequences, sfx_play_string_pcsoff_check_fn);
  }

  SECTION(Samples)
    play_check(play_samples, sfx_play_string_check_fn);

  SECTION(SamplesMusicOff)
  {
    au.reinit(false, true);
    play_check(play_samples_music_off, sfx_play_string_check_fn);
  }

  SECTION(SamplesPCSOff)
  {
    au.reinit(true, false);
    play_check(play_samples, sfx_play_string_pcsoff_check_fn);
  }

  SECTION(SFXModeOff)
    play_check(play_sfxmode_off, sfx_play_string_sfxmode_check_fn, (boolean)false);

  SECTION(SFXModeOn)
    play_check(play_sfxmode_on, sfx_play_string_sfxmode_check_fn, (boolean)true);

  SECTION(Corruption)
  {
    /* 2.81b until 2.93e could corrupt the nul terminator in case 1 and
     * the byte preceding the start of the string in case 2. */
    char test1[] = "_&test1";
    char test2[] = "_test2&";

    sfx_play_string(test1 + 1, false);
    ASSERTEQ(test1[0], '_', "");
    ASSERTEQ(test1[7], '\0', "");
    sfx_play_string(test1 + 1, true);
    ASSERTEQ(test1[0], '_', "");
    ASSERTEQ(test1[7], '\0', "");
    sfx_play_string(test2 + 1, false);
    ASSERTEQ(test2[0], '_', "");
    ASSERTEQ(test2[7], '\0', "");
    sfx_play_string(test2 + 1, false);
    ASSERTEQ(test2[0], '_', "");
    ASSERTEQ(test2[7], '\0', "");
  }
}

UNITTEST(sfx_play_from_list)
{
  sfx_play_test_setup au;
  struct sfx_list sfx_list{};
  auto_free a(sfx_list);

  auto sfx_play_list_check_fn =
   std::function<void (const play_test_data &, int)>(
    [&sfx_list](const play_test_data &d, int sfx_number)
  {
    if(d.str[0] == '\0') /* This clears the SFX and plays the default... */
      return;

    boolean ret = sfx_set_string(&sfx_list, sfx_number, d.str, strlen(d.str));
    ASSERTEQ(ret, true, "%s", d.str);
    sfx_play_from_list(&sfx_list, sfx_number);
    check_notes(d, true);
  });

  auto sfx_play_list_sfxmode_check_fn =
   std::function<void (const play_test_data &, boolean)>(
    [&sfx_list](const play_test_data &d, boolean sfx_mode)
  {
    boolean ret = sfx_set_string(&sfx_list, 0, d.str, strlen(d.str));
    ASSERTEQ(ret, true, "%s", d.str);
    sfx_play_from_list(&sfx_list, 0);
    sfx_play_from_list(&sfx_list, 0);
    check_notes(d, sfx_mode);
  });

  auto sfx_play_list_pcsoff_check_fn =
   std::function<void (const play_test_data &, int)>(
    [&sfx_list](const play_test_data &d, int sfx_number)
  {
    boolean ret = sfx_set_string(&sfx_list, sfx_number, d.str, strlen(d.str));
    ASSERTEQ(ret, true, "%s", d.str);
    sfx_play_from_list(&sfx_list, sfx_number);
    check_pcsoff(d, true);
  });

  SECTION(Notes)
    play_check(play_notes, sfx_play_list_check_fn, 0);

  SECTION(Durations)
    play_check(play_durations, sfx_play_list_check_fn, 0);

  SECTION(Sequences)
    play_check(play_sequences, sfx_play_list_check_fn, 0);

  SECTION(JunkChars1)
    play_check(play_junk, sfx_play_list_check_fn, 0);

  SECTION(JunkChars2)
    play_junk2_check(sfx_play_list_check_fn, 0);

  SECTION(Underscore)
    play_check(play_underscore_on, sfx_play_list_check_fn, 0);

  SECTION(UnderscoreMusicOff)
  {
    au.reinit(false, true);
    play_check(play_underscore_off, sfx_play_list_check_fn, 0);
  }

  SECTION(PCSOff)
  {
    au.reinit(true, false);
    play_check(play_sequences, sfx_play_list_pcsoff_check_fn, 0);
  }

  SECTION(Samples)
    play_check(play_samples, sfx_play_list_check_fn, 0);

  SECTION(SamplesMusicOff)
  {
    au.reinit(false, true);
    play_check(play_samples_music_off, sfx_play_list_check_fn, 0);
  }

  SECTION(SamplesPCSOff)
  {
    au.reinit(true, false);
    play_check(play_samples, sfx_play_list_pcsoff_check_fn, 0);
  }

  /* SFX always have SFX mode enabled */
  SECTION(SFXModeOn)
    play_check(play_sfxmode_on, sfx_play_list_sfxmode_check_fn, (boolean)true);

  SECTION(DefaultSFX)
  {
    size_t i;
    for(i = 0; i < NUM_BUILTIN_SFX; i++)
    {
      sfx_clear_queue();
      sfx_play_from_list(&sfx_list, i);
      check_notes(play_default_sfx_200[i], true);
    }
    for(; i < MAX_NUM_SFX; i++)
    {
      sfx_play_from_list(&sfx_list, i);
      ASSERTEQ(sfx_is_playing(), false, "%zu", i);
      ASSERTEQ(sfx_length_left(), 0, "%zu", i);
    }
  }

  SECTION(AllSFX)
  {
    /* All 256 SFX should be addressable. The functions technically support
     * more, but 256 is the soft limit enforced elsewhere. */
    size_t i;
    for(i = 0; i < MAX_NUM_SFX; i++)
    {
      static constexpr struct play_test_data t = { "+ze", 1, {{ 640, 10, 9 }}, {}};
      boolean ret;

      ret = sfx_set_string(&sfx_list, i, t.str, strlen(t.str));
      ASSERTEQ(ret, true, "%zu", i);
      sfx_play_from_list(&sfx_list, i);
      check_notes(t, true);
      sfx_unset(&sfx_list, i);
    }
  }
}

UNITTEST(FillQueue)
{
  sfx_play_test_setup au;
  struct sfx_list sfx_list{};
  auto_free a(sfx_list);

  size_t i;
  for(i = 0; i < AUDIO_SFX_QUEUE_SIZE + 3; i++)
    sfx_play_string("c", false);

  boolean ret = sfx_is_playing();
  ASSERTEQ(ret, true, "");
  int num = sfx_length_left();
  ASSERTEQ(num, AUDIO_SFX_QUEUE_SIZE - 1, "");

  int freq;
  int duration;
  for(i = 0; i < AUDIO_SFX_QUEUE_SIZE - 1; i++)
  {
    ret = audio_sfx_get_next_sound(&freq, &duration);
    ASSERTEQ(ret, true, "%zu", i);
    ASSERTEQ(freq, 254, "%zu", i);
    ASSERTEQ(duration, 19, "%zu", i);
  }

  ret = audio_sfx_get_next_sound(&freq, &duration);
  ASSERTEQ(ret, false, "%zu", i);
  ASSERTEQ(freq, AUDIO_SFX_FREQ_REST, "%zu", i);
  ASSERTEQ(duration, 1, "%zu", i);
}
