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

#include "Host.hpp"
#include "DNS.hpp"
#include "Socket.hpp"

#include "../platform.h"
#include "../util.h"

#ifndef _MSC_VER
#include <sys/time.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Suppress unfixable sign comparison warning.
#if defined(__WIN32__) && defined(__GNUC__)
//#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#ifndef __amigaos__
#define CONFIG_IPV6
#endif

static struct config_info *conf;

int host_type_to_proto(enum host_type type)
{
  switch(type)
  {
    case HOST_TYPE_UDP:
      return IPPROTO_UDP;

    default:
      return IPPROTO_TCP;
  }
}

int host_type_to_socktype(enum host_type type)
{
  switch(type)
  {
    case HOST_TYPE_UDP:
      return SOCK_DGRAM;

    default:
      return SOCK_STREAM;
  }
}

int host_family_to_af(enum host_family family)
{
  switch(family)
  {
#ifdef CONFIG_IPV6
    case HOST_FAMILY_IPV6:
      return AF_INET6;
#endif
    default:
      return AF_INET;
  }
}

boolean Host::host_layer_init(struct config_info *in_conf)
{
  if(!Socket::init(in_conf))
    return false;

  if(!DNS::init(in_conf))
    return false;

  conf = in_conf;
  return true;
}

void Host:: host_layer_exit(void)
{
  DNS::exit();
  Socket::exit();
}

Host::Host(enum host_type type, enum host_family family)
{
  this->state = HOST_UNINITIALIZED;
  this->type = type;
  this->preferred_family = family;

  // Hints to provide to getaddrinfo.
  this->preferred_af = host_family_to_af(family);
  this->socktype = host_type_to_socktype(type);
  this->proto = host_type_to_proto(type);
}

Host::~Host()
{
  this->close();
}

/**
 * Creates a socket and initializes sockfd and af.
 * This may be called multiple times as part connect(), bind(), sendto(),
 * or recvfrom().
 *
 * @param type     IP protocol to use (HOST_TYPE_TCP or HOST_TYPE_UDP).
 * @param family   Address family to use (HOST_FAMILY_IPV4 or HOST_FAMILY_IPV6).
 *
 * @return `true` on successful socket creation, otherwise `false`.
 */
boolean Host::create_socket(enum host_type type, enum host_family family)
{
  struct linger linger = { 1, 30 };
  const uint32_t on = 1;
  int err, fd, af;

  assert(this->state == HOST_UNINITIALIZED);
  if(this->state != HOST_UNINITIALIZED)
    return false;

  af = host_family_to_af(family);

  switch(type)
  {
    case HOST_TYPE_UDP:
      fd = Socket::socket(af, SOCK_DGRAM, IPPROTO_UDP);
      break;

    default:
      fd = Socket::socket(af, SOCK_STREAM, IPPROTO_TCP);
      break;
  }

  if(fd < 0)
  {
    Socket::perror("socket");
    return false;
  }

#if defined(CONFIG_IPV6) && defined(IPV6_V6ONLY)
  if(af == AF_INET6)
  {
    const uint32_t off = 0;
    err = Socket::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&off, 4);
    if(err < 0)
    {
      Socket::perror("setsockopt(IPV6_V6ONLY)");
      goto err_close;
    }
  }
#endif

  /* We need to turn off bind address checking, allowing
   * port numbers to be reused; otherwise, TIME_WAIT will
   * delay binding to these ports for 2 * MSL seconds.
   */
  err = Socket::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&on, 4);
  if(err < 0)
  {
    Socket::perror("setsockopt(SO_REUSEADDR)");
    goto err_close;
  }

  /* When connection is closed, we want to "linger" to
   * make sure everything is transmitted. Enable this
   * feature here. However, we can only do this on TCP
   * sockets, as some platforms (winsock) don't like
   * modifying linger on UDP sockets.
   */
  if(proto == IPPROTO_TCP)
  {
    err = Socket::setsockopt(fd, SOL_SOCKET, SO_LINGER,
     (void *)&linger, sizeof(struct linger));
    if(err < 0)
    {
      Socket::perror("setsockopt(SO_LINGER)");
      goto err_close;
    }
  }

  // Initially the socket is blocking
  Socket::set_blocking(fd, true);

  this->state = HOST_INITIALIZED;
  this->af = af;
  this->sockfd = fd;
  return true;

err_close:
  Socket::close(fd);
  return false;
}

void Host::close()
{
  if(this->state != HOST_UNINITIALIZED)
  {
    Socket::close(this->sockfd);
    this->state = HOST_UNINITIALIZED;
  }
}

void Host::set_timeout_ms(Uint32 timeout_ms)
{
  this->timeout_ms = timeout_ms;
}

boolean Host::send(const void *buffer, size_t len)
{
  const char *buf = (const char *)buffer;
  Uint32 start, now;
  ssize_t count;
  size_t pos;

  start = get_ticks();

  // send stuff in blocks, incrementally
  for(pos = 0; pos < len; pos += count)
  {
    now = get_ticks();

    // time out if no data is sent
    if(now - start > this->timeout_ms)
      return false;

    // normally it won't all get through at once
    count = Socket::send(this->sockfd, buf + pos, len - pos, 0);
    if(count < 0)
    {
      // non-blocking socket, so can fail and still be ok
      if(!Socket::is_last_error_fatal())
      {
        count = 0;
        continue;
      }
      return false;
    }

    if(this->cancel_callback && this->cancel_callback())
      return false;
  }
  return true;
}

boolean Host::receive(void *buffer, size_t len)
{
  char *buf = (char *)buffer;
  Uint32 start, now;
  ssize_t count;
  size_t pos;

  start = get_ticks();

  // receive stuff in blocks, incrementally
  for(pos = 0; pos < len; pos += count)
  {
    now = get_ticks();

    // time out if no data is received
    if(now - start > this->timeout_ms)
      return false;

    // normally it won't all get through at once
    count = Socket::recv(this->sockfd, buf + pos, len - pos, 0);
    if(count < 0 && Socket::is_last_error_fatal())
    {
      return false;
    }
    else

    if(count <= 0)
    {
      // non-blocking socket, so can fail and still be ok
      // Add a short delay to prevent excessive CPU use
      delay(10);
      count = 0;
      continue;
    }

    if(this->cancel_callback && this->cancel_callback())
      return false;
  }
  return true;
}

static void reset_timeout(struct timeval *tv, Uint32 timeout)
{
  /* Because of the way select() works on Unix platforms, this needs to
   * be reset every time select() is used on it. */
  tv->tv_sec = (timeout / 1000);
  tv->tv_usec = (timeout % 1000) * 1000;
}

/**
 * Create a socket and connect to a host.
 *
 * @param ai       `addrinfo` struct for host to connect to.
 * @param family   Address family to use (HOST_FAMILY_IPV4 or HOST_FAMILY_IPV6).
 *
 * @return `true` on successful socket creation/connection, otherwise `false`.
 *         On failure, any opened socket will be closed.
 */
boolean Host::create_connection(struct addrinfo *ai, enum host_family family)
{
  struct timeval tv;
  fd_set mask;
  int res;

  if(!this->create_socket(this->type, family))
    return false;

  // Disable blocking on the socket so a timeout can be enforced.
  Socket::set_blocking(this->sockfd, false);

  Socket::connect(this->sockfd, ai->ai_addr, (socklen_t)ai->ai_addrlen);

  FD_ZERO(&mask);
  FD_SET(this->sockfd, &mask);
  reset_timeout(&tv, this->timeout_ms);

  res = Socket::select(this->sockfd + 1, NULL, &mask, NULL, &tv);
  if(res != 1)
  {
    if(res == 0)
      info("Connection timed out.\n");
    else
      Socket::perror("connect");

    Socket::close(this->sockfd);
    this->state = HOST_UNINITIALIZED;
    return false;
  }

  Socket::set_blocking(this->sockfd, true);
  return true;
}

/**
 * `address_op` callback for Host::raw_connect.
 */
struct addrinfo *Host::create_connection_op(struct addrinfo *ais, void *priv)
{
  struct addrinfo *ai;

#ifdef CONFIG_IPV6
  /* First try to connect to an IPv6 address (if any)
   */
  for(ai = ais; ai; ai = ai->ai_next)
  {
    if(ai->ai_family == AF_INET6)
    {
      if(this->create_connection(ai, HOST_FAMILY_IPV6))
        return ai;
    }
  }
#endif

  /* No IPv6 addresses could be connected; try IPv4 (if any)
   */
  for(ai = ais; ai; ai = ai->ai_next)
  {
    if(ai->ai_family == AF_INET)
    {
      if(this->create_connection(ai, HOST_FAMILY_IPV4))
        return ai;
    }
  }
  return nullptr;
}

/**
 * Perform an operation that requires the iteration of an address lookup.
 *
 * @param hostname  Host to connect to.
 * @param port      Host port to connect to.
 * @param priv      Pointer to pass on to `op`.
 * @param op        Operation to perform on the resulting `addrinfo` list.
 *
 * @return `true` if the operation completed successfully, otherwise `false`.
 */
boolean Host::address_op(const char *hostname, int port, void *priv,
 struct addrinfo *(Host::*op)(struct addrinfo *ais, void *priv))
{
  struct addrinfo hints, *ais, *ai;
  char port_str[6];
  int ret;

  // Stringify user port for "service" argument below
  snprintf(port_str, 6, "%d", port);
  port_str[5] = 0;

  // Set some hints for the getaddrinfo() call
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = this->socktype;
  hints.ai_protocol = this->proto;
  hints.ai_family = this->preferred_af;

  // Look up host(s) matching hints
  ret = DNS::lookup(hostname, port_str, &hints, &ais, this->timeout_ms);
  if(ret != 0)
  {
    warn("Failed to look up '%s' (%s)\n", hostname, Socket::getaddrinfo_error(ret));
    return false;
  }

  ai = (this->*op)(ais, priv);
  Socket::freeaddrinfo(ais);

  // None of the listed hosts (if any) were connectable
  if(!ai)
  {
    warn("No routeable host '%s' found\n", hostname);
    return false;
  }

  // TODO should duplicate this. It's safe right now since the hostname always
  // comes from the config file, but that won't always be the case.
  if(!this->name)
    this->name = hostname;

  return true;
}

boolean Host::connect_direct(const char *hostname, int port)
{
  return this->address_op(hostname, port, nullptr, Host::create_connection_op);
}

enum proxy_status Host::connect_socks4a(const char *target_host, int target_port)
{
  char handshake[8];
  int rBuf;
  target_port = Socket::htons(target_port);

  if(!this->connect_direct(conf->socks_host, conf->socks_port))
    return PROXY_CONNECTION_FAILED;

  if(!this->send("\4\1", 2))
    return PROXY_SEND_ERROR;
  if(!this->send(&target_port, 2))
    return PROXY_SEND_ERROR;
  if(!this->send("\0\0\0\1anonymous", 14))
    return PROXY_SEND_ERROR;
  if(!this->send(target_host, strlen(target_host)))
    return PROXY_SEND_ERROR;
  if(!this->send("\0", 1))
    return PROXY_SEND_ERROR;

  rBuf = this->receive(handshake, 8);
  if(rBuf == -1)
    return PROXY_CONNECTION_FAILED;
  if(handshake[1] != 90)
    return PROXY_HANDSHAKE_FAILED;
  return PROXY_SUCCESS;
}

enum proxy_status Host::connect_socks4(struct addrinfo *ai_data)
{
  char handshake[8];
  int rBuf;

  if(!this->connect_direct(conf->socks_host, conf->socks_port))
    return PROXY_CONNECTION_FAILED;

  if(!this->send("\4\1", 2))
    return PROXY_SEND_ERROR;
  if(!this->send(ai_data->ai_addr->sa_data, 6))
    return PROXY_SEND_ERROR;
  if(!this->send("anonymous\0", 10))
    return PROXY_SEND_ERROR;

  rBuf = this->receive(handshake, 8);
  if(rBuf == -1)
    return PROXY_CONNECTION_FAILED;
  if(handshake[1] != 90)
    return PROXY_HANDSHAKE_FAILED;
  return PROXY_SUCCESS;
}

enum proxy_status Host::connect_socks5(struct addrinfo *ai_data)
{
  /* This handshake is more complicated than SOCKS4 or 4a...
   * and we're also only supporting none or user/password auth.
   */

  char handshake[10];
  int rBuf;

  if(!this->connect_direct(conf->socks_host, conf->socks_port))
    return PROXY_CONNECTION_FAILED;

  // Version[0x05]|Number of auth methods|auth methods
  if(!this->send("\5\1\0", 3))
    return PROXY_SEND_ERROR;

  rBuf = this->receive(handshake, 2);
  if(rBuf == -1)
    return PROXY_CONNECTION_FAILED;
  if(handshake[0] != 0x5)
    return PROXY_HANDSHAKE_FAILED;

#ifdef CONFIG_IPV6
  if(ai_data->ai_family == AF_INET6)
    return PROXY_ADDRESS_TYPE_UNSUPPORTED;
#endif

  // Version[0x05]|Command|0x00|address type|destination|port
  if(!this->send("\5\1\0\1", 4))
    return PROXY_SEND_ERROR;
  if(!this->send(ai_data->ai_addr->sa_data+2, 4))
    return PROXY_SEND_ERROR;
  if(!this->send(ai_data->ai_addr->sa_data, 2))
    return PROXY_SEND_ERROR;

  rBuf = this->receive(handshake, 10);
  if(rBuf == -1)
    return PROXY_CONNECTION_FAILED;
  switch(handshake[1])
  {
    case 0x0:
      return PROXY_SUCCESS;
    case 0x1:
    case 0x7:
      return PROXY_UNKNOWN_ERROR;
    case 0x2:
      return PROXY_ACCESS_DENIED;
    case 0x3:
    case 0x4:
    case 0x6:
      return PROXY_REFLECTION_FAILED;
    case 0x5:
      return PROXY_TARGET_REFUSED;
    case 0x8:
      return PROXY_ADDRESS_TYPE_UNSUPPORTED;
    default:
      return PROXY_UNKNOWN_ERROR;
  }
}

// SOCKS Proxy support
enum proxy_status Host::connect_proxy(const char *target_host, int target_port)
{
  struct addrinfo target_hints, *target_ais, *target_ai;
  char port_str[6];
  int ret;

  snprintf(port_str, 6, "%d", target_port);
  port_str[5] = 0;

  memset(&target_hints, 0, sizeof(struct addrinfo));
  target_hints.ai_socktype = this->socktype;
  target_hints.ai_protocol = this->proto;
  target_hints.ai_family = this->preferred_af;
  ret = DNS::lookup(target_host, port_str, &target_hints, &target_ais,
   this->timeout_ms);

  /* Some perimeter gateways block access to DNS [wifi hotspots are
   * particularly notorious for this] so we have to fall back to SOCKS4a
   * to force the proxy to DNS the address for us. If this fails, we abort.
   */
  if(ret != 0)
    return this->connect_socks4a(target_host, target_port);

#ifdef CONFIG_IPV6
  for(target_ai = target_ais; target_ai; target_ai = target_ai->ai_next)
  {
    if(target_ai->ai_family == AF_INET6)
    {
      if(this->connect_socks5(target_ai) == PROXY_SUCCESS)
      {
        Socket::freeaddrinfo(target_ais);
        return PROXY_SUCCESS;
      }
      this->close();
    }
  }
#endif

  for(target_ai = target_ais; target_ai; target_ai = target_ai->ai_next)
  {
    if(target_ai->ai_family == AF_INET)
    {
      if(this->connect_socks5(target_ai) == PROXY_SUCCESS)
      {
        Socket::freeaddrinfo(target_ais);
        return PROXY_SUCCESS;
      }
      this->close();
      if(this->connect_socks4(target_ai) == PROXY_SUCCESS)
      {
        Socket::freeaddrinfo(target_ais);
        return PROXY_SUCCESS;
      }
      this->close();
    }
  }
  Socket::freeaddrinfo(target_ais);
  return PROXY_CONNECTION_FAILED;
}

boolean Host::connect(const char *hostname, int port)
{
  assert(this->state == HOST_UNINITIALIZED);
  if(this->state != HOST_UNINITIALIZED)
    return false;

  this->proxied = false;
  if(strlen(conf->socks_host) > 0)
  {
    if(this->connect_proxy(hostname, port) == PROXY_SUCCESS)
    {
      this->proxied = true;
      this->endpoint = hostname;
      return true;
    }
    this->close();
    return false;
  }
  return this->connect_direct(hostname, port);
}

void Host::set_callbacks(void (*send_callback)(long offset),
 void (*receive_callback)(long offset), boolean (*cancel_callback)(void))
{
  assert(send_callback == NULL);
  this->receive_callback = receive_callback;
  this->cancel_callback = cancel_callback;
}

boolean Host::is_last_error_fatal()
{
  return Socket::is_last_error_fatal();
}

void Host::set_blocking(boolean blocking)
{
  assert(this->state != HOST_UNINITIALIZED);
  if(this->state == HOST_UNINITIALIZED)
    return;

  Socket::set_blocking(this->sockfd, blocking);
}

#ifdef NETWORK_DEADCODE

struct host *host_accept(struct host *s)
{
  struct sockaddr *addr;
  struct host *c = NULL;
  socklen_t addrlen;
  int newfd;

  switch(s->af)
  {
#ifdef CONFIG_IPV6
    case AF_INET6:
      addrlen = sizeof(struct sockaddr_in6);
      break;
#endif
    default:
      addrlen = sizeof(struct sockaddr_in);
      break;
  }

  addr = cmalloc(addrlen);

  newfd = platform_accept(s->fd, addr, &addrlen);
  if(newfd >= 0)
  {
    assert(addr->sa_family == s->af);

    platform_socket_blocking(newfd, true);
    c = ccalloc(1, sizeof(struct host));
    c->af = addr->sa_family;
    c->proto = s->proto;
    c->name = NULL;
    c->fd = newfd;
  }
  else
  {
    if(platform_last_error_fatal())
      platform_perror("accept");
  }

  free(addr);
  return c;
}

static struct addrinfo *bind_op(int fd, struct addrinfo *ais, void *priv,
 Uint32 timeout)
{
  // timeout currently unimplemented
  struct addrinfo *ai;

#ifdef CONFIG_IPV6
  /* First try to bind to an IPv6 address (if any)
   */
  for(ai = ais; ai; ai = ai->ai_next)
  {
    if(ai->ai_family != AF_INET6)
      continue;

    if(platform_bind(fd, ai->ai_addr, ai->ai_addrlen) >= 0)
      break;

    platform_perror("bind");
  }

  if(ai)
    return ai;
#endif

  /* No IPv6 addresses could be bound; try IPv4 (if any)
   */
  for(ai = ais; ai; ai = ai->ai_next)
  {
    if(ai->ai_family != AF_INET)
      continue;

    if(platform_bind(fd, ai->ai_addr, ai->ai_addrlen) >= 0)
      break;

    platform_perror("bind");
  }

  return ai;
}

boolean host_bind(struct host *h, const char *hostname, int port)
{
  return host_address_op(h, hostname, port, NULL, bind_op);
}

boolean host_listen(struct host *h)
{
  if(platform_listen(h->fd, 0) < 0)
  {
    platform_perror("listen");
    return false;
  }
  return true;
}

boolean host_recv_raw(struct host *h, char *buffer, unsigned int len)
{
  return __recv(h, buffer, len);
}

boolean host_send_raw(struct host *h, const char *buffer, unsigned int len)
{
  return __send(h, buffer, len);
}

struct buf_priv_data {
  char *buffer;
  unsigned int len;
  struct host *h;
  boolean ret;
};

static struct addrinfo *recvfrom_raw_op(int fd, struct addrinfo *ais,
 void *priv, Uint32 timeout)
{
  // timeout currently unimplemented
  struct buf_priv_data *buf_priv = priv;
  unsigned int len = buf_priv->len;
  char *buffer = buf_priv->buffer;
  struct host *h = buf_priv->h;
  struct addrinfo *ai;
  ssize_t ret;

  for(ai = ais; ai; ai = ai->ai_next)
  {
    socklen_t addrlen = ai->ai_addrlen;

    if(ai->ai_family != h->af)
      continue;

    ret = platform_recvfrom(h->fd, buffer, len, 0, ai->ai_addr, &addrlen);

    if(ret < 0)
    {
      warn("Failed to recvfrom() any data.\n");
      buf_priv->ret = false;
    }
    else if((unsigned int)ret != len)
    {
      warn("Failed to recvfrom() all data (sent %zd wanted %u).\n", ret, len);
      buf_priv->ret = false;
    }

    break;
  }

  if(!ai)
    buf_priv->ret = false;

  return ai;
}

static struct addrinfo *sendto_raw_op(int fd, struct addrinfo *ais, void *priv,
 Uint32 timeout)
{
  // timeout currently unimplemented
  struct buf_priv_data *buf_priv = priv;
  unsigned int len = buf_priv->len;
  char *buffer = buf_priv->buffer;
  struct host *h = buf_priv->h;
  struct addrinfo *ai;
  ssize_t ret;

  for(ai = ais; ai; ai = ai->ai_next)
  {
    if(ai->ai_family != h->af)
      continue;

    ret = platform_sendto(h->fd, buffer, len, 0,
     ai->ai_addr, ai->ai_addrlen);

    if(ret < 0)
    {
      warn("Failed to sendto() any data.\n");
      buf_priv->ret = false;
    }
    else if((unsigned int)ret != len)
    {
      warn("Failed to sendto() all data (sent %zd wanted %u).\n", ret, len);
      buf_priv->ret = false;
    }

    break;
  }

  if(!ai)
    buf_priv->ret = false;

  return ai;
}

boolean host_recvfrom_raw(struct host *h, char *buffer, unsigned int len,
 const char *hostname, int port)
{
  struct buf_priv_data buf_priv = { buffer, len, h, true };
  host_address_op(h, hostname, port, &buf_priv, recvfrom_raw_op);
  return buf_priv.ret;
}

boolean host_sendto_raw(struct host *h, const char *buffer, unsigned int len,
 const char *hostname, int port)
{
  struct buf_priv_data buf_priv = { (char *)buffer, len, h, true };
  host_address_op(h, hostname, port, &buf_priv, sendto_raw_op);
  return buf_priv.ret;
}

int host_poll_raw(struct host *h, unsigned int timeout)
{
  struct timeval tv;
  fd_set mask;
  int ret;

  FD_ZERO(&mask);
  FD_SET(h->fd, &mask);

  tv.tv_sec  = (timeout / 1000);
  tv.tv_usec = (timeout % 1000) * 1000;

  ret = platform_select(h->fd + 1, &mask, NULL, NULL, &tv);
  if(ret < 0)
    return -1;

  if(ret > 0)
    if(FD_ISSET(h->fd, &mask))
      return 1;

  return 0;
}

#endif // NETWORK_DEADCODE
