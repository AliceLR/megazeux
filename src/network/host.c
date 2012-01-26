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

#include "../platform.h"
#include "../util.h"

#ifndef _MSC_VER
#include <sys/time.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

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

// Suppress unfixable sign comparison warning.
#if defined(__WIN32__) && defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#ifdef NETWORK_DEADCODE
#define __network_maybe_static
#else
#define __network_maybe_static static
#endif

#include "zlib.h"

#define BLOCK_SIZE    4096UL
#define LINE_BUF_LEN  256

struct host
{
  void (*recv_cb)(long offset);
  bool (*cancel_cb)(void);

  const char *name;
  const char *endpoint;
  bool proxied;
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

#ifndef __amigaos__
#define CONFIG_IPV6
#endif

#if defined(__amigaos__)

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

#define EAI_NONAME -2
#define EAI_FAMILY -6

#define getaddrinfo             __getaddrinfo
#define freeaddrinfo            __freeaddrinfo

#endif

#if (defined(__GNUC__) && defined(__WIN64__)) || defined(__amigaos__)

#if defined(__amigaos__)
static
#endif
char *gai_strerror(int errcode)
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

#endif // (__GNUC__ && __WIN64__) || __amigaos__

#if defined(__WIN32__) || defined(__amigaos__)

// Forward declarations
static inline struct hostent *platform_gethostbyname(const char *name);
static inline uint16_t platform_htons(uint16_t hostshort);

static int __getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  struct addrinfo *r = NULL, *r_head = NULL;
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
      r->ai_next = cmalloc(sizeof(struct addrinfo));
      r = r->ai_next;
    }
    else
      r_head = r = cmalloc(sizeof(struct addrinfo));

    // Zero the fake addrinfo and fill out the essential fields
    memset(r, 0, sizeof(struct addrinfo));
    r->ai_family = hints->ai_family;
    r->ai_socktype = hints->ai_socktype;
    r->ai_protocol = hints->ai_protocol;
    r->ai_addrlen = sizeof(struct sockaddr_in);
    r->ai_addr = cmalloc(r->ai_addrlen);

    // Zero the fake ipv4 addr and fill our all of the fields
    addr = (struct sockaddr_in *)r->ai_addr;
    memcpy(&addr->sin_addr.s_addr, hostent->h_addr_list[i], sizeof(uint32_t));
    addr->sin_family = r->ai_family;
    addr->sin_port = platform_htons(atoi(service));
  }

  *res = r_head;
  return 0;
}

static void __freeaddrinfo(struct addrinfo *res)
{
  struct addrinfo *r, *r_next;
  for(r = res; r; r = r_next)
  {
    r_next = r->ai_next;
    free(r->ai_addr);
    free(r);
  }
}

#endif // __WIN32__ || __amigaos__

#ifndef __WIN32__

__network_maybe_static bool host_last_error_fatal(void)
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

// For LoadObject/LoadFunction
#include "SDL.h"

static struct
{
  /* These are Winsock 2.0 functions that should be present all the
   * way back to 98 (and 95 with the additional Winsock 2.0 update).
   */
  int (PASCAL *accept)(SOCKET, const struct sockaddr *, int *);
  int (PASCAL *bind)(SOCKET, const struct sockaddr *, int);
  int (PASCAL *closesocket)(SOCKET);
  int (PASCAL *connect)(SOCKET, const struct sockaddr *, int);
  struct hostent *(PASCAL *gethostbyname)(const char*);
  u_short (PASCAL *htons)(u_short);
  int (PASCAL *ioctlsocket)(SOCKET, long, u_long *);
  int (PASCAL *listen)(SOCKET, int);
  int (PASCAL *select)(int, fd_set *, fd_set *, fd_set *,
   const struct timeval *);
  int (PASCAL *send)(SOCKET, const char *, int, int);
  int (PASCAL *sendto)(SOCKET, const char*, int, int,
   const struct sockaddr *, int);
  int (PASCAL *setsockopt)(SOCKET, int, int, const char *, int);
  SOCKET (PASCAL *socket)(int, int, int);
  int (PASCAL *recv)(SOCKET, char *, int, int);
  int (PASCAL *recvfrom)(SOCKET, char*, int, int,
   struct sockaddr *, int *);

  // Similar to above but these are extensions of Berkeley sockets
  int (PASCAL *WSACancelBlockingCall)(void);
  int (PASCAL *WSACleanup)(void);
  int (PASCAL *WSAGetLastError)(void);
  int (PASCAL *WSAStartup)(WORD, LPWSADATA);
  int (PASCAL *__WSAFDIsSet)(SOCKET, fd_set *);

  // These functions were only implemented as of Windows XP (5.1)
  void (WSAAPI *freeaddrinfo)(struct addrinfo *);
  int (WSAAPI *getaddrinfo)(const char *, const char *,
   const struct addrinfo *, struct addrinfo **);

  void *handle;
}
socksyms;

static const struct dso_syms_map socksyms_map[] =
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
  { "sendto",                (void **)&socksyms.sendto },
  { "setsockopt",            (void **)&socksyms.setsockopt },
  { "socket",                (void **)&socksyms.socket },
  { "recv",                  (void **)&socksyms.recv },
  { "recvfrom",              (void **)&socksyms.recvfrom },

  { "WSACancelBlockingCall", (void **)&socksyms.WSACancelBlockingCall },
  { "WSACleanup",            (void **)&socksyms.WSACleanup },
  { "WSAGetLastError",       (void **)&socksyms.WSAGetLastError },
  { "WSAStartup",            (void **)&socksyms.WSAStartup },
  { "__WSAFDIsSet",          (void **)&socksyms.__WSAFDIsSet },

  { "freeaddrinfo",          (void **)&socksyms.freeaddrinfo },
  { "getaddrinfo",           (void **)&socksyms.getaddrinfo },

  { NULL, NULL }
};

static int init_ref_count;

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
    warn("Failed to load Winsock 2.0, falling back to Winsock\n");
    socksyms.handle = SDL_LoadObject(WINSOCK);
    if(!socksyms.handle)
    {
      warn("Failed to load Winsock fallback\n");
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
      warn("Failed to load Winsock symbol '%s'\n", socksyms_map[i].name);
      socket_free_syms();
      return false;
    }
  }

  return true;
}

__network_maybe_static bool host_last_error_fatal(void)
{
  if(socksyms.WSAGetLastError() == WSAEWOULDBLOCK)
    return false;
  return __host_last_error_fatal();
}

static inline int platform_accept(int sockfd,
 struct sockaddr *addr, socklen_t *addrlen)
{
  return socksyms.accept(sockfd, addr, addrlen);
}

static inline int platform_bind(int sockfd,
 const struct sockaddr *addr, socklen_t addrlen)
{
  return socksyms.bind(sockfd, addr, addrlen);
}

static inline void platform_close(int fd)
{
  socksyms.closesocket(fd);
}

static inline int platform_connect(int sockfd,
 const struct sockaddr *serv_addr, socklen_t addrlen)
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

static inline void platform_freeaddrinfo(struct addrinfo *res)
{
  if(socksyms.freeaddrinfo)
    socksyms.freeaddrinfo(res);
  else
    __freeaddrinfo(res);
}

static inline int platform_getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  if(socksyms.getaddrinfo)
    return socksyms.getaddrinfo(node, service, hints, res);
  return __getaddrinfo(node, service, hints, res);
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
  return socksyms.send(s, buf, (int)len, flags);
}

static inline ssize_t platform_sendto(int s, const void *buf, size_t len,
 int flags, const struct sockaddr *to, socklen_t tolen)
{
  return socksyms.sendto(s, buf, (int)len, flags, to, tolen);
}

static inline int platform_setsockopt(int s, int level, int optname,
 const void *optval, socklen_t optlen)
{
  return socksyms.setsockopt(s, level, optname, optval, optlen);
}

static inline int platform_socket(int af, int type, int protocol)
{
  return (int)socksyms.socket(af, type, protocol);
}

static inline void platform_socket_blocking(int s, bool blocking)
{
  unsigned long mode = !blocking;
  socksyms.ioctlsocket(s, FIONBIO, &mode);
}

static inline ssize_t platform_recv(int s, void *buf, size_t len, int flags)
{
  return socksyms.recv(s, buf, (int)len, flags);
}

static inline ssize_t platform_recvfrom(int s, void *buf, size_t len,
 int flags, struct sockaddr *from, socklen_t *fromlen)
{
  return socksyms.recvfrom(s, buf, (int)len, flags, from, fromlen);
}

#endif // __WIN32__

static struct config_info *conf;

bool host_layer_init(struct config_info *in_conf)
{
#ifdef __WIN32__
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
#endif // __WIN32__

  conf = in_conf;
  return true;
}

void host_layer_exit(void)
{
#ifdef __WIN32__
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
#endif // __WIN32__
}


struct host *host_create(enum host_type type, enum host_family fam)
{
  struct linger linger = { 1, 30 };
  const uint32_t on = 1;
  int err, fd, af, proto;
  struct host *h;

  switch(fam)
  {
#ifdef CONFIG_IPV6
    case HOST_FAMILY_IPV6:
      af = AF_INET6;
      break;
#endif
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

#if defined(CONFIG_IPV6) && defined(IPV6_V6ONLY)
  if(af == AF_INET6)
  {
    const uint32_t off = 0;
    err = platform_setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&off, 4);
    if(err < 0)
    {
      perror("setsockopt(IPV6_V6ONLY)");
      return NULL;
    }
  }
#endif

  /* We need to turn off bind address checking, allowing
   * port numbers to be reused; otherwise, TIME_WAIT will
   * delay binding to these ports for 2 * MSL seconds.
   */
  err = platform_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&on, 4);
  if(err < 0)
  {
    perror("setsockopt(SO_REUSEADDR)");
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
      perror("setsockopt(SO_LINGER)");
      return NULL;
    }
  }

  // Initially the socket is blocking
  platform_socket_blocking(fd, true);

  // Create our "host" abstraction (latterly augmented)
  h = ccalloc(1, sizeof(struct host));
  h->proto = proto;
  h->af = af;
  h->fd = fd;
  return h;
}

void host_destroy(struct host *h)
{
  if(h)
  {
    platform_close(h->fd);
    free(h);
  }
}

static bool __send(struct host *h, const void *buffer, size_t len)
{
  const char *buf = buffer;
  Uint32 start, now;
  ssize_t count;
  size_t pos;

  start = get_ticks();

  // send stuff in blocks, incrementally
  for(pos = 0; pos < len; pos += count)
  {
    now = get_ticks();

    // time out in 10 seconds if no data is sent
    if(now - start > 10 * 1000)
      return false;

    // normally it won't all get through at once
    count = platform_send(h->fd, &buf[pos], len - pos, 0);
    if(count < 0)
    {
      // non-blocking socket, so can fail and still be ok
      if(!host_last_error_fatal())
      {
        count = 0;
        continue;
      }

      return false;
    }

    if(h->cancel_cb && h->cancel_cb())
      return false;
  }

  return true;
}

static bool __recv(struct host *h, void *buffer, unsigned int len)
{
  char *buf = buffer;
  Uint32 start, now;
  ssize_t count;
  size_t pos;

  start = get_ticks();

  // receive stuff in blocks, incrementally
  for(pos = 0; pos < len; pos += count)
  {
    now = get_ticks();

    // time out in 10 seconds if no data is received
    if(now - start > 10 * 1000)
      return false;

    // normally it won't all get through at once
    count = platform_recv(h->fd, &buf[pos], len - pos, 0);
    if(count < 0)
    {
      // non-blocking socket, so can fail and still be ok
      if(!host_last_error_fatal())
      {
        count = 0;
        continue;
      }

      return false;
    }

    if(h->cancel_cb && h->cancel_cb())
      return false;
  }

  return true;
}

static struct addrinfo *connect_op(int fd, struct addrinfo *ais, void *priv)
{
  struct addrinfo *ai;

#ifdef CONFIG_IPV6
  /* First try to connect to an IPv6 address (if any)
   */
  for(ai = ais; ai; ai = ai->ai_next)
  {
    if(ai->ai_family != AF_INET6)
      continue;

    if(platform_connect(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen) >= 0)
      break;

    perror("connect");
  }

  if(ai)
    return ai;
#endif

  /* No IPv6 addresses could be connected; try IPv4 (if any)
   */
  for(ai = ais; ai; ai = ai->ai_next)
  {
    if(ai->ai_family != AF_INET)
      continue;

    if(platform_connect(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen) >= 0)
      break;

    perror("connect");
  }

  return ai;
}

static bool host_address_op(struct host *h, const char *hostname,
 int port, void *priv,
 struct addrinfo *(*op)(int fd, struct addrinfo *ais, void *priv))
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
    warn("Failed to look up '%s' (%s)\n", hostname, gai_strerror(ret));
    return false;
  }

  ai = op(h->fd, ais, priv);
  platform_freeaddrinfo(ais);

  // None of the listed hosts (if any) were connectable
  if(!ai)
  {
    warn("No routeable host '%s' found\n", hostname);
    return false;
  }

  if(!h->name)
    h->name = hostname;

  return true;
}

static bool _raw_host_connect(struct host *h, const char *hostname, int port)
{
  return host_address_op(h, hostname, port, NULL, connect_op);
}

static enum proxy_status socks4a_connect(struct host *h,
 const char *target_host, int target_port)
{
  char handshake[8];
  int rBuf;
  target_port = platform_htons(target_port);

  _raw_host_connect(h, conf->socks_host, conf->socks_port);

  if (!__send(h, "\4\1", 2))
    return PROXY_SEND_ERROR;
  if (!__send(h, &target_port, 2))
    return PROXY_SEND_ERROR;
  if (!__send(h, "\0\0\0\1anonymous", 14))
    return PROXY_SEND_ERROR;
  if (!__send(h, target_host, strlen(target_host)))
    return PROXY_SEND_ERROR;
  if (!__send(h, "\0", 1))
    return PROXY_SEND_ERROR;

  rBuf = __recv(h, handshake, 8);
  if (rBuf == -1)
    return PROXY_CONNECTION_FAILED;
  if (handshake[1] != 90)
    return PROXY_HANDSHAKE_FAILED;
  return PROXY_SUCCESS;
}

static enum proxy_status socks4_connect(struct host *h,
 struct addrinfo *ai_data)
{
  char handshake[8];
  int rBuf;

  _raw_host_connect(h, conf->socks_host, conf->socks_port);

  if (!__send(h, "\4\1", 2))
    return PROXY_SEND_ERROR;
  if (!__send(h, ai_data->ai_addr->sa_data, 6))
    return PROXY_SEND_ERROR;
  if (!__send(h, "anonymous\0", 10))
    return PROXY_SEND_ERROR;

  rBuf = __recv(h, handshake, 8);
  if (rBuf == -1)
    return PROXY_CONNECTION_FAILED;
  if (handshake[1] != 90)
    return PROXY_HANDSHAKE_FAILED;
  return PROXY_SUCCESS;
}

static enum proxy_status socks5_connect(struct host *h,
 struct addrinfo *ai_data)
{
  /* This handshake is more complicated than SOCKS4 or 4a...
   * and we're also only supporting none or user/password auth.
   */

  char handshake[10];
  int rBuf;

  _raw_host_connect(h, conf->socks_host, conf->socks_port);

  // Version[0x05]|Number of auth methods|auth methods
  if (!__send(h, "\5\1\0", 3))
    return PROXY_SEND_ERROR;

  rBuf = __recv(h, handshake, 2);
  if (rBuf == -1)
    return PROXY_CONNECTION_FAILED;
  if (handshake[0] != 0x5)
    return PROXY_HANDSHAKE_FAILED;

#ifdef CONFIG_IPV6
  if (ai_data->ai_family == AF_INET6)
    return PROXY_ADDRESS_TYPE_UNSUPPORTED;
#endif

  // Version[0x05]|Command|0x00|address type|destination|port
  if (!__send(h, "\5\1\0\1", 4))
    return PROXY_SEND_ERROR;
  if (!__send(h, ai_data->ai_addr->sa_data+2, 4))
    return PROXY_SEND_ERROR;
  if (!__send(h, ai_data->ai_addr->sa_data, 2))
    return PROXY_SEND_ERROR;

  rBuf = __recv(h, handshake, 10);
  if (rBuf == -1)
    return PROXY_CONNECTION_FAILED;
  switch (handshake[1])
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
static enum proxy_status proxy_connect(struct host *h, const char *target_host, int target_port)
{
  struct addrinfo target_hints, *target_ais, *target_ai;
  char port_str[6];
  int ret;

  snprintf(port_str, 6, "%d", target_port);
  port_str[5] = 0;

  memset(&target_hints, 0, sizeof(struct addrinfo));
  target_hints.ai_socktype = (h->proto == IPPROTO_TCP) ? SOCK_STREAM : SOCK_DGRAM;
  target_hints.ai_protocol = h->proto;
  target_hints.ai_family = h->af;
  ret = platform_getaddrinfo(target_host, port_str, &target_hints, &target_ais);

  /* Some perimeter gateways block access to DNS [wifi hotspots are
   * particularly notorious for this] so we have to fall back to SOCKS4a
   * to force the proxy to DNS the address for us. If this fails, we abort.
   */
  if(ret != 0)
    return socks4a_connect(h, target_host, target_port);

#ifdef CONFIG_IPV6
  for(target_ai = target_ais; target_ai; target_ai = target_ai->ai_next)
  {
    if(target_ai->ai_family != AF_INET6)
      continue;
    if (socks5_connect(h, target_ai) == PROXY_SUCCESS)
    {
      platform_freeaddrinfo(target_ais);
      return PROXY_SUCCESS;
    }
  }
#endif

  for(target_ai = target_ais; target_ai; target_ai = target_ai->ai_next)
  {
    if(target_ai->ai_family != AF_INET)
      continue;
    if (socks5_connect(h, target_ai) == PROXY_SUCCESS)
    {
      platform_freeaddrinfo(target_ais);
      return PROXY_SUCCESS;
    }
    if (socks4_connect(h, target_ai) == PROXY_SUCCESS)
    {
      platform_freeaddrinfo(target_ais);
      return PROXY_SUCCESS;
    }
  }

  return PROXY_CONNECTION_FAILED;
}

bool host_connect(struct host *h, const char *hostname, int port)
{
  if (strlen(conf->socks_host) > 0)
  {
    if (proxy_connect(h, hostname, port) == PROXY_SUCCESS)
    {
      h->proxied = true;
      h->endpoint = hostname;
      return true;
    }
    return false;
  }
  return _raw_host_connect(h, hostname, port);
}

static int http_recv_line(struct host *h, char *buffer, int len)
{
  int pos;

  assert(len > 0);

  // Grab data bytewise until we find CRLF characters
  for(pos = 0; pos < len; pos++)
  {
    // If recv() times out or fails, abort
    if(!__recv(h, &buffer[pos], 1))
    {
      pos = -HOST_RECV_FAILED;
      goto err_out;
    }

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
    pos = -2;

err_out:
  return pos;
}

static ssize_t http_send_line(struct host *h, const char *message)
{
  char line[LINE_BUF_LEN];
  ssize_t len;

  snprintf(line, LINE_BUF_LEN, "%s\r\n", message);
  len = (ssize_t)strlen(line);

  if(!__send(h, line, len))
    len = -HOST_SEND_FAILED;

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

static ssize_t zlib_skip_gzip_header(char *initial, unsigned long len)
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

enum host_status host_recv_file(struct host *h, const char *url,
 FILE *file, const char *expected_type)
{
  bool mid_inflate = false, mid_chunk = false, deflated = false;
  unsigned int content_length = 0;
  const char *host_name = h->name;
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
  if (h->proxied)
    host_name = h->endpoint;

  snprintf(line, LINE_BUF_LEN, "Host: %s", host_name);

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
    if(!__recv(h, block, block_size))
      return -HOST_RECV_FAILED;

    if(deflated)
    {
      /* This is the first block requiring inflation. In this case, we must
       * parse the GZIP header in order to compute an offset to the DEFLATE
       * formatted data.
       */
      if(!mid_inflate)
      {
        ssize_t deflate_offset = 0;

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

    if(h->recv_cb)
      h->recv_cb(ftell(file));

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

void host_set_callbacks(struct host *h, void (*send_cb)(long offset),
 void (*recv_cb)(long offset), bool (*cancel_cb)(void))
{
  assert(h != NULL);
  assert(send_cb == NULL);
  h->recv_cb = recv_cb;
  h->cancel_cb = cancel_cb;
}

#ifdef NETWORK_DEADCODE

#undef  FD_ISSET
#define FD_ISSET(fd,set) socksyms.__WSAFDIsSet((SOCKET)(fd),(fd_set *)(set))

void host_blocking(struct host *h, bool blocking)
{
  platform_socket_blocking(h->fd, blocking);
}

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
    if(host_last_error_fatal())
      perror("accept");
  }

  free(addr);
  return c;
}

static struct addrinfo *bind_op(int fd, struct addrinfo *ais, void *priv)
{
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

    perror("bind");
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

    perror("bind");
  }

  return ai;
}

bool host_bind(struct host *h, const char *hostname, int port)
{
  return host_address_op(h, hostname, port, NULL, bind_op);
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
  return __recv(h, buffer, len);
}

bool host_send_raw(struct host *h, const char *buffer, unsigned int len)
{
  return __send(h, buffer, len);
}

struct buf_priv_data {
  char *buffer;
  unsigned int len;
  struct host *h;
  bool ret;
};

static struct addrinfo *recvfrom_raw_op(int fd, struct addrinfo *ais,
 void *priv)
{
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

static struct addrinfo *sendto_raw_op(int fd, struct addrinfo *ais, void *priv)
{
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

bool host_recvfrom_raw(struct host *h, char *buffer, unsigned int len,
 const char *hostname, int port)
{
  struct buf_priv_data buf_priv = { buffer, len, h, true };
  host_address_op(h, hostname, port, &buf_priv, recvfrom_raw_op);
  return buf_priv.ret;
}

bool host_sendto_raw(struct host *h, const char *buffer, unsigned int len,
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

enum host_status host_send_file(struct host *h, FILE *file,
 const char *mime_type)
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
      if(!__send(h, zblock, BLOCK_SIZE - stream.avail_out))
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
      if(!__send(h, &crc, sizeof(uint32_t)))
        return -HOST_SEND_FAILED;

      // Write out GZIP `ISIZE' footer
      if(!__send(h, &uSize, sizeof(uint32_t)))
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
  enum host_status ret;
  size_t path_len;
  FILE *f;

  if(http_recv_line(h, buffer, LINE_BUF_LEN) < 0)
  {
    warn("Failed to receive HTTP request\n");
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
    warn("Client must support HTTP 1.1, rejecting\n");
    return false;
  }

  if(!http_skip_headers(h))
  {
    warn("Failed to skip HTTP headers\n");
    return false;
  }

  path++;
  debug("Received request for '%s'\n", path);

  f = fopen(path, "rb");
  if(!f)
  {
    warn("Failed to open file '%s', sending 404\n", path);

    snprintf(buffer, LINE_BUF_LEN,
     "HTTP/1.1 404 Not Found\r\n"
     "Content-Length: %zd\r\n"
     "Content-Type: text/html\r\n"
     "Connection: close\r\n\r\n", strlen(resp_404));

    if(__send(h, buffer, strlen(buffer)))
    {
      if(!__send(h, resp_404, strlen(resp_404)))
        warn("Failed to send 404 payload\n");
    }
    else
      warn("Failed to send 404 status code\n");

    return false;
  }

  path_len = strlen(path);
  if(path_len >= 4 && strcasecmp(&path[path_len - 4], ".txt") == 0)
    mime_type = "text/plain";

  ret = host_send_file(h, f, mime_type);
  if(ret != HOST_SUCCESS)
  {
    warn("Failed to send file '%s' over HTTP (error %d)\n", path, ret);
    return false;
  }

  return true;
}

#endif // NETWORK_DEADCODE
