/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef __SOCKSYMS_H
#define __SOCKSYMS_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../configure.h"

#include <errno.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef __WIN32__
#ifndef __WIN64__
// Windows XP WinSock2 needed for getaddrinfo() API
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif // !__WIN64__
#include <winsock2.h>
#include <ws2tcpip.h>
#else // !__WIN32__
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <fcntl.h>
#endif // __WIN32__

#if defined(__amigaos__)

#define EAI_NONAME -2
#define EAI_FAMILY -6

struct addrinfo
{
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  size_t ai_addrlen;
  struct sockaddr *ai_addr;
  char *ai_canonname;
  struct addrinfo *ai_next;
};

int getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
#endif

static inline bool __platform_last_error_fatal(void)
{
  switch(errno)
  {
    case 0:
    case EAGAIN:
    case EINTR:
      return false;
    default:
      return true;
  }
}

#if (defined(__GNUC__) && defined(__WIN32__)) || defined(__amigaos__)

static inline const char *platform_gai_strerror(int errcode)
{
  switch(errcode)
  {
    case EAI_NONAME:
      return "Node or service is not known.";
    case EAI_FAMILY:
      return "Address family is not supported.";
    default:
      return "Unknown error.";
  }
}

#else
static inline const char *platform_gai_strerror(int errcode)
{
  return gai_strerror(errcode);
}
#endif

#ifdef __WIN32__

bool socksyms_init(struct config_info *conf);
void socksyms_exit(void);

bool platform_last_error_fatal(void);

void platform_perror(const char *message);

int platform_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int platform_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

void platform_close(int fd);

int platform_connect(int sockfd, const struct sockaddr *serv_addr,
 socklen_t addrlen);

struct hostent *platform_gethostbyname(const char *name);

uint16_t platform_htons(uint16_t hostshort);

void platform_freeaddrinfo(struct addrinfo *res);

int platform_getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res);

int platform_listen(int sockfd, int backlog);

int platform_select(int nfds, fd_set *readfds, fd_set *writefds,
 fd_set *exceptfds, struct timeval *timeout);

ssize_t platform_send(int s, const void *buf, size_t len, int flags);

ssize_t platform_sendto(int s, const void *buf, size_t len, int flags,
 const struct sockaddr *to, socklen_t tolen);

int platform_setsockopt(int s, int level, int optname, const void *optval,
 socklen_t optlen);

int platform_socket(int af, int type, int protocol);

void platform_socket_blocking(int s, bool blocking);

ssize_t platform_recv(int s, void *buf, size_t len, int flags);

ssize_t platform_recvfrom(int s, void *buf, size_t len, int flags,
 struct sockaddr *from, socklen_t *fromlen);

#else /* !__WIN32__ */

static inline bool socksyms_init(struct config_info *conf) { return true; }
static inline void socksyms_exit(void) {}

static inline bool platform_last_error_fatal(void)
{
  return __platform_last_error_fatal();
}

static inline void platform_perror(const char *message)
{
  perror(message);
}

static inline int platform_accept(int sockfd,
 struct sockaddr *addr, socklen_t *addrlen)
{
  return accept(sockfd, addr, addrlen);
}

static inline int platform_bind(int sockfd,
 const struct sockaddr *addr, socklen_t addrlen)
{
  return bind(sockfd, addr, addrlen);
}

static inline void platform_close(int fd)
{
  close(fd);
}

static inline int platform_connect(int sockfd,
 const struct sockaddr *serv_addr, socklen_t addrlen)
{
  return connect(sockfd, serv_addr, addrlen);
}

static inline void platform_freeaddrinfo(struct addrinfo *res)
{
  freeaddrinfo(res);
}

static inline int platform_getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  return getaddrinfo(node, service, hints, res);
}

static inline struct hostent *platform_gethostbyname(const char *name)
{
  return gethostbyname(name);
}

static inline uint16_t platform_htons(uint16_t hostshort)
{
  return htons(hostshort);
}

static inline int platform_listen(int sockfd, int backlog)
{
  return listen(sockfd, backlog);
}

static inline int platform_select(int nfds, fd_set *readfds,
 fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  return select(nfds, readfds, writefds, exceptfds, timeout);
}

static inline ssize_t platform_send(int s, const void *buf, size_t len,
 int flags)
{
  return send(s, buf, len, flags);
}

static inline ssize_t platform_sendto(int s, const void *buf, size_t len,
 int flags, const struct sockaddr *to, socklen_t tolen)
{
  return sendto(s, buf, len, flags, to, tolen);
}

static inline int platform_setsockopt(int s, int level, int optname,
 const void *optval, socklen_t optlen)
{
  return setsockopt(s, level, optname, optval, optlen);
}

static inline int platform_socket(int af, int type, int protocol)
{
  return socket(af, type, protocol);
}

static inline void platform_socket_blocking(int s, bool blocking)
{
  int flags = fcntl(s, F_GETFL);

  if(!blocking)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;

  fcntl(s, F_SETFL, flags);
}

static inline ssize_t platform_recv(int s, void *buf, size_t len, int flags)
{
  return recv(s, buf, len, flags);
}

static inline ssize_t platform_recvfrom(int s, void *buf, size_t len,
 int flags, struct sockaddr *from, socklen_t *fromlen)
{
  return recvfrom(s, buf, len, flags, from, fromlen);
}

#endif // !__WIN32__

__M_END_DECLS

#endif /* __SOCKSYMS_H */
