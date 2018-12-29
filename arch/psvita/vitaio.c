/* MegaZeux
 *
 * Copyright (C) 2018 Ian Burgmyer <spectere@gmail.com>
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

#define __VITAIO_C

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "linked_list.h"
#include "vitaio.h"

#define FULL_MAX_PATH (MAX_PATH * 2 + 1)

static char root[] = ROOT_PATH;
static char cwd[MAX_PATH] = "\0";

static char* get_absolute_path(const char* path)
{
  static char full_path[FULL_MAX_PATH] = "";

  full_path[0] = '\0';
  strncpy(full_path, root, FULL_MAX_PATH);
  strncat(full_path, path, FULL_MAX_PATH);
  return full_path;
}

static char* resolve_virtual_path(const char* path)
{
  static char result[MAX_PATH];
  char *component, *data;
  size_t len;

  list path_chain;
  list_new(&path_chain);

  /* Check to see if this is an absolute or relative path. */
  if(path[0] != '/')
  {
    strncpy(result, cwd, MAX_PATH);
    strncat(result, "/", MAX_PATH);
  }
  else
  {
    result[0] = '\0';
  }

  strncat(result, path, MAX_PATH);

  /* We use strtok() mostly because we want to ignore consecutive delimiters. */
  component = strtok(result, "/");
  while(component != NULL)
  {
    /* Delete the most recent node if it's "..". It's safe to delete even
     * if the list is empty. The linked list implementation checks for
     * that condition. */
    if(strncmp(component, "..\0", 3) == 0)
    {
      list_delete_current(&path_chain);
      goto cont;
    }

    /* Ignore ".", since it represents the current directory. */
    if(strncmp(component, ".\0", 2) == 0)
      goto cont;

    /* Create a new pointer, with an extra char for the null terminator. */
    list_insert_last(&path_chain);
    list_get_last(&path_chain);
    len = strlen(component) + 1;
    path_chain.current->data = malloc(len * sizeof(char));
    strncpy(path_chain.current->data, component, len);

cont:
    /* Advance to the next token. */
    component = strtok(NULL, "/");
  }

  /* Put together the completed path and clean up the string pointers at
   * the same time. */
  result[0] = '\0';
  LIST_ITERATE(data, &path_chain)
  {
    strncat(result, "/", MAX_PATH);
    strncat(result, data, MAX_PATH);
  }

  /* Delete the list and return the path. */
  list_clear(&path_chain);
  return result;
}

int vitaio_chdir(const char *path)
{
  DIR *dir;

  /* Attempt to open the directory to see if it exists and is valid.
   * As an added bonus, this will set errno to a reasonable value for us. */
  dir = opendir(get_absolute_path(resolve_virtual_path(path)));
  if(dir == NULL)
    return -1;

  closedir(dir);

  /* Set the current working directory appropriately and return 0. */
  strncpy(cwd, resolve_virtual_path(path), MAX_PATH);
  return 0;
}

FILE* vitaio_fopen(const char *pathname, const char *mode)
{
  return fopen(get_absolute_path(resolve_virtual_path(pathname)), mode);
}

char* vitaio_getcwd(char *buf, size_t size)
{
  size_t path_len = strlen(cwd);

  /* POSIX requires us to check the length of the size parameter (including
   * the trailing null), set errno to ERANGE if the buffer length is
   * insufficient, and return NULL.
   *
   * POSIX.1-2001: If buf is NULL and size == 0, ignore this and allocate
   * the buffer. */
  if(size < path_len + 1 && !(buf == NULL && size == 0))
  {
    errno = ERANGE;
    return NULL;
  }

  /* POSIX.1-2001 indicates that the buffer should be initialized if a NULL
   * buffer is passed with a size of 0. */
  if(buf == NULL && size == 0)
  {
    buf = malloc(path_len + 1);

    /* Check for an out of memory condition. malloc() will set errno if there
     * was an issue, so leave it alone and just return NULL. */
    if(buf == NULL)
      return NULL;
  }

  /* Copy the virtualized path into buf and return a pointer to buf. Add 1 to
   * the length so that strncpy null-terminates the string for us. */
  strncpy(buf, cwd, path_len + 1);
  return buf;
}

int vitaio_mkdir(const char *path, mode_t mode)
{
  /* TODO: Actually implement this. */
  errno = EROFS;
  return -1;
}

DIR* vitaio_opendir(const char *name)
{
  return opendir(get_absolute_path(resolve_virtual_path(name)));
}

int vitaio_rmdir(const char *path)
{
  /* TODO: Actually implement this. */
  errno = EROFS;
  return -1;
}

int vitaio_stat(const char *path, struct stat *buf)
{
  return stat(get_absolute_path(resolve_virtual_path(path)), buf);
}
