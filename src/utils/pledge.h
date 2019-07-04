/**
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

// Common code for util pledge/unveil support to reduce copypaste.
#ifndef UTILS_PLEDGE_H
#define UTILS_PLEDGE_H

#ifdef CONFIG_PLEDGE

#ifdef __UTIL_H
#error "Include pledge.h before util.h!"
#endif

#include <unistd.h>
#include <sys/param.h>
// These macros conflict with internally-defined MZX macros
#undef MIN
#undef MAX

// unveil added in OpenBSD 6.3
#if defined(OpenBSD) && OpenBSD >= 201811
#define HAS_UNVEIL
#endif

#endif /* CONFIG_PLEDGE */
#endif /* UTILS_PLEDGE_H */
