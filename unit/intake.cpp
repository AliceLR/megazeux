/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Automated test for the parts of intake that are testable.
 * Currently, this is mainly intake_apply_event_fixed.
 */

#include "Unit.hpp"
#include "../src/intake.c"

// TODO shut up the linker as these aren't CORE_LIBSPEC currently.
void cursor_underline(Uint32 x, Uint32 y) {}
boolean has_unicode_input() { return false; }

struct int_pair
{
  int input;
  int expected;
};

struct event_input_data
{
  const char *base;
  int start_pos;
  const char *input;
  const char *expected;
};

struct event_repeat_data
{
  const char *base;
  int start_pos;
  int repeat_times;
  const char *expected;
};

static const int_pair pos_data[] =
{
  { 0, 0 },
  { 50, 50 },
  { 100, 100 },
  { 1000000, 100 },
  { -1, 0 },
  { -12781831, 0 },
};

static const int_pair length_data[] =
{
  { 0, 0 },
  { 50, 50 },
  { 240, 240 },
  { 1000, 240 },
  { 1000000, 240 },
  { -1, 0 },
  { -4983412, 0 },
};

static const char skip_template[256] =
 "aaa bb`cc~dd!ee@ff#gg$hh%ii^jj&kk*ll(mm)"
 "nn-oo_pp=qq+rr[ss]tt{uu}vv\\ww|xx;yy:zz'01\""
 "23,45<67.>89/00??0";

static const int skip_forward_positions[] =
{
   0,  3,  6,  9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39,
  42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81,
  84, 87, 90, 94, 97, 100
};

static const int skip_backward_positions[] =
{
  100, 99, 95, 92, 88, 85, 82,
   79, 76, 73, 70, 67, 64, 61, 58, 55, 52, 49, 46, 43, 40,
   37, 34, 31, 28, 25, 22, 19, 16, 13, 10,  7,  4,  0
};

/**
 * Test internal and external functions for manipulating the position and
 * current length of the intake editor. A large portion of this is making sure
 * the external position and length pointers are syncronized correctly.
 */
UNITTEST(PosLength)
{
  struct intake_subcontext intk{};
  char dest[256];
  char buf[80];
  int ext = 0;

  intk.current_length = 100;
  intk.max_length = 240;

  samesize(dest, skip_template);
  memcpy(dest, skip_template, arraysize(dest));

  SECTION(intake_set_pos_no_external)
  {
    for(const int_pair &d : pos_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_pos(&intk, d.input);
      ASSERTEQX(intk.pos, d.expected, buf);
    }
  }

  SECTION(intake_set_pos_external)
  {
    intk.pos_external = &ext;

    for(const int_pair &d : pos_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_pos(&intk, d.input);
      ASSERTEQX(intk.pos, d.expected, buf);
      ASSERTEQX(ext, d.expected, buf);
    }
  }

  SECTION(intake_set_length_no_external)
  {
    for(const int_pair &d : length_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_length(&intk, d.input);
      ASSERTEQX(intk.current_length, d.expected, buf);
    }
  }

  SECTION(intake_set_length_external)
  {
    intk.length_external = &ext;

    for(const int_pair &d : length_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_length(&intk, d.input);
      ASSERTEQX(intk.current_length, d.expected, buf);
      ASSERTEQX(ext, d.expected, buf);
    }
  }

  // intake_sync should not crash when the provided intake is null.
  SECTION(intake_sync_null)
  {
    intake_sync(nullptr);
  }

  /**
   * When no buffer/external pointers are present, intake_sync should not
   * modify the intake position or current length values.
   */
  SECTION(intake_sync_no_data_or_external)
  {
    for(const int_pair &d : pos_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_pos(&intk, d.input);
      intake_sync((subcontext *)&intk);
      ASSERTEQX(intk.pos, d.expected, buf);
    }

    for(const int_pair &d : length_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_length(&intk, d.input);
      intake_sync((subcontext *)&intk);
      ASSERTEQX(intk.current_length, d.expected, buf);
    }
  }

  /**
   * When an external position pointer is present, intake_sync should update
   * the position to reflect the provided external position, bounded to the
   * current length.
   */
  SECTION(intake_sync_pos)
  {
    intk.pos_external = &ext;

    for(const int_pair &d : pos_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      ext = d.input;
      intake_sync((subcontext *)&intk);
      ASSERTEQX(intk.pos, d.expected, buf);
      ASSERTEQX(ext, d.expected, buf);
    }
  }

  /**
   * When a buffer is present, intake_sync should update the current length to
   * the length of the buffer string.
   */
  SECTION(intake_sync_no_external)
  {
    int dest_len = strlen(dest);
    intk.current_length = 100;
    intk.dest = dest;
    intk.pos = 97;

    for(const int_pair &d : length_data)
    {
      dest_len = std::max(0, std::min(dest_len, d.input));
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, dest_len);
      intake_set_length(&intk, d.input);
      intake_sync((subcontext *)&intk);
      ASSERTEQX(intk.current_length, dest_len, buf);
      ASSERTEQX(intk.pos, 97, buf);
    }
  }

  /**
   * When external pointers are present but there is no buffer, intake_sync
   * should update the current length to the external length.
   */
  SECTION(intake_sync_no_dest)
  {
    intk.length_external = &ext;
    intk.dest = nullptr;

    for(const int_pair &d : length_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      ext = d.input;
      intake_sync((subcontext *)&intk);
      ASSERTEQX(intk.current_length, d.expected, buf);
      ASSERTEQX(ext, d.expected, buf);
    }
  }

  /**
   * When both dest and external length are present, dest should be preferred
   * for updating the length.
   */
  SECTION(intake_sync_dest_and_external)
  {
    int dest_len = strlen(dest);
    intk.length_external = &ext;
    intk.dest = dest;

    for(const int_pair &d : length_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, dest_len);
      ext = d.input;
      intake_sync((subcontext *)&intk);
      ASSERTEQX(intk.current_length, dest_len, buf);
      ASSERTEQX(ext, dest_len, buf);
    }
  }
}

/**
 * Test the default fixed-size buffer intake editing operations.
 */
UNITTEST(EventFixed)
{
  static const event_input_data insert_data[] =
  {
    { "test string :)", 0,  "Inserted ",                "Inserted test string :)" },
    { "abcdevwxyz",     5,  " put this in the middle ", "abcde put this in the middle vwxyz" },
    { "test string :)", 15, " append this.",            "test string :) append this." },
    { "",               0,  "blank string",             "blank string" },
    { "whatever",       0,  "",                         "whatever" },
    { "whatever",       4,  "",                         "whatever" },
    { "whatever",       8,  "",                         "whatever" },
  };
  static const event_input_data overwrite_data[] =
  {
    { "test string :)", 0,  "overwritten",              "overwritten :)" },
    { "abcdevwxyz",     5,  " put this at the end",     "abcde put this at the end" },
    { "test string :)", 15, " append this.",            "test string :) append this." },
    { "",               0,  "blank string",             "blank string" },
    { "whatever",       0,  "",                         "whatever" },
    { "whatever",       4,  "",                         "whatever" },
    { "whatever",       8,  "",                         "whatever" },
  };

  struct intake_subcontext intk{};
  subcontext *sub = reinterpret_cast<subcontext *>(&intk);
  char dest[256];
  char buf[80];
  boolean result;

  intk.dest = dest;
  intk.max_length = 240;
  intk.current_length = 100;
  intk.pos = 0;

  samesize(dest, skip_template);
  memcpy(dest, skip_template, arraysize(dest));

  SECTION(intake_skip_back)
  {
    int ext = 100;
    intk.pos = 100;
    intk.pos_external = &ext;

    for(int i : skip_backward_positions)
    {
      snprintf(buf, arraysize(buf), "%d", i);
      ASSERTEQX(intk.pos, i, buf);
      ASSERTEQX(ext, i, buf);
      intake_skip_back(&intk);
    }
  }

  SECTION(intake_skip_forward)
  {
    int ext = 0;
    intk.pos_external = &ext;

    for(int i : skip_forward_positions)
    {
      snprintf(buf, arraysize(buf), "%d", i);
      ASSERTEQX(intk.pos, i, buf);
      ASSERTEQX(ext, i, buf);
      intake_skip_forward(&intk);
    }
  }

  SECTION(intake_apply_event_fixed_errs)
  {
    // Should not crash with a null intake.
    result = intake_apply_event_fixed(nullptr, INTK_MOVE, 0, 0, nullptr);
    ASSERTEQ(result, false);

    // Should not crash with a null dest.
    intk.dest = nullptr;
    result = intake_apply_event_fixed(sub, INTK_MOVE, 0, 0, nullptr);
    ASSERTEQ(result, false);
    intk.dest = dest;

    // Should not crash if intk->pos is somehow invalid.
    int invalid_pos[] = { -1, -12431, intk.current_length + 1, 100000 };
    for(int i : invalid_pos)
    {
      intk.pos = i;
      result = intake_apply_event_fixed(sub, INTK_MOVE, 0, 0, nullptr);
      ASSERTEQ(result, false);
    }
  }

  SECTION(INTK_NO_EVENT)
  {
    // Nothing should ever actually send this event (if it does, it's a bug).
    result = intake_apply_event_fixed(sub, INTK_NO_EVENT, 0, 0, nullptr);
    ASSERTEQ(result, false);
  }

  SECTION(INTK_MOVE)
  {
    // Applying this moves the cursor, that's it.
    for(const int_pair &d : pos_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intk.pos = 25;
      result = intake_apply_event_fixed(sub, INTK_MOVE, d.input, 0, nullptr);
      ASSERTEQX(result, true, buf);
      ASSERTEQX(intk.pos, d.expected, buf);
    }
  }

  SECTION(INTK_MOVE_WORDS)
  {
    // This just wraps the skip forward/back functions.
    int ext = 0;
    intk.pos_external = &ext;

    for(int i : skip_forward_positions)
    {
      snprintf(buf, arraysize(buf), "%d (forward)", i);
      ASSERTEQX(intk.pos, i, buf);
      ASSERTEQX(ext, i, buf);
      result = intake_apply_event_fixed(sub, INTK_MOVE_WORDS, intk.pos, 1, nullptr);
      ASSERTEQX(result, true, buf);
    }

    ext = 100;
    intk.pos = 100;
    for(int i : skip_backward_positions)
    {
      snprintf(buf, arraysize(buf), "%d (backward)", i);
      ASSERTEQX(intk.pos, i, buf);
      ASSERTEQX(ext, i, buf);
      result = intake_apply_event_fixed(sub, INTK_MOVE_WORDS, intk.pos, -1, nullptr);
      ASSERTEQX(result, true, buf);
    }
  }

  SECTION(INTK_INSERT)
  {
    // Insert text into the buffer.
    for(const event_input_data &d : insert_data)
    {
      int expected_len = strlen(d.expected);
      strcpy(dest, d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      for(size_t i = 0, len = strlen(d.input); i < len; i++)
      {
        result = intake_apply_event_fixed(sub, INTK_INSERT, intk.pos + 1, d.input[i], nullptr);
        ASSERTEQX(result, true, d.expected);
      }
      ASSERTCMP(dest, d.expected);
      ASSERTEQ(intk.current_length, expected_len);
    }

    // Special: prevent inserting beyond the maximum length.
    intk.max_length = 8;
    intk.current_length = 7;
    strcpy(dest, "abcdefg");

    intk.pos = 7;
    result = intake_apply_event_fixed(sub, INTK_INSERT, intk.pos + 1, 'h', nullptr);
    ASSERTEQ(result, true);
    result = intake_apply_event_fixed(sub, INTK_INSERT, intk.pos + 1, 'i', nullptr);
    ASSERTEQ(result, false);
    ASSERTEQ(intk.pos, 8);
    intk.pos = 0;
    result = intake_apply_event_fixed(sub, INTK_INSERT, intk.pos + 1, 'i', nullptr);
    ASSERTEQ(result, false);
    ASSERTEQ(intk.pos, 0);
  }

  SECTION(INTK_OVERWRITE)
  {
    // Overwrite text in the buffer. Can insert at the end of the line.
    for(const event_input_data &d : overwrite_data)
    {
      int expected_len = strlen(d.expected);
      strcpy(dest, d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      for(size_t i = 0, len = strlen(d.input); i < len; i++)
      {
        result = intake_apply_event_fixed(sub, INTK_OVERWRITE, intk.pos + 1, d.input[i], nullptr);
        ASSERTEQX(result, true, d.expected);
      }
      ASSERTCMP(dest, d.expected);
      ASSERTEQ(intk.current_length, expected_len);
    }

    // Special: prevent inserting beyond the maximum length.
    intk.max_length = 8;
    intk.current_length = 7;
    strcpy(dest, "abcdefg");

    intk.pos = 7;
    result = intake_apply_event_fixed(sub, INTK_OVERWRITE, intk.pos + 1, 'h', nullptr);
    ASSERTEQ(result, true);
    result = intake_apply_event_fixed(sub, INTK_OVERWRITE, intk.pos + 1, 'i', nullptr);
    ASSERTEQ(result, false);
    ASSERTEQ(intk.pos, 8);
    // This should work, though.
    intk.pos = 0;
    result = intake_apply_event_fixed(sub, INTK_OVERWRITE, intk.pos + 1, 'i', nullptr);
    ASSERTEQ(result, true);
    ASSERTCMP(dest, "ibcdefgh");
  }

  SECTION(INTK_DELETE)
  {
    // Delete the char at the cursor.
    static const event_repeat_data data[] =
    {
      { "testing string :)", 8, 7, "testing :)" },
      { "abcdef", 0, 6, "" },
      { "abcdef", 1, 4, "af" },
      { "", 0, 10, "" },
      { "some stuff", 10, 10, "some stuff" },
    };

    for(const event_repeat_data &d : data)
    {
      int expected_len = strlen(d.expected);
      strcpy(dest, d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      for(int i = 0; i < d.repeat_times; i++)
      {
        result = intake_apply_event_fixed(sub, INTK_DELETE, intk.pos, 0, nullptr);
        ASSERTEQX(result, true, d.expected);
      }
      ASSERTCMP(dest, d.expected);
      ASSERTEQ(intk.current_length, expected_len);
    }
  }

  SECTION(INTK_BACKSPACE)
  {
    // Delete the char before the cursor.
    static const event_repeat_data data[] =
    {
      { "testing string :)", 14, 7, "testing :)" },
      { "abcdef", 6, 6, "" },
      { "abcdef", 5, 4, "af" },
      { "", 0, 10, "" },
      { "some stuff", 0, 10, "some stuff" },
    };

    for(const event_repeat_data &d : data)
    {
      int expected_len = strlen(d.expected);
      strcpy(dest, d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      for(int i = 0; i < d.repeat_times; i++)
      {
        result = intake_apply_event_fixed(sub, INTK_BACKSPACE, intk.pos - 1, 0, nullptr);
        ASSERTEQX(result, true, d.expected);
      }
      ASSERTCMP(dest, d.expected);
      ASSERTEQ(intk.current_length, expected_len);
    }
  }

  SECTION(INTK_BACKSPACE_WORDS)
  {
    // Delete an entire word before the cursor.
    // NOTE: old intake() deletes an extra non-alphanumeric char before the
    // word, which seems like a bug and has not been replicated here.
    static const event_repeat_data data[] =
    {
      { "testing string :)", 17, 1, "testing " },
      { "testing string :)", 17, 2, "" },
      { "testing string :)", 15, 2, ":)" },
      { "testing{string}", 14, 1, "testing{}" },
      { "abc def ghi", 9, 2, "abc hi" },
      { "abcdef", 6, 1, "" },
      { "abcdef", 3, 1, "def" },
      { "", 0, 2, "" },
      { "some stuff", 0, 2, "some stuff" },
      { "whatever", 8, 0, "whatever" },
      { "whatever", 4, -1, "whatever" },
    };

    for(const event_repeat_data &d : data)
    {
      int expected_len = strlen(d.expected);
      strcpy(dest, d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      result = intake_apply_event_fixed(sub, INTK_BACKSPACE_WORDS, intk.pos, d.repeat_times, nullptr);
      ASSERTEQX(result, true, d.expected);
      ASSERTCMP(dest, d.expected);
      ASSERTEQ(intk.current_length, expected_len);
    }
  }

  SECTION(INTK_CLEAR)
  {
    static const event_repeat_data data[] =
    {
      { "testing string :)", 17, 1, nullptr },
      { "testing string :)", 10, 1, nullptr },
      { "testing string :)", 0, 1, nullptr },
      { "abc def ghi", 9, 1, nullptr },
      { "abcdef", 6, 1, nullptr },
      { "abcdef", 3, 1, nullptr },
      { "", 0, 1, nullptr },
    };

    for(const event_repeat_data &d : data)
    {
      snprintf(buf, arraysize(buf), "%s @ %d", d.base, d.start_pos);
      strcpy(dest, d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      result = intake_apply_event_fixed(sub, INTK_CLEAR, 0, 0, nullptr);
      ASSERTEQX(result, true, buf);
      ASSERTXCMP(dest, "", buf);
      ASSERTEQX(intk.current_length, 0, buf);
    }
  }

  SECTION(INTK_INSERT_BLOCK)
  {
    for(const event_input_data &d : insert_data)
    {
      strcpy(dest, d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      int len = strlen(d.input);
      result = intake_apply_event_fixed(sub, INTK_INSERT_BLOCK, intk.pos + len, len, d.input);
      ASSERTEQX(result, true, d.expected);
      ASSERTCMP(dest, d.expected);
    }
  }

  SECTION(INTK_OVERWRITE_BLOCK)
  {
    for(const event_input_data &d : overwrite_data)
    {
      strcpy(dest, d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      int len = strlen(d.input);
      result = intake_apply_event_fixed(sub, INTK_OVERWRITE_BLOCK, intk.pos + len, len, d.input);
      ASSERTEQX(result, true, d.expected);
      ASSERTCMP(dest, d.expected);
    }
  }
}

/**
 * Just make sure that the event callback is called and that the buffer (if
 * present) and intk->pos have not been modified at the time the callback is
 * called.
 */
UNITTEST(EventCallback)
{
  SECTION(intake_event_ext_no_dest)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(intake_event_ext_dest)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(intake_input_string)
  {
    UNIMPLEMENTED(); // FIXME
  }
}
