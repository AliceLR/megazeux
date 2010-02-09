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
#include "manifest.h"

#include "const.h"
#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <errno.h>

#define OUTBOUND_PORT 80

static bool check_prune_basedir(const char *file)
{
  char path[MAX_PATH];
  int ret;

  ret = get_path(file, path, MAX_PATH);
  if(ret < 0)
  {
    warning("Path too long\n");
    return false;
  }

  // This file has no base directory
  if(ret == 0)
    return true;

  // At the head of the recursion we remove the directory
  rmdir(path);

  // Recursion; remove any parent directory
  return check_prune_basedir(path);
}

static bool check_create_basedir(const char *file)
{
  char path[MAX_PATH];
  struct stat s;
  int ret;

  ret = get_path(file, path, MAX_PATH);
  if(ret < 0)
  {
    warning("Path too long\n");
    return false;
  }

  // This file has no base directory
  if(ret == 0)
    return true;

  if(stat(path, &s) < 0)
  {
    // Every other kind of error is fatal
    if(errno != ENOENT)
    {
      perror("stat");
      return false;
    }

    // Recursion; create any parent directory
    if(!check_create_basedir(path))
      return false;

    // At the tail of the recursion we create the directory
    mkdir(path, 0777);
    return true;
  }

  if(!S_ISDIR(s.st_mode))
  {
    warning("Path '%s' is getting in the way (must be a directory)\n", path);
    return false;
  }

  return true;
}

int main(int argc, char *argv[])
{
  struct manifest_entry *removed, *replaced, *added, *e;
  const char *basedir = "/2.82/windows-x86";
  struct host *h;

  if(!host_layer_init())
  {
    warning("Error initializing socket layer!\n");
    goto exit_out;
  }

  h = host_create(HOST_TYPE_TCP, HOST_FAMILY_IPV4);
  if(!h)
  {
    warning("Error creating host for outgoing data.\n");
    goto exit_socket_layer;
  }

  if(!host_connect(h, "updates.mzx.devzero.co.uk", OUTBOUND_PORT))
    goto exit_host_destroy;

  chdir("282");

  if(!manifest_get_updates(h, basedir, &removed, &replaced, &added))
  {
    warning("Failed to compute update manifests; aborting\n");
    goto exit_host_destroy;
  }

  for(e = removed; e; e = e->next)
  {
    if(unlink(e->name))
    {
      perror("unlink");
      goto err_free_update_manifests;
    }

    /* Obtain the path for this file. If the file isn't at the top
     * level, and the directory is empty (rmdir ensures this)
     * the directory will be pruned.
     */
    check_prune_basedir(e->name);
  }

  for(e = added; e; e = e->next)
  {
    if(!check_create_basedir(e->name))
      goto err_free_update_manifests;

    if(!manifest_entry_download_replace(h, basedir, e))
      goto err_free_update_manifests;
  }

  for(e = replaced; e; e = e->next)
  {
    if(!check_create_basedir(e->name))
      goto err_free_update_manifests;

    if(!manifest_entry_download_replace(h, basedir, e))
      goto err_free_update_manifests;
  }

  chdir("..");

err_free_update_manifests:
  manifest_list_free(&removed);
  manifest_list_free(&replaced);
  manifest_list_free(&added);
exit_host_destroy:
  host_destroy(h);
exit_socket_layer:
  host_layer_exit();
exit_out:
  return 0;
}
