/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __HOST_HPP
#define __HOST_HPP

#include "../compat.h"
#include "../configure.h"
#include "../platform.h"

#include <stdio.h> // for FILE

enum host_family
{
  /** Prefer IPv4 address resolution for hostnames */
  HOST_FAMILY_IPV4,
  /** Prefer IPv6 address resolution for hostnames */
  HOST_FAMILY_IPV6,
};

enum host_type
{
  /** Use TCP protocol for sockets */
  HOST_TYPE_TCP,
  /** Use UDP protocol for sockets */
  HOST_TYPE_UDP,
};

enum proxy_status
{
  PROXY_SUCCESS,
  PROXY_CONNECTION_FAILED,
  PROXY_AUTH_FAILED,
  PROXY_AUTH_UNSUPPORTED,
  PROXY_SEND_ERROR,
  PROXY_HANDSHAKE_FAILED,
  PROXY_REFLECTION_FAILED,
  PROXY_TARGET_REFUSED,
  PROXY_ADDRESS_TYPE_UNSUPPORTED,
  PROXY_ACCESS_DENIED,
  PROXY_UNKNOWN_ERROR
};

/**
 * Base class for network connections.
 * Most uses will want to use a specialized host class instead (see HTTPHost).
 */
class UPDATER_LIBSPEC Host
{
private:
  enum host_state
  {
    HOST_UNINITIALIZED, // No socket or connection.
    HOST_INITIALIZED,   // Has a socket, but no connection.
    HOST_CONNECTED,     // Has a connection.
    HOST_BOUND,         // Is bound to a port.
  };
  enum host_state state;
  enum host_type type;
  enum host_family preferred_family;
  int preferred_af;
  int socktype;
  int proto;

  // TODO send_callback
  void (*receive_callback)(long offset);
  boolean (*cancel_callback)(void);

  const char *name = nullptr;
  const char *endpoint = nullptr;
  boolean proxied;
  int af;
  int sockfd;
  Uint32 timeout_ms;

  boolean create_socket(enum host_type type, enum host_family family);
  boolean address_op(const char *hostname, int port, void *priv,
   struct addrinfo *(Host::*op)(struct addrinfo *ais, void *priv));

  boolean create_connection(struct addrinfo *ai, enum host_family family);
  struct addrinfo *create_connection_op(struct addrinfo *ais, void *priv);
  boolean connect_direct(const char *hostname, int port);
  enum proxy_status connect_socks4a(const char *hostname, int port);
  enum proxy_status connect_socks4(struct addrinfo *ai);
  enum proxy_status connect_socks5(struct addrinfo *ai);
  enum proxy_status connect_proxy(const char *hostname, int port);

public:
  /**
   * Default time to wait for requests before aborting.
   */
  const int TIMEOUT_DEFAULT = (10 * 1000);

  /**
   * Initializes the host layer. Must be called before all other host functions.
   *
   * @return Whether initialization succeeded, or not
   */
  static boolean host_layer_init(struct config_info *conf);

  /**
   * Shuts down the host layer.
   * Frees any associated operating system resources.
   */
  static void host_layer_exit(void);

  /**
   * Initialize a host object.
   *
   * @param type     IP protocol to use (HOST_TYPE_TCP or HOST_TYPE_UDP).
   * @param family   Preferred address family (HOST_FAMILY_IPV4 or HOST_FAMILY_IPV6).
   *                 Any resulting connection will not necessarily use this.
   */
  Host(enum host_type type, enum host_family family);

  /**
   * Destructor (close the socket and free any data/resources if needed).
   */
  ~Host();

  /**
   * Resets this host object to its initial state.
   * Closes the connection (if connected) and releases the socket (if created).
   */
  void close();

  /**
   * Connects to the specified remote host and port.
   *
   * @param hostname Remote hostname or IP address to connect to.
   * @param port     Target port (service) to use.
   *
   * @return `true` on a successful connection, otherwise `false`.
   */
  boolean connect(const char *hostname, int port);

  /**
   * Sets the timeout for sending and receiving packets (default is 10s).
   *
   * @param timeout_ms  Timeout (in ms).
   */
  void set_timeout_ms(Uint32 timeout_ms);

  /**
   * Set send/recv callbacks which will be called (potentially many times) as
   * the library fills the send/recv buffers for "block transfers". HTTP headers
   * and other preambles are explicitly ignored. Raw transfers are always fully
   * accounted. This code is primarily used by UI widgets like progress meters.
   *
   * Additionally, include a callback for cancellation. This will cause the
   * send/recv functions to fail and subsequently abort the transfer process.
   * This is again useful for cancelling long or slow transfers with UI widgets.
   *
   * Please note that this code does not abstract away protocol specific
   * knowledge. For example, CHUNKED HTTP transfers will appear as many
   * incrementally filled, fixed size buffers. Therefore, you should not use
   * `len' below to compute the transfer amount remaining. If you do not have
   * any expectations about transfer size, these callbacks may in fact not be
   * very useful.
   *
   * TODO maybe replace this?
   *
   * @param send_cb   Implementation of a send callback (or NULL for none)
   * @param recv_cb   Implementation of a recv callback (or NULL for none)
   * @param cancel_cb Implementation of a cancel callback (or NULL for none)
   */
  void set_callbacks( void (*send_cb)(long offset), void (*recv_cb)(long offset),
   boolean (*cancel_cb)(void));

  /**
   * Some socket operations "fail" for non-fatal reasons. If using non-blocking
   * sockets, they may fail with EAGAIN, EINTR, or in some other
   * platform-specific way, if there was no data available at that time.
   *
   * @return `true` if the last socket error was fatal, otherwise `false`.
   */
  boolean is_last_error_fatal();

  /**
   * Sets the socket blocking mode on the given host.
   * Should only be used after connect() or bind().
   *
   * @param blocking `true` if the socket should block, `false` otherwise.
   */
  void set_blocking(boolean blocking);

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
  struct host *accept();

#ifdef NETWORK_DEADCODE
  // FIXME
  /**
   * Binds the managed host to the specified host and port.
   *
   * @param hostname Hostname or IP address to bind to
   * @param port     Target port (service) to use
   *
   * @return `true` if the bind was successful, otherwise `false`.
   */
  boolean bind(const char *hostname, int port);

  /**
   * Prepares a socket processed with HostServer::bind to listen for
   * connections. Only applies if the managed socket is HOST_TYPE_TCP.
   *
   * @return `true` if listening is successful, otherwise `false`.
   */
  boolean listen();
#endif

protected:
  /**
   * Receive a buffer on a host via raw socket access.
   *
   * @param buffer    Buffer to receive data into (pre-allocated).
   * @param len       Amount of data to receive into the buffer.
   *
   * @return `true` if the buffer was received, otherwise `false`.
   */
  boolean receive(void *buffer, size_t len);

  /**
   * Send a buffer on a host via raw socket access.
   *
   * @param buffer    Buffer to send from (pre-allocated).
   * @param len       Length of data to send.
   *
   * @return `true` if the buffer was sent, otherwise `false`.
   */
  boolean send(const void *buffer, size_t len);
};

#ifdef NETWORK_DEADCODE

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

// FIXME: Document
boolean host_recvfrom_raw(struct host *h, char *buffer,
 unsigned int len, const char *hostname, int port);

// FIXME: Document
boolean host_sendto_raw(struct host *h, const char *buffer,
 unsigned int len, const char *hostname, int port);

/**
 * Stream a file from disk to a network socket.
 *
 * @param h           Host to converse in HTTP with
 * @param file        File to stream to socket (must already exist)
 * @param mime_type   MIME type of payload
 *
 * @return See \ref host_status_t.
 */
enum host_status host_send_file(struct host *h, FILE *file,
 const char *mime_type);

// FIXME: Document?
boolean host_handle_http_request(struct host *h);

#endif // NETWORK_DEADCODE

#endif /* __HOST_HPP */
