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

#include <network.h>
#include <fcntl.h>

/**
 * Wii essentially uses standard BSD socket functions with different names.
 * Note while BSD socket fd functions are compatible with other operations for
 * fds, the Wii socket functions are not. That doesn't really matter as far
 * as MegaZeux is concerned, though.
 */

struct hostent *Socket::gethostbyname(const char *name)
{
  return net_gethostbyname(name);
}

void Socket::perror(const char *message)
{
  ::perror(message);
}

int Socket::accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  return net_accept(sockfd, addr, addrlen);
}

int Socket::bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
  return net_bind(sockfd, (struct sockaddr *)addr, addrlen);
}

void Socket::close(int fd)
{
  net_close(fd);
}

int Socket::connect(int sockfd, const struct sockaddr *serv_addr,
 socklen_t addrlen)
{
  return net_connect(sockfd, const_cast<struct sockaddr *>(serv_addr), addrlen);
}

uint16_t Socket::hton_short(uint16_t hostshort)
{
  return htons(hostshort);
}

int Socket::listen(int sockfd, int backlog)
{
  return net_listen(sockfd, backlog);
}

int Socket::select(int nfds, fd_set *readfds, fd_set *writefds,
 fd_set *exceptfds, struct timeval *timeout)
{
  return net_select(nfds, readfds, writefds, exceptfds, timeout);
}

ssize_t Socket::send(int sockfd, const void *buf, size_t len, int flags)
{
  return net_send(sockfd, buf, len, flags);
}

ssize_t Socket::sendto(int sockfd, const void *buf, size_t len, int flags,
 const struct sockaddr *to, socklen_t tolen)
{
  return net_sendto(sockfd, buf, len, flags, (struct sockaddr *)to, tolen);
}

int Socket::setsockopt(int sockfd, int level, int optname,
 const void *optval, socklen_t optlen)
{
  return net_setsockopt(sockfd, level, optname, optval, optlen);
}

int Socket::socket(int af, int type, int protocol)
{
  return net_socket(af, type, protocol);
}

int Socket::recv(int sockfd, void *buf, size_t len, int flags)
{
  return net_recv(sockfd, buf, len, flags);
}

int Socket::recvfrom(int sockfd, void *buf, size_t len, int flags,
 struct sockaddr *from, socklen_t *fromlen)
{
  return net_recvfrom(sockfd, buf, len, flags, from, fromlen);
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
