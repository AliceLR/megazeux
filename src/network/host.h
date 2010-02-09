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

#include <stdio.h> // for FILE

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

typedef enum
{
  HOST_SUCCESS,
  HOST_FWRITE_FAILED,
  HOST_SEND_FAILED,
  HOST_RECV_FAILED,
  HOST_HTTP_INVALID_STATUS,
  HOST_HTTP_INVALID_HEADER,
  HOST_HTTP_INVALID_CONTENT_LENGTH,
  HOST_HTTP_INVALID_TRANSFER_ENCODING,
  HOST_HTTP_INVALID_CONTENT_TYPE,
  HOST_HTTP_INVALID_CONTENT_ENCODING,
  HOST_HTTP_INVALID_CHUNK_LENGTH,
  HOST_ZLIB_INVALID_DATA,
  HOST_ZLIB_INVALID_GZIP_HEADER,
  HOST_ZLIB_INFLATE_FAILED,
} host_status_t;

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
 * Sets a socket blocking mode on the given host.
 *
 * @param h        Host to alter block mode on
 * @param blocking `true' if the socket should block, `false' otherwise
 */
void host_blocking(struct host *h, bool blocking);

/**
 * Creates a host for use either as a client or a server. The new host will
 * be blocking by default.
 *
 * @param proto    The IP protocol, typically IPPROTO_TCP or IPPROTO_UDP
 * @param af       Address family of socket, typically AF_INET or AF_INET6
 *
 * @return A host object that can be passed to other network functions,
 *         or NULL if an error occurred.
 */
struct host *host_create(host_type_t type, host_family_t fam);

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
 * host data structure which must be freed. The new host will be
 * blocking by default.
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
 * Receive a buffer on a host via raw socket access.
 *
 * @param h      Host to receive buffer with
 * @param buffer Buffer to receive into (pre-allocated)
 * @param len    Length of data to receive (implicitly, length of buffer)
 *
 * @return `true' if the buffer was received, `false' if there was a
 *         communication error.
 */
bool host_recv_raw(struct host *h, char *buffer, unsigned int len);

/**
 * Send a buffer on a host via raw socket access.
 *
 * @param h      Host to send buffer with
 * @param buffer Buffer to send from (pre-allocated)
 * @param len    Length of data to send (buffer must be large enough)
 *
 * @return `true' if the buffer was sent, `false' if there was a
 *         communication error.
 */
bool host_send_raw(struct host *h, const char *buffer, unsigned int len);

/**
 * Polls a host via raw socket access.
 *
 * @param h       Host to poll socket of
 * @param timeout Timeout in milliseconds for poll
 *
 * @return <0 if there was a failure, 0 if there was no data, and the
 *         >0 if there was activity on the socket.
 */
int host_poll_raw(struct host *h, unsigned int timeout);

/**
 * Obtain a buffer containing a file, referenced by URL, over HTTP.
 *
 * @param h             Host to converse in HTTP over
 * @param url           HTTP URL to transfer
 * @param file          File to create and stream to
 * @param expected_type MIME type to expect in response
 *
 * @return See \ref host_status_t.
 */
host_status_t host_get_file(struct host *h, const char *url,
 FILE *file, const char *expected_type);

bool host_handle_http_request(struct host *h);

__M_END_DECLS

#endif // __HOST_H
