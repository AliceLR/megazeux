/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "editor_syms.h"
#include "util.h"

#include <string.h>

// for 'macros' below
#include "editor/robo_ed.h"

#if defined(CONFIG_MODULAR) && defined(CONFIG_SDL)

#include "SDL_loadso.h"

static const struct
{
  const char *name;
  void **sym_ptr;
}
editor_sym_map[] =
{
  { "edit_world", (void **)&editor_syms.edit_world },
  { "init_macros", (void **)&editor_syms.init_macros },
  { "free_extended_macros", (void **)&editor_syms.free_extended_macros },
  { NULL, NULL }
};

#if defined(__WIN32__)
#define EDITOR_DYLIB "mzxedit.dll"
#elif defined(__APPLE__)
#define EDITOR_DYLIB "libmzxedit.dylib"
#else
#define EDITOR_DYLIB "libmzxedit.so"
#endif

static int editor_init_syms(void)
{
  int i;

  editor_syms.handle = SDL_LoadObject(EDITOR_DYLIB);
  if(!editor_syms.handle)
  {
    debug("Failed to load '" EDITOR_DYLIB "'.\n");
    return false;
  }

  for(i = 0; editor_sym_map[i].name; i++)
  {
    *editor_sym_map[i].sym_ptr =
     SDL_LoadFunction(editor_syms.handle, editor_sym_map[i].name);

    if(!*editor_sym_map[i].sym_ptr)
    {
      debug("Failed to load symbol '%s'.\n", editor_sym_map[i].name);
      SDL_UnloadObject(editor_syms.handle);
      editor_syms.handle = NULL;
      return false;
    }
  }

  return true;
}

#else // !CONFIG_SDL

#include "editor/edit.h"
#include "editor/macro.h"

/* For non-SDL builds we don't support dyloading the editor; assume for now
 * that if CONFIG_EDITOR on a !CONFIG_SDL build, that the editor was compiled
 * in. That means we can just access the symbols directly, forge handle, and
 * indicate to main() that the editor symbols were found.
 *
 * This code is also used if CONFIG_MODULAR is not defined, as this means the
 * editor symbols are being linked in regardless of whether SDL is used.
 */
static int editor_init_syms(void)
{
  editor_syms.handle = (void *)1;
  editor_syms.edit_world = edit_world;
  editor_syms.init_macros = init_macros;
  editor_syms.free_extended_macros = free_extended_macros;
  return true;
}

#endif // CONFIG_SDL

int editor_init_hook(World *mzx_world)
{
  if(!editor_init_syms())
    return false;
  editor_syms.init_macros(mzx_world);
  return true;
}

void editor_free_hook(World *mzx_world)
{
  if(editor_syms.handle)
    editor_syms.free_extended_macros(mzx_world);
}
