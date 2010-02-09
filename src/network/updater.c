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

#include "util.h"

#include <unistd.h>

#define OUTBOUND_PORT 80

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

  h = host_create(HOST_TYPE_TCP, HOST_FAMILY_IPV4, true);
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
  }

  for(e = added; e; e = e->next)
    if(!manifest_entry_download_replace(h, basedir, e))
      goto err_free_update_manifests;

  for(e = replaced; e; e = e->next)
    if(!manifest_entry_download_replace(h, basedir, e))
      goto err_free_update_manifests;

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
