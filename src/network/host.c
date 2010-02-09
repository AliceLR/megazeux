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
#include "host.h"

#include "util.h"

#include <sys/time.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#ifdef __WIN32__
// Windows XP WinSock2 needed for getaddrinfo() API
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#else // !__WIN32__
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <fcntl.h>
#endif // __WIN32__

#include "zlib.h"

#define BLOCK_SIZE    4096UL
#define LINE_BUF_LEN  256

struct host
{
  const char *name;
  int proto;
  int af;
  int fd;
};

static inline bool __host_last_error_fatal(void)
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

#ifndef __WIN32__

bool host_last_error_fatal(void)
{
  return __host_last_error_fatal();
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

#else // __WIN32__

/* MegaZeux needs ws2_32.dll for getaddrinfo() and friends, which are
 * relatively new POSIX APIs added to simplify the lookup of (and
 * association with) a host. They passively support IPv6 lookups,
 * for example, and can deal with DNS extensions like SRV for
 * protocol specific resolves. This is useful stuff for the future.
 *
 * Therefore, it was decided to rely on Winsock2, specifically the
 * version introduced in Windows XP. If the new XP-added functions cannot
 * be loaded, the old gethostbyname() method will be wrapped to produce
 * an interface identical to that of getaddrinfo(). This allows us to
 * support all Winsock 2.0 platforms, which was officially 98 onwards,
 * but 95 was also supported with an additional download.
 */

#include "SDL.h" // for LoadObject/LoadFunction

static struct
{
  /* These are Winsock 2.0 functions that should be present all the
   * way back to 98 (and 95 with the additional Winsock 2.0 update).
   */
  WINSOCK_API_LINKAGE int PASCAL
   (*accept)(SOCKET, const struct sockaddr *, int *);
  WINSOCK_API_LINKAGE int PASCAL
   (*bind)(SOCKET, const struct sockaddr *, int);
  WINSOCK_API_LINKAGE int PASCAL (*closesocket)(SOCKET);
  WINSOCK_API_LINKAGE int PASCAL
   (*connect)(SOCKET, const struct sockaddr *, int);
  WINSOCK_API_LINKAGE DECLARE_STDCALL_P(struct hostent *)
   (*gethostbyname)(const char*);
  WINSOCK_API_LINKAGE u_short PASCAL (*htons)(u_short);
  WINSOCK_API_LINKAGE int PASCAL (*ioctlsocket)(SOCKET, long, u_long *);
  WINSOCK_API_LINKAGE int PASCAL (*listen)(SOCKET, int);
  WINSOCK_API_LINKAGE int PASCAL
   (*select)(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
  WINSOCK_API_LINKAGE int PASCAL (*send)(SOCKET, const char *, int, int);
  WINSOCK_API_LINKAGE int PASCAL
   (*setsockopt)(SOCKET, int, int, const char *, int);
  WINSOCK_API_LINKAGE SOCKET PASCAL (*socket)(int, int, int);
  WINSOCK_API_LINKAGE int PASCAL (*recv)(SOCKET, char *, int, int);

  // Similar to above but these are extensions of Berkeley sockets
  WINSOCK_API_LINKAGE int PASCAL (*WSACancelBlockingCall)(void);
  WINSOCK_API_LINKAGE int PASCAL (*WSACleanup)(void);
  WINSOCK_API_LINKAGE int PASCAL (*WSAGetLastError)(void);
  WINSOCK_API_LINKAGE int PASCAL (*WSAStartup)(WORD, LPWSADATA);
  int PASCAL (*__WSAFDIsSet)(SOCKET, fd_set *);

  // These functions were only implemented as of Windows XP (5.1)
  void WSAAPI (*freeaddrinfo)(struct addrinfo *);
  int WSAAPI (*getaddrinfo)(const char *, const char *,
   const struct addrinfo *, struct addrinfo **);

  void *handle;
}
socksyms;

static const struct
{
  const char *name;
  void **sym_ptr;
}
socksyms_map[] =
{
  { "accept",                (void **)&socksyms.accept },
  { "bind",                  (void **)&socksyms.bind },
  { "closesocket",           (void **)&socksyms.closesocket },
  { "connect",               (void **)&socksyms.connect },
  { "gethostbyname",         (void **)&socksyms.gethostbyname },
  { "htons",                 (void **)&socksyms.htons },
  { "ioctlsocket",           (void **)&socksyms.ioctlsocket },
  { "listen",                (void **)&socksyms.listen },
  { "select",                (void **)&socksyms.select },
  { "send",                  (void **)&socksyms.send },
  { "setsockopt",            (void **)&socksyms.setsockopt },
  { "socket",                (void **)&socksyms.socket },
  { "recv",                  (void **)&socksyms.recv },

  { "WSACancelBlockingCall", (void **)&socksyms.WSACancelBlockingCall },
  { "WSACleanup",            (void **)&socksyms.WSACleanup },
  { "WSAGetLastError",       (void **)&socksyms.WSAGetLastError },
  { "WSAStartup",            (void **)&socksyms.WSAStartup },
  { "__WSAFDIsSet",          (void **)&socksyms.__WSAFDIsSet },

  { "freeaddrinfo",          (void **)&socksyms.freeaddrinfo },
  { "getaddrinfo",           (void **)&socksyms.getaddrinfo },

  { NULL, NULL }
};

typedef int sockaddr_t;

static int init_ref_count;

#undef  FD_ISSET
#define FD_ISSET(fd,set) socksyms.__WSAFDIsSet((SOCKET)(fd),(fd_set *)(set))

#define WINSOCK2 "ws2_32.dll"
#define WINSOCK  "wsock32.dll"

static void socket_free_syms(void)
{
  if(socksyms.handle)
  {
    SDL_UnloadObject(socksyms.handle);
    socksyms.handle = NULL;
  }
}

static bool socket_load_syms(void)
{
  int i;

  socksyms.handle = SDL_LoadObject(WINSOCK2);
  if(!socksyms.handle)
  {
    warning("Failed to load Winsock 2.0, falling back to Winsock\n");
    socksyms.handle = SDL_LoadObject(WINSOCK);
    if(!socksyms.handle)
    {
      warning("Failed to load Winsock fallback\n");
      return false;
    }
  }

  for(i = 0; socksyms_map[i].name; i++)
  {
    *socksyms_map[i].sym_ptr =
     SDL_LoadFunction(socksyms.handle, socksyms_map[i].name);

    if(!*socksyms_map[i].sym_ptr)
    {
      // Skip these NT 5.1 WS2 extensions; we can fall back
      if((strcmp(socksyms_map[i].name, "freeaddrinfo") == 0) ||
         (strcmp(socksyms_map[i].name, "getaddrinfo") == 0))
        continue;

      // However all other Winsock symbols must be loaded, or we fail hard
      warning("Failed to load Winsock symbol '%s'\n", socksyms_map[i].name);
      socket_free_syms();
      return false;
    }
  }

  return true;
}

bool host_layer_init(void)
{
  WORD version = MAKEWORD(1, 0);
  WSADATA ws_data;

  if(init_ref_count == 0)
  {
    if(!socket_load_syms())
      return false;

    if(socksyms.WSAStartup(version, &ws_data) < 0)
      return false;
  }

  init_ref_count++;
  return true;
}

void host_layer_exit(void)
{
  assert(init_ref_count > 0);
  init_ref_count--;

  if(init_ref_count != 0)
    return;

  if(socksyms.WSACleanup() == SOCKET_ERROR)
  {
    if(socksyms.WSAGetLastError() == WSAEINPROGRESS)
    {
      socksyms.WSACancelBlockingCall();
      socksyms.WSACleanup();
    }
  }

  socket_free_syms();
}

bool host_last_error_fatal(void)
{
  if(socksyms.WSAGetLastError() == WSAEWOULDBLOCK)
    return false;
  return __host_last_error_fatal();
}

static inline int platform_accept(int sockfd,
 struct sockaddr *addr, sockaddr_t *addrlen)
{
  return socksyms.accept(sockfd, addr, addrlen);
}

static inline int platform_bind(int sockfd,
 const struct sockaddr *addr, sockaddr_t addrlen)
{
  return socksyms.bind(sockfd, addr, addrlen);
}

static inline void platform_close(int fd)
{
  socksyms.closesocket(fd);
}

static inline int platform_connect(int sockfd,
 const struct sockaddr *serv_addr, sockaddr_t addrlen)
{
  return socksyms.connect(sockfd, serv_addr, addrlen);
}

static inline struct hostent *platform_gethostbyname(const char *name)
{
  return socksyms.gethostbyname(name);
}

static inline uint16_t platform_htons(uint16_t hostshort)
{
  return socksyms.htons(hostshort);
}

static int getaddrinfo_legacy_wrapper(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  struct addrinfo *r, *r_head = NULL;
  struct hostent *hostent;
  int i;

  if(hints->ai_family != AF_INET)
    return EAI_FAMILY;

  hostent = platform_gethostbyname(node);
  if(hostent->h_addrtype != AF_INET)
    return EAI_NONAME;

  /* Walk the h_addr_list and create faked addrinfo structures
   * corresponding to the addresses. We don't support non-IPV4
   * addresses or any other magic, but that's an acceptable
   * fallback, and it won't affect users on XP or newer.
   */
  for(i = 0; hostent->h_addr_list[i]; i++)
  {
    struct sockaddr_in *addr;

    if(r_head)
    {
      r->ai_next = malloc(sizeof(struct addrinfo));
      r = r->ai_next;
    }
    else
      r_head = r = malloc(sizeof(struct addrinfo));

    // Zero the fake addrinfo and fill out the essential fields
    memset(r, 0, sizeof(struct addrinfo));
    r->ai_family = hints->ai_family;
    r->ai_socktype = hints->ai_socktype;
    r->ai_protocol = hints->ai_protocol;
    r->ai_addrlen = sizeof(struct sockaddr_in);
    r->ai_addr = malloc(r->ai_addrlen);

    // Zero the fake ipv4 addr and fill our all of the fields
    addr = (struct sockaddr_in *)r->ai_addr;
    memcpy(&addr->sin_addr.s_addr, hostent->h_addr_list[i], sizeof(uint32_t));
    addr->sin_family = r->ai_family;
    addr->sin_port = platform_htons(atoi(service));
  }

  *res = r_head;
  return 0;
}

static void freeaddrinfo_legacy_wrapper(struct addrinfo *res)
{
  struct addrinfo *r, *r_next;
  for(r = res; r; r = r_next)
  {
    r_next = r->ai_next;
    free(r->ai_addr);
    free(r);
  }
}

static inline void platform_freeaddrinfo(struct addrinfo *res)
{
  if(socksyms.freeaddrinfo)
    socksyms.freeaddrinfo(res);
  else
    freeaddrinfo_legacy_wrapper(res);
}

static inline int platform_getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  if(socksyms.getaddrinfo)
    return socksyms.getaddrinfo(node, service, hints, res);
  return getaddrinfo_legacy_wrapper(node, service, hints, res);
}

static inline int platform_listen(int sockfd, int backlog)
{
  return socksyms.listen(sockfd, backlog);
}

static inline int platform_select(int nfds, fd_set *readfds,
 fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  return socksyms.select(nfds, readfds, writefds, exceptfds, timeout);
}

static inline ssize_t platform_send(int s, const void *buf, size_t len,
 int flags)
{
  return socksyms.send(s, buf, len, flags);
}

static inline int platform_setsockopt(int s, int level, int optname,
 const void *optval, socklen_t optlen)
{
  return socksyms.setsockopt(s, level, optname, optval, optlen);
}

static inline int platform_socket(int af, int type, int protocol)
{
  return socksyms.socket(af, type, protocol);
}

static inline void platform_socket_blocking(int s, bool blocking)
{
  unsigned long mode = !blocking;
  socksyms.ioctlsocket(s, FIONBIO, &mode);
}

static inline ssize_t platform_recv(int s, void *buf, size_t len, int flags)
{
  return socksyms.recv(s, buf, len, flags);
}

#endif // __WIN32__

void host_blocking(struct host *h, bool blocking)
{
  platform_socket_blocking(h->fd, blocking);
}

struct host *host_create(host_type_t type, host_family_t fam)
{
  struct linger linger = { .l_onoff = 1, .l_linger = 30 };
  int err, fd, af, proto;
  const uint32_t on = 1;
  struct host *h;

  switch(fam)
  {
    case HOST_FAMILY_IPV6:
      af = AF_INET6;
      break;

    default:
      af = AF_INET;
      break;
  }

  switch(type)
  {
    case HOST_TYPE_UDP:
      fd = platform_socket(af, SOCK_DGRAM, IPPROTO_UDP);
      proto = IPPROTO_UDP;
      break;

    default:
      fd = platform_socket(af, SOCK_STREAM, IPPROTO_TCP);
      proto = IPPROTO_TCP;
      break;
  }

  if(fd < 0)
  {
    perror("socket");
    return NULL;
  }

  /* We need to turn off bind address checking, allowing
   * port numbers to be reused; otherwise, TIME_WAIT will
   * delay binding to these ports for 2 * MSL seconds.
   */
  err = platform_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&on, 4);
  if(err < 0)
  {
    perror("setsockopt1");
    return NULL;
  }

  /* When connection is closed, we want to "linger" to
   * make sure everything is transmitted. Enable this
   * feature here. However, we can only do this on TCP
   * sockets, as some platforms (winsock) don't like
   * modifying linger on UDP sockets.
   */
  if(proto == IPPROTO_TCP)
  {
    err = platform_setsockopt(fd, SOL_SOCKET, SO_LINGER,
     (void *)&linger, sizeof(struct linger));
    if(err < 0)
    {
      perror("setsockopt2");
      return NULL;
    }
  }

  // Initially the socket is blocking
  platform_socket_blocking(fd, true);

  // Create our "host" abstraction (latterly augmented)
  h = malloc(sizeof(struct host));
  memset(h, 0, sizeof(struct host));
  h->proto = proto;
  h->af = af;
  h->fd = fd;
  return h;
}

void host_destroy(struct host *h)
{
  platform_close(h->fd);
  free(h);
}

static bool __send(int fd, const void *buffer, unsigned int len)
{
  struct timeval start, now;
  const char *buf = buffer;
  unsigned int pos;
  bool ret = true;
  int count;

  gettimeofday(&start, NULL);

  // send stuff in blocks, incrementally
  for(pos = 0; pos < len; pos += count)
  {
    gettimeofday(&now, NULL);

    // time out in 10 seconds if no data is sent
    if(((now.tv_sec - start.tv_sec) * 1000 +
        (now.tv_usec - start.tv_usec) / 1000) > 10 * 1000)
    {
      ret = false;
      goto exit_out;
    }

    // normally it won't all get through at once
    count = platform_send(fd, &buf[pos], len - pos, 0);
    if(count < 0)
    {
      // non-blocking socket, so can fail and still be ok
      if(!host_last_error_fatal())
      {
        count = 0;
        continue;
      }

      ret = false;
      goto exit_out;
   }
 }

exit_out:
  return ret;
}

static bool __recv(int fd, void *buffer, unsigned int len)
{
  struct timeval start, now;
  char *buf = buffer;
  unsigned int pos;
  bool ret = true;
  int count;

  gettimeofday(&start, NULL);

  // receive stuff in blocks, incrementally
  for(pos = 0; pos < len; pos += count)
  {
    gettimeofday(&now, NULL);

    // time out in 10 seconds if no data is received
    if(((now.tv_sec - start.tv_sec) * 1000 +
        (now.tv_usec - start.tv_usec) / 1000) > 10 * 1000)
    {
      ret = false;
      goto exit_out;
    }

    // normally it won't all get through at once
    count = platform_recv(fd, &buf[pos], len - pos, 0);
    if(count < 0)
    {
      // non-blocking socket, so can fail and still be ok
      if(!host_last_error_fatal())
      {
        count = 0;
        continue;
      }

      ret = false;
      goto exit_out;
    }
  }

exit_out:
  return ret;
}

struct host *host_accept(struct host *s)
{
  struct sockaddr *addr;
  struct host *c = NULL;
  socklen_t addrlen;
  int newfd;

  switch(s->af)
  {
    case AF_INET6:
      addrlen = sizeof(struct sockaddr_in6);
      break;

    default:
      addrlen = sizeof(struct sockaddr_in);
      break;
  }

  addr = malloc(addrlen);

  newfd = platform_accept(s->fd, addr, &addrlen);
  if(newfd >= 0)
  {
    assert(addr->sa_family == s->af);

    platform_socket_blocking(newfd, true);
    c = malloc(sizeof(struct host));
    c->af = addr->sa_family;
    c->proto = s->proto;
    c->name = NULL;
    c->fd = newfd;
  }
  else
  {
    if(host_last_error_fatal())
      perror("accept");
  }

  free(addr);
  return c;
}

static bool host_connect_or_bind(struct host *h, const char *hostname,
 int port, bool connect)
{
  struct addrinfo hints, *ais, *ai;
  char port_str[6];
  int ret;

  // Stringify user port for "service" argument below
  snprintf(port_str, 6, "%d", port);
  port_str[5] = 0;

  // Set some hints for the getaddrinfo() call
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = (h->proto == IPPROTO_TCP) ? SOCK_STREAM : SOCK_DGRAM;
  hints.ai_protocol = h->proto;
  hints.ai_family = h->af;

  // Look up host(s) matching hints
  ret = platform_getaddrinfo(hostname, port_str, &hints, &ais);
  if(ret != 0)
  {
    warning("Failed to look up '%s' (%s)\n", hostname, gai_strerror(ret));
    return false;
  }

  // Walk the list of hosts and attempt to connect in-order
  for(ai = ais; ai; ai = ai->ai_next)
  {
    // We can be flexible on this -- it doesn't matter
    if(ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
      continue;

    if(connect)
    {
      if(platform_connect(h->fd, ai->ai_addr, ai->ai_addrlen) < 0)
      {
        perror("connect");
        continue;
      }
    }
    else
    {
      if(platform_bind(h->fd, ai->ai_addr, ai->ai_addrlen) < 0)
      {
        perror("bind");
        continue;
      }
    }

    break;
  }

  platform_freeaddrinfo(ais);

  // None of the listed hosts (if any) were connectable
  if(!ai)
  {
    warning("No routeable host '%s' found\n", hostname);
    return false;
  }

  h->name = hostname;
  return true;
}

bool host_bind(struct host *h, const char *hostname, int port)
{
  return host_connect_or_bind(h, hostname, port, false);
}

bool host_connect(struct host *h, const char *hostname, int port)
{
  return host_connect_or_bind(h, hostname, port, true);
}

bool host_listen(struct host *h)
{
  if(platform_listen(h->fd, 0) < 0)
  {
    perror("listen");
    return false;
  }
  return true;
}

bool host_recv_raw(struct host *h, char *buffer, unsigned int len)
{
  return __recv(h->fd, buffer, len);
}

bool host_send_raw(struct host *h, const char *buffer, unsigned int len)
{
  return __send(h->fd, buffer, len);
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

static int http_recv_line(struct host *h, char *buffer, int len)
{
  int pos;

  assert(len > 0);

  // Grab data bytewise until we find CRLF characters
  for(pos = 0; pos < len; pos++)
  {
    // If recv() times out or fails, abort
    if(!__recv(h->fd, &buffer[pos], 1))
      return -HOST_RECV_FAILED;

    // Erase terminating CRLF and fix up count
    if(buffer[pos] == '\n')
    {
      buffer[pos--] = 0;
      buffer[pos] = 0;
      break;
    }
  }

  // We didn't find CRLF; this is bad
  if(pos == len)
    return -2;

  // All went well; return the line length
  return pos;
}

static int http_send_line(struct host *h, const char *message)
{
  char line[LINE_BUF_LEN];
  size_t len;

  snprintf(line, LINE_BUF_LEN, "%s\r\n", message);
  len = strlen(line);

  if(!__send(h->fd, line, len))
    return -HOST_SEND_FAILED;

  return len;
}

static bool http_skip_headers(struct host *h)
{
  char buffer[LINE_BUF_LEN];

  while(true)
  {
    int len = http_recv_line(h, buffer, LINE_BUF_LEN);

    // We read the final newline (empty, with CRLF)
    if(len == 0)
      return true;

    // Connection probably failed; fail hard
    else if(len < 0)
      return false;
  }
}

static int zlib_forge_gzip_header(char *buffer)
{
  // GZIP magic (see RFC 1952)
  buffer[0] = 0x1F;
  buffer[1] = 0x8B;
  buffer[2] = Z_DEFLATED;

  // Flags (no flags required)
  buffer[3] = 0x0;

  // Zero mtime etc.
  memset(&buffer[4], 0, 6);

  // GZIP header is 10 bytes
  return 10;
}

static int zlib_skip_gzip_header(char *initial, unsigned long len)
{
  Bytef *gzip = (Bytef *)initial;
  uint8_t flags;

  if(len < 10)
    return -HOST_ZLIB_INVALID_DATA;

  /* Unfortunately, Apache (and probably other httpds) send deflated
   * data in the gzip format. Internally, gzip is identical to zlib's
   * DEFLATE format, but it has some additional headers that must be
   * skipped before we can proceed with the inflation.
   *
   * RFC 1952 details the gzip headers.
   */
  if(*gzip++ != 0x1F || *gzip++ != 0x8B || *gzip++ != Z_DEFLATED)
    return -HOST_ZLIB_INVALID_GZIP_HEADER;

  // Grab gzip flags from header and skip MTIME+XFL+OS
  flags = *gzip++;
  gzip += 6;

  // Header "reserved" bits must not be set
  if(flags & 0xE0)
    return -HOST_ZLIB_INVALID_GZIP_HEADER;

  // Skip extended headers
  if(flags & 0x4)
    gzip += 2 + *gzip + (*(gzip + 1) << 8);

  // Skip filename (null terminated string)
  if(flags & 0x8)
    while(*gzip++);

  // Skip comment
  if(flags & 0x10)
    while(*gzip++);

  // FIXME: Skip CRC16 (should verify if exists)
  if(flags & 0x2)
    gzip += 2;

  // Return number of bytes to skip from buffer
  return gzip - (Bytef *)initial;
}

host_status_t host_recv_file(struct host *h, const char *url,
 FILE *file, const char *expected_type)
{
  bool mid_inflate = false, mid_chunk = false, deflated = false;
  unsigned int content_length = 0;
  unsigned long len = 0, pos = 0;
  char line[LINE_BUF_LEN];
  z_stream stream;
  size_t line_len;

  enum {
    NONE,
    NORMAL,
    CHUNKED,
  } transfer_type = NONE;

  // Tell the server that we support pipelining
  snprintf(line, LINE_BUF_LEN, "GET %s HTTP/1.1", url);
  line[LINE_BUF_LEN - 1] = 0;
  if(http_send_line(h, line) < 0)
    return -HOST_SEND_FAILED;

  // For vhost resolution
  snprintf(line, LINE_BUF_LEN, "Host: %s", h->name);
  line[LINE_BUF_LEN - 1] = 0;
  if(http_send_line(h, line) < 0)
    return -HOST_SEND_FAILED;

  // We support DEFLATE/GZIP payloads
  if(http_send_line(h, "Accept-Encoding: gzip") < 0)
    return -HOST_SEND_FAILED;

  // Black line tells server we are done
  if(http_send_line(h, "") < 0)
    return -HOST_SEND_FAILED;

  // Read in the HTTP status line
  line_len = http_recv_line(h, line, LINE_BUF_LEN);

  /* These two conditionals check the status line is formatted:
   *   "HTTP/1.? 200 OK" (where ? is anything)
   *
   * This is because partially HTTP/1.1 capable servers supporting
   * pipelining may not advertise full HTTP/1.1 compliance (e.g.
   * `perlbal' httpd).
   *
   * MegaZeux only cares about pipelining.
   */
  if(line_len != 15 ||
   strncmp(line, "HTTP/1.", 7) != 0 ||
   strcmp(&line[7 + 1], " 200 OK") != 0)
    return -HOST_HTTP_INVALID_STATUS;

  // Now parse the HTTP headers, extracting only the pertinent fields

  while(true)
  {
    int len = http_recv_line(h, line, LINE_BUF_LEN);
    char *key, *value, *buf = line;

    if(len < 0)
      return -HOST_HTTP_INVALID_HEADER;
    else if(len == 0)
      break;

    key = strsep(&buf, ":");
    value = strsep(&buf, ":");

    if(!key || !value)
      return -HOST_HTTP_INVALID_HEADER;

    // Skip common prefix space if present
    if(value[0] == ' ')
      value++;

    /* Parse pertinent headers. These are:
     *
     *   Content-Length     Necessary to determine payload length
     *   Transfer-Encoding  Instead of Content-Length, can only be "chunked"
     *   Content-Type       Text or binary; also used for sanity checks
     *   Content-Encoding   Present and set to 'gzip' if deflated
     */

    if(strcmp(key, "Content-Length") == 0)
    {
      char *endptr;

      content_length = (unsigned int)strtoul(value, &endptr, 10);
      if(endptr[0])
        return -HOST_HTTP_INVALID_CONTENT_LENGTH;

      transfer_type = NORMAL;
    }

    else if(strcmp(key, "Transfer-Encoding") == 0)
    {
      if(strcmp(value, "chunked") != 0)
        return -HOST_HTTP_INVALID_TRANSFER_ENCODING;

      transfer_type = CHUNKED;
    }

    else if(strcmp(key, "Content-Type") == 0)
    {
      if(strcmp(value, expected_type) != 0)
        return -HOST_HTTP_INVALID_CONTENT_TYPE;
    }

    else if(strcmp(key, "Content-Encoding") == 0)
    {
      if(strcmp(value, "gzip") != 0)
        return -HOST_HTTP_INVALID_CONTENT_ENCODING;

      deflated = true;
    }
  }

  if(transfer_type != NORMAL && transfer_type != CHUNKED)
    return -HOST_HTTP_INVALID_TRANSFER_ENCODING;

  while(true)
  {
    unsigned long block_size;
    char block[BLOCK_SIZE];

    /* Both transfer mechanisms need preambles. For NORMAL, this will
     * happen only once, because we have a predetermined length for
     * transfer. However, for CHUNKED we don't know the total payload
     * size, so this will be invoked each time we exhaust a chunk.
     *
     * The CHUNKED handling basically involves chopping away the
     * headers and determining the next chunk size.
     */
    if(!mid_chunk)
    {
      if(transfer_type == NORMAL)
        len = content_length;

      else if(transfer_type == CHUNKED)
      {
        char *endptr, *length, *buf = line;

        // Get a chunk_length;parameters formatted line (CRLF terminated)
        if(http_recv_line(h, line, LINE_BUF_LEN) <= 0)
          return -HOST_HTTP_INVALID_CHUNK_LENGTH;

        // HTTP 1.1 says we can ignore trailing parameters
        length = strsep(&buf, ";");
        if(!length)
          return -HOST_HTTP_INVALID_CHUNK_LENGTH;

        // Convert hex length to unsigned long; check for conversion errors
        len = strtoul(length, &endptr, 16);
        if(endptr[0])
          return -HOST_HTTP_INVALID_CHUNK_LENGTH;
      }

      mid_chunk = true;
      pos = 0;
    }

    /* For NORMAL transfers, this indicates that there was a zero byte
     * payload. This is unusual but we can handle it safely by aborting.
     *
     * For CHUNKED transfers, zero indicates that there are no more chunks
     * to process, and that final footer handling should occur. We then
     * abort as with NORMAL.
     */
    if(len == 0)
    {
      if(transfer_type == CHUNKED)
        if(!http_skip_headers(h))
          return -HOST_HTTP_INVALID_HEADER;
      break;
    }

    /* For a NORMAL transfer, the block_size computation should yield
     * BLOCK_SIZE until the final block, which will be len % BLOCK_SIZE.
     *
     * For CHUNKED, this block_size can be more volatile. In most cases it
     * will be BLOCK_SIZE if chunk size > BLOCK_SIZE, until the final block.
     *
     * However for very small chunks (which are unlikely) this will always
     * be shorter than BLOCK_SIZE.
     */
    block_size = MIN(BLOCK_SIZE, len - pos);

    /* In either case, all headers and block computation has now been done,
     * and the buffer can be streamed to disk.
     */
    if(!__recv(h->fd, block, block_size))
      return -HOST_RECV_FAILED;

    if(deflated)
    {
      /* This is the first block requiring inflation. In this case, we must
       * parse the GZIP header in order to compute an offset to the DEFLATE
       * formatted data.
       */
      if(!mid_inflate)
      {
        int deflate_offset = 0;

        /* Compute the offset within this block to begin the inflation
         * process. For all but the first block, deflate_offset will be
         * zero.
         */
        deflate_offset = zlib_skip_gzip_header(block, block_size);
        if(deflate_offset < 0)
          return deflate_offset;

        /* Now we can initialize the decompressor. Pass along the block
         * without the GZIP header (and for a GZIP, this is also without
         * the DEFLATE header too, which is what the -MAX_WBITS trick is for).
         */
        stream.avail_in = block_size - (unsigned long)deflate_offset;
        stream.next_in = (Bytef *)&block[deflate_offset];
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;

        if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
          return -HOST_ZLIB_INFLATE_FAILED;

        mid_inflate = true;
      }
      else
      {
        stream.avail_in = block_size;
        stream.next_in = (Bytef *)block;
      }

      while(true)
      {
        char outbuf[BLOCK_SIZE];
        int ret;

        // Each pass, only decompress a maximum of BLOCK_SIZE
        stream.avail_out = BLOCK_SIZE;
        stream.next_out = (Bytef *)outbuf;

        /* Perform the inflation (this will modify avail_in and
         * next_in automatically.
         */
        ret = inflate(&stream, Z_NO_FLUSH);
        if(ret != Z_OK && ret != Z_STREAM_END)
          return -HOST_ZLIB_INFLATE_FAILED;

        // Push the block to disk
        if(fwrite(outbuf, BLOCK_SIZE - stream.avail_out, 1, file) != 1)
          return -HOST_FWRITE_FAILED;

        // If the stream has terminated, flag it and break out
        if(ret == Z_STREAM_END)
        {
          mid_inflate = false;
          break;
        }

        /* The stream hasn't terminated but we've exhausted input
         * data for this pass.
         */
        if(stream.avail_in == 0)
          break;
      }

      // The stream terminated, so we should free associated data-structures
      if(!mid_inflate)
        inflateEnd(&stream);
    }
    else
    {
      /* If the transfer is not deflated, we can simply write out
       * block_size bytes to the file now.
       */
      if(fwrite(block, block_size, 1, file) != 1)
        return -HOST_FWRITE_FAILED;
    }

    pos += block_size;

    /* For NORMAL transfers we can now abort since we have reached the end
     * of our payload.
     *
     * For CHUNKED transfers, we remove the trailing newline and flag that
     * a new set of chunk headers should be read.
     */
    if(len == pos)
    {
      if(transfer_type == NORMAL)
        break;

      else if(transfer_type == CHUNKED)
      {
        if(http_recv_line(h, line, LINE_BUF_LEN) != 0)
          return -HOST_HTTP_INVALID_HEADER;
        mid_chunk = false;
      }
    }
  }

  return HOST_SUCCESS;
}

host_status_t host_send_file(struct host *h, FILE *file, const char *mime_type)
{
  bool mid_deflate = false;
  char line[LINE_BUF_LEN];
  uint32_t crc, uSize;
  z_stream stream;
  long size;

  // Tell the client that we're going to use HTTP/1.1 features
  if(http_send_line(h, "HTTP/1.1 200 OK") < 0)
    return -HOST_SEND_FAILED;

  /* To bring ourselves into complete HTTP 1.1 compliance, send
   * some headers that we know our client doesn't actually need.
   */
  if(http_send_line(h, "Accept-Ranges: bytes") < 0)
    return -HOST_SEND_FAILED;
  if(http_send_line(h, "Vary: Accept-Encoding") < 0)
    return -HOST_SEND_FAILED;

  // Always zlib deflate content; keeps code simple
  if(http_send_line(h, "Content-Encoding: gzip") < 0)
    return -HOST_SEND_FAILED;

  // We'll just send everything chunked, unconditionally
  if(http_send_line(h, "Transfer-Encoding: chunked") < 0)
    return -HOST_SEND_FAILED;

  // Pass along a type hint for the client (mandatory sanity check for MZX)
  snprintf(line, LINE_BUF_LEN, "Content-Type: %s", mime_type);
  line[LINE_BUF_LEN - 1] = 0;
  if(http_send_line(h, line) < 0)
    return -HOST_SEND_FAILED;

  // Terminate the headers with a blank line
  if(http_send_line(h, "") < 0)
    return -HOST_SEND_FAILED;

  // Initialize CRC for GZIP footer
  crc = crc32(0L, Z_NULL, 0);

  // Record uncompressed size for GZIP footer
  size = ftell_and_rewind(file);
  uSize = (uint32_t)size;
  if(size < 0)
    return -HOST_FREAD_FAILED;

  while(true)
  {
    int deflate_offset = 0, ret = Z_OK, deflate_flag = Z_SYNC_FLUSH;
    char block[BLOCK_SIZE], zblock[BLOCK_SIZE];
    size_t block_size;

    /* Read a block from the disk source and compute a CRC32 on the
     * fly. This CRC will be dumped at the end of the deflated data
     * and is required for RFC 1952 compliancy.
     */
    block_size = fread(block, 1, BLOCK_SIZE, file);
    crc = crc32(crc, (Bytef *)block, block_size);

    /* The fread() above pretty much guarantees that block_size will
     * be BLOCK_SIZE up to the final block. However, fread() also
     * returns a short count if there was an I/O error, and we must
     * detect this. If it's legitimately the final block, give the
     * compressor this information.
     */
    if(block_size != BLOCK_SIZE)
    {
      if(!feof(file))
        return -HOST_FREAD_FAILED;
      deflate_flag = Z_FINISH;
    }

    /* We exhausted input at a block boundary. This is unlikely,
     * but in the event that it happens we can simply ignore
     * the deflate stage and write out the terminal chunk signature.
     */
    if(block_size == 0)
      break;

    /* Regardless of whether we are initializing the compressor in
     * the next section or not, we always have BLOCK_SIZE aligned
     * input data available.
     */
    stream.avail_in = block_size;
    stream.next_in = (Bytef *)block;

    if(!mid_deflate)
    {
      deflate_offset = zlib_forge_gzip_header(zblock);

      stream.avail_out = BLOCK_SIZE - (unsigned long)deflate_offset;
      stream.next_out = (Bytef *)&zblock[deflate_offset];
      stream.zalloc = Z_NULL;
      stream.zfree = Z_NULL;
      stream.opaque = Z_NULL;

      ret = deflateInit2(&stream, Z_BEST_COMPRESSION,
       Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
      if(ret != Z_OK)
        return -HOST_ZLIB_DEFLATE_FAILED;

      mid_deflate = true;
    }
    else
    {
      stream.avail_out = BLOCK_SIZE;
      stream.next_out = (Bytef *)zblock;
    }

    while(true)
    {
      unsigned long chunk_size;

      // Deflate a chunk of the input (partially or fully)
      ret = deflate(&stream, deflate_flag);
      if(ret != Z_OK && ret != Z_STREAM_END)
        return -HOST_ZLIB_INFLATE_FAILED;

      // Compute chunk length (final chunk includes GZIP footer)
      chunk_size = BLOCK_SIZE - stream.avail_out;
      if(ret == Z_STREAM_END)
        chunk_size += 2 * sizeof(uint32_t);

      // Dump compressed chunk length
      snprintf(line, LINE_BUF_LEN, "%lx", chunk_size);
      if(http_send_line(h, line) < 0)
        return -HOST_SEND_FAILED;

      // Send the compressed output block over the socket
      if(!__send(h->fd, zblock, BLOCK_SIZE - stream.avail_out))
        return -HOST_SEND_FAILED;

      /* We might not have finished the entire stream, but the
       * available input is likely to have been exhausted. With
       * Z_SYNC_FLUSH this will commonly result in zero bytes
       * remaining in the input source. Additionally, if this is
       * the final chunk (and Z_FINISH was flagged), Z_STREAM_END
       * will be set. In either case, we must break out.
       */
      if((ret == Z_OK && stream.avail_in == 0) || ret == Z_STREAM_END)
        break;

      // Output has been flushed; start over for the remaining input (if any)
      stream.avail_out = BLOCK_SIZE;
      stream.next_out = (Bytef *)zblock;
    }

    /* Z_FINISH was flagged, stream ended
     * Terminate compression
     */
    if(ret == Z_STREAM_END)
    {
      // Free any zlib allocated resources
      deflateEnd(&stream);

      // Write out GZIP `CRC32' footer
      if(!__send(h->fd, &crc, sizeof(uint32_t)))
        return -HOST_SEND_FAILED;

      // Write out GZIP `ISIZE' footer
      if(!__send(h->fd, &uSize, sizeof(uint32_t)))
        return -HOST_SEND_FAILED;

      mid_deflate = false;
    }

    // Newline after chunk's data
    if(http_send_line(h, "") < 0)
      return -HOST_SEND_FAILED;

    // Final block; can break out
    if(block_size != BLOCK_SIZE)
      break;
  }

  // Terminal chunk signature, so called "trailer"
  if(http_send_line(h, "0") < 0)
    return -HOST_SEND_FAILED;

  // Post-trailer newline
  if(http_send_line(h, "") < 0)
    return -HOST_SEND_FAILED;

  return HOST_SUCCESS;
}

static const char resp_404[] =
 "<html>"
  "<head><title>404</title></head>"
  "<body><pre>404 ;-(</pre></body>"
 "</html>";

bool host_handle_http_request(struct host *h)
{
  const char *mime_type = "application/octet-stream";
  char buffer[LINE_BUF_LEN], *buf = buffer;
  char *cmd_type, *path, *proto;
  host_status_t ret;
  size_t path_len;
  FILE *f;

  if(http_recv_line(h, buffer, LINE_BUF_LEN) < 0)
  {
    warning("Failed to receive HTTP request\n");
    return false;
  }

  cmd_type = strsep(&buf, " ");
  if(!cmd_type)
    return false;

  if(strcmp(cmd_type, "GET") != 0)
    return false;

  path = strsep(&buf, " ");
  if(!path)
    return false;

  proto = strsep(&buf, " ");
  if(!proto)
    return false;

  if(strncmp("HTTP/1.1", proto, 8) != 0)
  {
    warning("Client must support HTTP 1.1, rejecting\n");
    return false;
  }

  if(!http_skip_headers(h))
  {
    warning("Failed to skip HTTP headers\n");
    return false;
  }

  path++;
  debug("Received request for '%s'\n", path);

  f = fopen(path, "rb");
  if(!f)
  {
    warning("Failed to open file '%s', sending 404\n", path);

    snprintf(buffer, LINE_BUF_LEN,
     "HTTP/1.1 404 Not Found\r\n"
     "Content-Length: %zd\r\n"
     "Content-Type: text/html\r\n"
     "Connection: close\r\n\r\n", strlen(resp_404));

    if(!__send(h->fd, buffer, strlen(buffer)))
    {
      warning("Failed to send 404 status code\n");
      return false;
    }

    if(!__send(h->fd, resp_404, strlen(resp_404)))
    {
      warning("Failed to send 404 payload\n");
      return false;
    }

    return false;
  }

  path_len = strlen(path);
  if(path_len >= 4 && strcasecmp(&path[path_len - 4], ".txt") == 0)
    mime_type = "text/plain";

  ret = host_send_file(h, f, mime_type);
  if(ret != HOST_SUCCESS)
  {
    warning("Failed to send file '%s' over HTTP (error %d)\n", path, ret);
    return false;
  }

  return true;
}
