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

struct int_pairs
{
  int input;
  int expected;
};

/**
 * Test internal and external functions for manipulating the position and
 * current length of the intake editor. A large portion of this is making sure
 * the external position and length pointers are syncronized correctly.
 */
UNITTEST(PosLength)
{
  static const struct int_pairs pos_data[] =
  {
    { 0, 0 },
    { 50, 50 },
    { 100, 100 },
    { 1000000, 100 },
    { -1, 0 },
    { -12781831, 0 },
  };
  static const struct int_pairs length_data[] =
  {
    { 0, 0 },
    { 50, 50 },
    { 240, 240 },
    { 1000, 240 },
    { 1000000, 240 },
    { -1, 0 },
    { -4983412, 0 },
  };

  struct intake_subcontext intk{};
  char dest[256] = "Test string.";
  char buf[80];
  int ext = 0;

  intk.current_length = 100;
  intk.max_length = 240;

  SECTION(intake_set_pos_no_external)
  {
    for(const struct int_pairs &d : pos_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_pos(&intk, d.input);
      ASSERTEQX(intk.pos, d.expected, buf);
    }
  }

  SECTION(intake_set_pos_external)
  {
    intk.pos_external = &ext;

    for(const struct int_pairs &d : pos_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_pos(&intk, d.input);
      ASSERTEQX(intk.pos, d.expected, buf);
      ASSERTEQX(ext, d.expected, buf);
    }
  }

  SECTION(intake_set_length_no_external)
  {
    for(const struct int_pairs &d : length_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_length(&intk, d.input);
      ASSERTEQX(intk.current_length, d.expected, buf);
    }
  }

  SECTION(intake_set_length_external)
  {
    intk.length_external = &ext;

    for(const struct int_pairs &d : length_data)
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
    for(const struct int_pairs &d : pos_data)
    {
      snprintf(buf, arraysize(buf), "%d -> %d", d.input, d.expected);
      intake_set_pos(&intk, d.input);
      intake_sync((subcontext *)&intk);
      ASSERTEQX(intk.pos, d.expected, buf);
    }

    for(const struct int_pairs &d : length_data)
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

    for(const struct int_pairs &d : pos_data)
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

    for(const struct int_pairs &d : length_data)
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

    for(const struct int_pairs &d : length_data)
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

    for(const struct int_pairs &d : length_data)
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
  struct intake_subcontext intk{};
  char dest[256] = "Test string.";
//  char buf[80];

  intk.max_length = 240;
  intk.current_length = 100;

  SECTION(intake_skip_back)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(intake_skip_forward)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(intake_apply_event_fixed_errs)
  {
    // Should not crash with a null intake.
    boolean r = intake_apply_event_fixed(nullptr, INTK_MOVE, 0, 0, nullptr);
    ASSERTEQ(r, false);

    // Should not crash with a null dest.
    intk.dest = nullptr;
    r = intake_apply_event_fixed((subcontext *)&intk, INTK_MOVE, 0, 0, nullptr);
    ASSERTEQ(r, false);
    intk.dest = dest;

    // Should not crash if intk->pos is somehow invalid.
    int invalid_pos[] = { -1, -12431, intk.current_length + 1, 100000 };
    for(int i : invalid_pos)
    {
      intk.pos = i;
      r = intake_apply_event_fixed((subcontext *)&intk, INTK_MOVE, 0, 0, nullptr);
      ASSERTEQ(r, false);
    }
  }

  SECTION(INTK_NO_EVENT)
  {
    // Nothing should ever actually send this event (if it does, it's a bug).
    boolean r = intake_apply_event_fixed((subcontext *)&intk, INTK_NO_EVENT, 0, 0, nullptr);
    ASSERTEQ(r, false);
  }

  SECTION(INTK_MOVE)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(INTK_MOVE_WORDS)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(INTK_INSERT)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(INTK_OVERWRITE)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(INTK_DELETE)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(INTK_BACKSPACE)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(INTK_BACKSPACE_WORDS)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(INTK_CLEAR)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(INTK_INSERT_BLOCK)
  {
    UNIMPLEMENTED(); // FIXME
  }

  SECTION(INTK_OVERWRITE_BLOCK)
  {
    UNIMPLEMENTED(); // FIXME
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
}
