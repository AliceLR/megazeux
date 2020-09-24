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

#include "network.h"
#include "../error.h"

#include "Host.hpp"

boolean network_layer_init(struct config_info *conf)
{
  if(!conf->network_enabled)
    return false;

  if(!Host::host_layer_init(conf))
  {
    error("Failed to initialize network layer.",
     ERROR_T_ERROR, ERROR_OPT_OK, 0);
    return false;
  }

  return true;
}

void network_layer_exit(struct config_info *conf)
{
  if(conf->network_enabled)
  {
    Host::host_layer_exit();
  }
}
