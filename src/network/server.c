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

#include "SDL.h"

#include <assert.h>

#define INBOUND_PORT 5656

int main(int argc, char *argv[])
{
  struct host *s, *c;

  if(!host_layer_init())
  {
    warning("Error initializing socket layer\n");
    goto exit_out;
  }

  s = host_create(HOST_TYPE_TCP, HOST_FAMILY_IPV4);
  if(!s)
  {
    warning("Error creating host for outgoing data\n");
    goto exit_socket_layer;
  }

  host_blocking(s, false);

  if(!host_bind(s, "localhost", INBOUND_PORT))
  {
    warning("Failed to bind host\n");
    goto exit_host_destroy;
  }

  if(!host_listen(s))
  {
    warning("Failed to listen with host\n");
    goto exit_host_destroy;
  }

  while(true)
  {
    c = host_accept(s);
    if(c)
    {
      while(true)
      {
        if(!host_handle_http_request(c))
        {
          warning("Failure handling HTTP request\n");
          host_destroy(c);
          break;
        }
      }
    }
    SDL_Delay(10);
  }

exit_host_destroy:
  host_destroy(s);
exit_socket_layer:
  host_layer_exit();
exit_out:
  return 0;
}
