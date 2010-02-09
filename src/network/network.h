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

#ifndef __NETWORK_H
#define __NETWORK_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../configure.h"

#ifdef CONFIG_NETWORK

CORE_LIBSPEC bool network_layer_init(struct config_info *conf);
CORE_LIBSPEC void network_layer_exit(struct config_info *conf);

#else /* !CONFIG_NETWORK */

static inline bool network_layer_init(struct config_info *conf)
{
  return true;
}

static inline void network_layer_exit(struct config_info *conf) {}

#endif /* CONFIG_NETWORK */

__M_END_DECLS

#endif // __NETWORK_H
