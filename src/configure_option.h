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

enum config_option_types
{
  CONF_OPT_INT,
  CONF_OPT_ENUM,
  CONF_OPT_STR,
  CONF_OPT_FUNC
};

struct config_enum
{
  const char * const key;
  const Uint8 value;
};

struct int_value
{
  int offset;
  size_t offset_sz;
  int min;
  int max;
};

struct enum_value
{
  int offset;
  size_t offset_sz;
  const struct config_enum *list;
  Uint8 list_len;
};

struct str_value
{
  int offset;
  size_t offset_sz;
};

struct fn_value
{
  fn_ptr fn;
};

struct config_option
{
  enum config_option_types type;
  union
  {
    struct int_value i;
    struct enum_value e;
    struct str_value s;
    struct fn_value f;
  } value;
};

#define OF(conf, field) offsetof(conf, field)
#define SZ(conf, field) sizeof(((conf *)0)->field)

#define CONF_INT(conf, field, min, max) \
{ CONF_OPT_INT, { .i = { OF(conf, field), SZ(conf, field), min, max }}}

#define CONF_ENUM(conf, field, en) \
 { CONF_OPT_ENUM, { .e = { OF(conf, field), SZ(conf, field), en, ARRAY_SIZE(en) }}}

#define CONF_STR(conf, field) \
 { CONF_OPT_STR, { .s = { OF(conf, field), SZ(conf, field) }}}

#define CONF_FN(conf, fn) \
 { CONF_OPT_FUNC, { .f = { (fn_ptr)fn }}}

/**
 * Set a generic config setting. The config_option struct contains enough info
 * about the location and size of the setting that knowing the actual config
 * struct isn't necessary. If the option dest is a function call, return the
 * function pointer to the handler that called this.
 */
static inline fn_ptr config_option_set(const struct config_option *dest,
 void *_conf, char *name, char *value)
{
  Uint8 *conf = (Uint8 *)_conf;

  switch(dest->type)
  {
    case CONF_OPT_INT:
    {
      const struct int_value *v = &(dest->value.i);
      int result;
      int n;

      if(sscanf(value, "%d%n", &result, &n) != 1 || value[n] != 0)
        break;

      if(result < v->min || result > v->max)
        break;

      if(v->offset_sz == 1) *((Uint8 *)(conf + v->offset)) = (Uint8)result;
      if(v->offset_sz == 4) *((int *)(conf + v->offset)) = result;
      break;
    }

    case CONF_OPT_ENUM:
    {
      const struct enum_value *e = &(dest->value.e);
      int result;
      Uint8 i;

      for(i = 0; i < e->list_len; i++)
      {
        if(!strcasecmp(value, e->list[i].key))
        {
          result = e->list[i].value;
          if(e->offset_sz == 1) *((Uint8 *)(conf + e->offset)) = (Uint8)result;
          if(e->offset_sz == 4) *((int *)(conf + e->offset)) = result;
          break;
        }
      }
      break;
    }

    case CONF_OPT_STR:
    {
      const struct str_value *s = &(dest->value.s);
      char *dest = (char *)(conf + s->offset);
      size_t stop = s->offset_sz - 1;
      size_t i;

      for(i = 0; value[i] && (i < stop); i++)
        dest[i] = value[i];

      dest[i] = 0;
      break;
    }

    case CONF_OPT_FUNC:
    {
      return dest->value.f.fn;
    }
  }
  return NULL;
}

__M_END_DECLS

#endif // __CONFIGURE_OPTION>H
