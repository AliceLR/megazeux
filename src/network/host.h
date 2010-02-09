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

#ifndef __HOST_H
#define __HOST_H

#include "compat.h"

__M_BEGIN_DECLS

struct host;

typedef enum
{
  /** Prefer IPv4 address resolution for hostnames */
  HOST_FAMILY_IPV4,
  /** Prefer IPv6 address resolution for hostnames */
  HOST_FAMILY_IPV6,
}
host_family_t;

typedef enum
{
  /** Use TCP protocol for sockets */
  HOST_TYPE_TCP,
  /** Use UDP protocol for sockets */
  HOST_TYPE_UDP,
}
host_type_t;

#ifdef WIN32

/**
 * Initializes the host layer. Must be called before all other host functions.
 *
 * @return Whether initialization succeeded, or not
 */
bool host_layer_init(void);

/**
 * Shuts down the host layer.
 * Frees any associated operating system resources.
 */
void host_layer_exit(void);

#else

static inline bool host_layer_init(void) { return true; }
static inline void host_layer_exit(void) { }

#endif

/**
 * Some socket operations "fail" for non-fatal reasons. If using non-blocking
 * sockets, they may fail with EAGAIN, EINTR, or in some other
 * platform-specific way, if there was no data available at that time.
 *
 * @return Whether the last socket error was fatal, or not
 */
bool host_last_error_fatal(void);

/**
 * Creates a host for use either as a client or a server.
 *
 * @param proto    The IP protocol, typically IPPROTO_TCP or IPPROTO_UDP
 * @param af       Address family of socket, typically AF_INET or AF_INET6
 * @param blocking Whether or not the socket should block for I/O
 *
 * @return A host object that can be passed to other network functions,
 *         or NULL if an error occurred.
 */
struct host *host_create(host_type_t type, host_family_t fam, bool blocking);

/**
 * Destroys a host created by @ref host_create (this closes the socket and
 * frees the associated object).
 *
 * @param h Host to destroy.
 */
void host_destroy(struct host *h);

/**
 * Accepts a connection from a host processed by \ref host_bind and
 * \ref host_listen previously. The new connection is assigned a new
 * host data structure which must be freed.
 *
 * @param s Serving host to accept connection with
 *
 * @return Connected client connection, or NULL if a failure occurred
 */
struct host *host_accept(struct host *s);

/**
 * Binds a host `h' to the specified host and port.
 *
 * @param hostname Hostname or IP address to bind to
 * @param port     Target port (service) to use
 *
 * @return Whether the bind was possible and successful, or not
 */
bool host_bind(struct host *h, const char *hostname, int port);

/**
 * Connects a host `h' to the specified host and port.
 *
 * @param hostname Hostname or IP address to connect to
 * @param port     Target port (service) to use
 *
 * @return Whether the connection was possible and successful, or not
 */
bool host_connect(struct host *h, const char *hostname, int port);

/**
 * Prepares a socket processed with @ref host_bind to listen for
 * connections. Only applies to @ref HOST_TYPE_TCP sockets.
 *
 * @param h Host to start listening with
 *
 * @return Whether it was possible to listen with this host, or not
 */
bool host_listen(struct host *h);

/**
 * Obtain a buffer containing a file, referenced by URL, over HTTP.
 *
 * @param h             Host to converse in HTTP over
 * @param file          HTTP URL to transfer
 * @param expected_type MIME type to expect in response
 * @param file_len      Length of response in bytes
 *
 * @return The file contents as a character buffer. This buffer can be
 *         freed using the C library free() function. It is always NULL
 *         terminated (not included in `file_len').
 */
char *host_get_file(struct host *h, const char *file,
 const char *expected_type, unsigned long *file_len);

__M_END_DECLS

#endif // __HOST_H
