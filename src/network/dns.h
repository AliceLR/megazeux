/* MegaZeux
 *
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __DNS_H
#define __DNS_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "socksyms.h"

int dns_getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res, Uint32 timeout);

bool dns_init(struct config_info *conf);
void dns_exit(void);

__M_END_DECLS

#endif /* __DNS_H */
