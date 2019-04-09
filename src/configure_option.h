/* MegaZeux
 *
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

// Macros/function to help reduce the number of config boilerplate functions.

#ifndef __CONFIGURE_OPTION_H
#define __CONFIGURE_OPTION_H

#include "compat.h"

__M_BEGIN_DECLS

#include "platform.h"
#include "util.h"

#include <string.h>

enum mapped_types
{
  MAPPED_INT,
  MAPPED_ENUM,
  MAPPED_STR,
  MAPPED_FUNC
};

struct mapped_enum_entry
{
  const char * const key;
  const Uint32 value;
};

struct mapped_int_value
{
  int offset;
  size_t offset_sz;
  int min;
  int max;
};

struct mapped_enum_value
{
  int offset;
  size_t offset_sz;
  const struct mapped_enum_entry *list;
  Uint8 list_len;
};

struct mapped_str_value
{
  int offset;
  size_t offset_sz;
};

struct mapped_fn_value
{
  fn_ptr fn;
};

struct mapped_field
{
  enum mapped_types type;
  union
  {
    struct mapped_int_value i;
    struct mapped_enum_value e;
    struct mapped_str_value s;
    struct mapped_fn_value f;
  } value;
};

#define OF(conf, field) offsetof(conf, field)
#define SZ(conf, field) sizeof(((conf *)0)->field)

// Map a numeric input with minimum value 'min' and maximum value 'max'.
// Values less than 'min' or greater than 'max' are ignored.
#define MAP_INT(conf, field, min, max) \
{ MAPPED_INT, { .i = { OF(conf, field), SZ(conf, field), min, max }}}

// Map a string input to a numeric value. Value is mapped from a key string in
// the mapped_enum_entry array 'v' to that string's corresponding numeric value
// (or, the value is ignored if the string is not in the array).
#define MAP_ENUM(conf, field, v) \
 { MAPPED_ENUM, { .e = { OF(conf, field), SZ(conf, field), v, ARRAY_SIZE(v) }}}

// Maps a string input to a fixed-length char buffer.
#define MAP_STR(conf, field) \
 { MAPPED_STR, { .s = { OF(conf, field), SZ(conf, field) }}}

// Stores a function pointer for custom handling of a setting.
#define MAP_FN(conf, fn) \
 { MAPPED_FUNC, { .f = { (fn_ptr)fn }}}

/**
 * Set a generic mapped setting. The mapped_field struct contains enough info
 * about the location and size of the setting that knowing the actual mapped
 * struct isn't necessary. If the option dest is a function call, return the
 * function pointer to the handler that called this.
 */
static inline fn_ptr mapped_field_set(const struct mapped_field *dest,
 void *_conf, char *name, char *value)
{
  Uint8 *conf = (Uint8 *)_conf;

  switch(dest->type)
  {
    case MAPPED_INT:
    {
      const struct mapped_int_value *v = &(dest->value.i);
      int result;
      int n;

      if(sscanf(value, "%d%n", &result, &n) != 1 || value[n] != 0)
        break;

      if(result < v->min || result > v->max)
        break;

      if(v->offset_sz == 1) *((Uint8 *)(conf + v->offset)) = (Uint8)result;
      if(v->offset_sz == 2) *((Uint16 *)(conf + v->offset)) = (Uint16)result;
      if(v->offset_sz == 4) *((int *)(conf + v->offset)) = result;
      break;
    }

    case MAPPED_ENUM:
    {
      const struct mapped_enum_value *e = &(dest->value.e);
      int result;
      Uint8 i;

      for(i = 0; i < e->list_len; i++)
      {
        if(strcasecmp(value, e->list[i].key))
          continue;

        result = e->list[i].value;
        if(e->offset_sz == 1) *((Uint8 *)(conf + e->offset)) = (Uint8)result;
        if(e->offset_sz == 2) *((Uint16 *)(conf + e->offset)) = (Uint16)result;
        if(e->offset_sz == 4) *((int *)(conf + e->offset)) = result;
        break;
      }
      break;
    }

    case MAPPED_STR:
    {
      const struct mapped_str_value *s = &(dest->value.s);
      char *dest = (char *)(conf + s->offset);
      size_t stop = s->offset_sz - 1;
      size_t i;

      for(i = 0; value[i] && (i < stop); i++)
        dest[i] = value[i];

      dest[i] = 0;
      break;
    }

    case MAPPED_FUNC:
    {
      return dest->value.f.fn;
    }
  }
  return NULL;
}

__M_END_DECLS

#endif // __CONFIGURE_OPTION_H
