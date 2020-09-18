/* MegaZeux
 *
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

#include "../../src/network/Socket.hpp"
#include "../../src/platform.h"
#include "../../src/util.h"

#include <network.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef IS_CXX_11
#include <type_traits>
#endif

/**
 * Wii essentially uses standard BSD socket functions with different names.
 * Note while BSD socket fd functions are compatible with other operations for
 * fds, the Wii socket functions are not. That doesn't really matter as far
 * as MegaZeux is concerned, though.
 */

/**
 * Track whether or not the internal Wii network layer is initialized.
 */
static boolean net_is_initialized = false;

/**
 * Since it seems like libogc won't set errno for anything but gethostbyname,
 * it has to be manually handled by the wrapper functions here. This is set to
 * whatever the return value of the previous socket function was. When >= 0,
 * there is no error. When negative, there was an error, which should be
 * converted to an error message using strerror_r(-net_errno).
 */
struct net_errno_pair
{
  platform_thread_id thread_id;
  int thread_errno;
};

static net_errno_pair *net_errno_list = nullptr;
static size_t net_errno_len = 0;
static size_t net_errno_alloc = 0;
static platform_mutex net_errno_lock;

static int &get_thread_net_errno()
{
  platform_thread_id current_id = platform_get_thread_id();
  size_t i = 0;

  if(net_errno_list)
  {
    for(i = 0; i < net_errno_len; i++)
    {
      net_errno_pair &p = net_errno_list[i];

      if(platform_is_same_thread(current_id, p.thread_id))
        return p.thread_errno;
    }
  }

  if(!net_errno_list || net_errno_len >= net_errno_alloc)
  {
    net_errno_alloc = net_errno_alloc ? net_errno_alloc * 2 : 4;
    net_errno_list =
     (net_errno_pair *)crealloc(net_errno_list, net_errno_alloc * sizeof(net_errno_pair));
  }

  net_errno_pair &p = net_errno_list[net_errno_len++];
  p.thread_id = current_id;
  p.thread_errno = 0;
  return p.thread_errno;
}

static int get_net_errno()
{
  platform_mutex_lock(&net_errno_lock);

  int result = get_thread_net_errno();

  platform_mutex_unlock(&net_errno_lock);
  return result;
}

static int set_net_errno(int value)
{
  /**
   * Nothing should be trying to read the errno unless something explicitly
   * returns a negative value...
   */
  if(value < 0)
  {
    platform_mutex_lock(&net_errno_lock);

    int &net_errno = get_thread_net_errno();
    net_errno = value;

    platform_mutex_unlock(&net_errno_lock);
  }
  return value;
}

boolean platform_socket_init()
{
  return true;
}

boolean platform_socket_init_late()
{
  if(!net_is_initialized)
  {
    if(!net_errno_lock)
      platform_mutex_init(&net_errno_lock);

    int res = net_init();
    set_net_errno(res);
    if(res != 0)
    {
      Socket::perror("platform_socket_init_late() -> net_init()");
      return false;
    }
  }
  net_is_initialized = true;
  return true;
}

void platform_socket_exit()
{
  if(net_is_initialized)
  {
    net_is_initialized = false;
    net_deinit();
  }
  if(net_errno_lock)
  {
    platform_mutex_destroy(&net_errno_lock);
    net_errno_lock = 0;
  }
  free(net_errno_list);
  net_errno_list = nullptr;
  net_errno_alloc = 0;
  net_errno_len = 0;
}

struct hostent *Socket::gethostbyname(const char *name)
{
  // This one actually does set errno...
  struct hostent *ret = net_gethostbyname(name);
  if(!ret)
    set_net_errno(errno);
  return ret;
}

int Socket::get_errno()
{
  int net_errno = get_net_errno();
  return net_errno < 0 ? -net_errno : 0;
}

void Socket::perror(const char *message)
{
  int net_errno = get_net_errno();
  char buf[256];
  buf[0] = '\0';

  // NOTE: return value not portable.
  strerror_r(MAX(0, -net_errno), buf, ARRAY_SIZE(buf));
  warn("--SOCKET-- %s: %s\n", message, buf);
}

int Socket::accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  int res = net_accept(sockfd, addr, addrlen);
  return set_net_errno(res);
}

int Socket::bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
  int res = net_bind(sockfd, (struct sockaddr *)addr, addrlen);
  return set_net_errno(res);
}

void Socket::close(int fd)
{
  int res = net_close(fd);
  set_net_errno(res);
}

int Socket::connect(int sockfd, const struct sockaddr *serv_addr,
 socklen_t addrlen)
{
  int res = net_connect(sockfd, const_cast<struct sockaddr *>(serv_addr), addrlen);
  return set_net_errno(res);
}

int Socket::getsockopt(int sockfd, int level, int optname, void *optval,
 socklen_t *optlen)
{
  warn("--SOCKET-- getsockopt() is not implemented by libogc.\n");
  if(level == SOL_SOCKET && optname == SO_ERROR && optval && optlen &&
   *optlen == sizeof(int))
  {
    // Just pretend everything is ok since there's no way to get the error ;-(
    int *optval_i = reinterpret_cast<int *>(optval);
    *optval_i = 0;
    return set_net_errno(0);
  }
  return set_net_errno(-EINVAL);
}

uint16_t Socket::hton_short(uint16_t hostshort)
{
  return htons(hostshort);
}

int Socket::listen(int sockfd, int backlog)
{
  int res = net_listen(sockfd, backlog);
  return set_net_errno(res);
}

int Socket::poll(struct pollfd *fds, unsigned int nfds, int timeout_ms)
{
  /**
   * libogc supports poll but uses its own struct with u32s instead of shorts
   * for some reason. Note "pollsd" instead of "pollfd". Socket.hpp defines a
   * pollfd that should be compatible with "pollsd".
   */
#ifdef IS_CXX_11
  struct pollfd tmpf;
  struct pollsd tmps;
  // Make sure pollsd wasn't modified in libogc...
  static_assert(sizeof(pollfd) == sizeof(pollsd), "");
  static_assert(sizeof(tmpf.fd) == sizeof(tmps.socket), "");
  static_assert(sizeof(tmpf.events) == sizeof(tmps.events), "");
  static_assert(sizeof(tmpf.revents) == sizeof(tmps.revents), "");
  static_assert(offsetof(pollfd, fd) == offsetof(pollsd, socket), "");
  static_assert(offsetof(pollfd, events) == offsetof(pollsd, events), "");
  static_assert(offsetof(pollfd, revents) == offsetof(pollsd, revents), "");
#endif

  int res = net_poll(reinterpret_cast<struct pollsd *>(fds), nfds, timeout_ms);
  return set_net_errno(res);
}

int Socket::select(int nfds, fd_set *readfds, fd_set *writefds,
 fd_set *exceptfds, struct timeval *timeout)
{
  warn("--SOCKET-- select() is not implemented by libogc.\n");
  int res = net_select(nfds, readfds, writefds, exceptfds, timeout);
  return set_net_errno(res);
}

ssize_t Socket::send(int sockfd, const void *buf, size_t len, int flags)
{
  int res = net_send(sockfd, buf, len, flags);
  return set_net_errno(res);
}

ssize_t Socket::sendto(int sockfd, const void *buf, size_t len, int flags,
 const struct sockaddr *to, socklen_t tolen)
{
  int res = net_sendto(sockfd, buf, len, flags, (struct sockaddr *)to, tolen);
  return set_net_errno(res);
}

int Socket::setsockopt(int sockfd, int level, int optname,
 const void *optval, socklen_t optlen)
{
  int res = net_setsockopt(sockfd, level, optname, optval, optlen);
  return set_net_errno(res);
}

int Socket::socket(int af, int type, int protocol)
{
  /**
   * Doesn't recognize IPPROTO_TCP, but IPPROTO_IP is fine... :/
   * https://github.com/devkitPro/wii-examples/blob/master/devices/network/sockettest/source/sockettest.c
   */
  if((type == SOCK_STREAM && protocol == IPPROTO_TCP) ||
   (type == SOCK_DGRAM && protocol == IPPROTO_UDP))
    protocol = IPPROTO_IP;

  int res = net_socket(af, type, protocol);
  return set_net_errno(res);
}

int Socket::recv(int sockfd, void *buf, size_t len, int flags)
{
  int res = net_recv(sockfd, buf, len, flags);
  return set_net_errno(res);
}

int Socket::recvfrom(int sockfd, void *buf, size_t len, int flags,
 struct sockaddr *from, socklen_t *fromlen)
{
  int res = net_recvfrom(sockfd, buf, len, flags, from, fromlen);
  return set_net_errno(res);
}

boolean Socket::is_last_error_fatal()
{
  return is_last_errno_fatal();
}

void Socket::set_blocking(int sockfd, boolean blocking)
{
  int flags = net_fcntl(sockfd, F_GETFL, 0);

  if(!blocking)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;

  net_fcntl(sockfd, F_SETFL, flags);
}
