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

#ifndef __SOCKET_HPP
#define __SOCKET_HPP

#define SKIP_SDL
#include "../compat.h"
#include "../configure.h"
#include "../platform.h"
#include "../util.h"

#include <errno.h>
#include <stdio.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef __WIN32__
#define NO_GETADDRINFO
#define UNIX_INLINE(x)
#ifndef __WIN64__
// Windows XP WinSock2 needed for getaddrinfo() API
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif // !__WIN64__
#include <winsock2.h>
#include <ws2tcpip.h>
#else // !__WIN32__
#define UNIX_INLINE(x) x
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

#define NO_GETADDRINFO
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
#endif

#ifndef NO_GETADDRINFO
#define UNIX_MAYBE_INLINE(x) UNIX_INLINE(x)
#else
#define UNIX_MAYBE_INLINE(x)
#endif

/**
 * Low-level abstraction for misc. socket symbols.
 * Don't use directly outside of src/network/.
 */
class Socket final
{
private:
  Socket() {}

public:
  static boolean init(struct config_info *conf) UNIX_INLINE({ return true; });
  static void exit(void) UNIX_INLINE({});

  static int getaddrinfo(const char *node, const char *service,
   const struct addrinfo *hints, struct addrinfo **res) UNIX_MAYBE_INLINE
  ({
    return getaddrinfo(node, service, hints, res);
  });

  static void freeaddrinfo(struct addrinfo *res) UNIX_MAYBE_INLINE
  ({
    freeaddrinfo(res);
  });

  static void getaddrinfo_perror(const char *message, int errcode) UNIX_MAYBE_INLINE
  ({
    warn("%s (code %d): %s\n", message, errcode, gai_strerror(errcode));
  });

  static void perror(const char *message) UNIX_INLINE
  ({
    perror(message);
  });

  static int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) UNIX_INLINE
  ({
    return accept(sockfd, addr, addrlen);
  });

  static int bind(int sockfd, const struct sockaddr *addr,
   socklen_t addrlen) UNIX_INLINE
  ({
    return bind(sockfd, addr, addrlen);
  });

  static void close(int fd) UNIX_INLINE
  ({
    close(fd);
  });

  static int connect(int sockfd, const struct sockaddr *serv_addr,
   socklen_t addrlen) UNIX_INLINE
  ({
    return connect(sockfd, serv_addr, addrlen);
  });

  static struct hostent *gethostbyname(const char *name) UNIX_INLINE
  ({
    return gethostbyname(name);
  });

  static uint16_t htons(uint16_t hostshort) UNIX_INLINE
  ({
    return htons(hostshort);
  });

  static int listen(int sockfd, int backlog) UNIX_INLINE
  ({
    return listen(sockfd, backlog);
  });

  static int select(int nfds, fd_set *readfds, fd_set *writefds,
   fd_set *exceptfds, struct timeval *timeout) UNIX_INLINE
  ({
    return select(nfds, readfds, writefds, exceptfds, timeout);
  });

  static ssize_t send(int sockfd, const void *buf, size_t len, int flags) UNIX_INLINE
  ({
    return send(sockfd, (const char *)buf, len, flags);
  });

  static ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
   const struct sockaddr *to, socklen_t tolen) UNIX_INLINE
  ({
    return sendto(sockfd, (const char *)buf, len, flags, to, tolen);
  });

  static int setsockopt(int sockfd, int level, int optname,
   const void *optval, socklen_t optlen) UNIX_INLINE
  ({
    return setsockopt(sockfd, level, optname, (const char *)optval, optlen);
  });

  static int socket(int af, int type, int protocol) UNIX_INLINE
  ({
    return socket(af, type, protocol);
  });

  static ssize_t recv(int sockfd, void *buf, size_t len, int flags) UNIX_INLINE
  ({
    return recv(sockfd, (char *)buf, len, flags);
  });

  static ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
   struct sockaddr *from, socklen_t *fromlen) UNIX_INLINE
  ({
    return recvfrom(sockfd, (char *)buf, len, flags, from, fromlen);
  });

#ifdef __WIN32__
  static int __WSAFDIsSet(int sockfd, fd_set *set);
#undef  FD_ISSET
#define FD_ISSET(fd,set) Socket::__WSAFDIsSet((int)fd, (fd_set *)(set))
#endif

   /**
    * These functions don't correspond to POSIX/Berkeley socket functions but
    * are provided for convenience.
    */

  static boolean is_last_error_fatal() UNIX_INLINE
  ({
    return is_last_errno_fatal();
  });

  static void set_blocking(int sockfd, boolean blocking) UNIX_INLINE
  ({
    int flags = fcntl(sockfd, F_GETFL);

    if(!blocking)
      flags |= O_NONBLOCK;
    else
      flags &= ~O_NONBLOCK;

    fcntl(sockfd, F_SETFL, flags);
  });

private:
  static boolean is_last_errno_fatal()
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
};

#endif /* __SOCKET_HPP */
