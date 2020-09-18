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
#include "../../src/util.h"

#include <network.h>
#include <fcntl.h>

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
 *
 * TODO thread safety.
 */
static int net_errno = 0;

boolean platform_socket_init()
{
  return true;
}

boolean platform_socket_init_late()
{
  if(!net_is_initialized)
  {
    net_errno = net_init();
    if(net_errno != 0)
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
}

struct hostent *Socket::gethostbyname(const char *name)
{
  // This one actually does set errno...
  struct hostent *ret = net_gethostbyname(name);
  net_errno = errno;
  return ret;
}

int Socket::get_errno()
{
  return net_errno < 0 ? -net_errno : 0;
}

void Socket::perror(const char *message)
{
  char buf[256];
  buf[0] = '\0';

  // NOTE: return value not portable.
  strerror_r(MAX(0, -net_errno), buf, ARRAY_SIZE(buf));
  warn("--SOCKET-- %s: %s\n", message, buf);
}

int Socket::accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  net_errno = net_accept(sockfd, addr, addrlen);
  return net_errno;
}

int Socket::bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
  net_errno = net_bind(sockfd, (struct sockaddr *)addr, addrlen);
  return net_errno;
}

void Socket::close(int fd)
{
  net_errno = net_close(fd);
}

int Socket::connect(int sockfd, const struct sockaddr *serv_addr,
 socklen_t addrlen)
{
  net_errno = net_connect(sockfd, const_cast<struct sockaddr *>(serv_addr), addrlen);
  return net_errno;
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
    net_errno = 0;
    return 0;
  }
  net_errno = -EINVAL;
  return net_errno;
}

uint16_t Socket::hton_short(uint16_t hostshort)
{
  return htons(hostshort);
}

int Socket::listen(int sockfd, int backlog)
{
  net_errno = net_listen(sockfd, backlog);
  return net_errno;
}

int Socket::select(int nfds, fd_set *readfds, fd_set *writefds,
 fd_set *exceptfds, struct timeval *timeout)
{
  /**
   * Try actual select first. Most likely, this isn't actually implemented and
   * poll needs to be substituted for it instead. Actual select can technically
   * return -EINVAL, but this only happens if nfds or the timeout are invalid
   * (i.e. it should happen instantly).
   */
  net_errno = net_select(nfds, readfds, writefds, exceptfds, timeout);
  if(net_errno != -EINVAL)
    return net_errno;

  struct pollsd ps[FD_SETSIZE];
  int num_ps = 0;

  for(int i = 0; i < nfds; i++)
  {
    int rf = readfds ? FD_ISSET(i, readfds) : 0;
    int wf = writefds ? FD_ISSET(i, writefds) : 0;
    int ef = exceptfds ? FD_ISSET(i, exceptfds) : 0;
    if(rf || wf || ef)
    {
      if(num_ps >= FD_SETSIZE)
        return -EINVAL;

      struct pollsd &p = ps[num_ps++];

      p.socket = i;
      p.events = 0;
      p.revents = 0;

      if(rf)
        p.events |= POLLIN;
      if(wf)
        p.events |= POLLOUT;
      if(ef)
        p.events |= POLLPRI;
    }
  }

  if(readfds)
    FD_ZERO(readfds);
  if(writefds)
    FD_ZERO(writefds);
  if(exceptfds)
    FD_ZERO(exceptfds);

  int timeout_ms = (timeout->tv_sec * 1000 + timeout->tv_usec / 1000);
  net_errno = net_poll(ps, num_ps, timeout_ms);
  if(net_errno > 0)
  {
    for(int i = 0; i < num_ps; i++)
    {
      struct pollsd &p = ps[i];
      if(readfds && (p.revents & POLLIN))
        FD_SET(p.socket, readfds);
      if(writefds && (p.revents & POLLOUT))
        FD_SET(p.socket, writefds);
      if(exceptfds && (p.revents & POLLPRI))
        FD_SET(p.socket, exceptfds);
    }
  }
  return net_errno;
}

ssize_t Socket::send(int sockfd, const void *buf, size_t len, int flags)
{
  net_errno = net_send(sockfd, buf, len, flags);
  return net_errno;
}

ssize_t Socket::sendto(int sockfd, const void *buf, size_t len, int flags,
 const struct sockaddr *to, socklen_t tolen)
{
  net_errno = net_sendto(sockfd, buf, len, flags, (struct sockaddr *)to, tolen);
  return net_errno;
}

int Socket::setsockopt(int sockfd, int level, int optname,
 const void *optval, socklen_t optlen)
{
  net_errno = net_setsockopt(sockfd, level, optname, optval, optlen);
  return net_errno;
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

  net_errno = net_socket(af, type, protocol);
  return net_errno;
}

int Socket::recv(int sockfd, void *buf, size_t len, int flags)
{
  net_errno = net_recv(sockfd, buf, len, flags);
  return net_errno;
}

int Socket::recvfrom(int sockfd, void *buf, size_t len, int flags,
 struct sockaddr *from, socklen_t *fromlen)
{
  net_errno = net_recvfrom(sockfd, buf, len, flags, from, fromlen);
  return net_errno;
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
