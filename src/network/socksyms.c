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

#include "socksyms.h"

#include "../util.h"

#include <assert.h>

#if defined(__WIN32__) || defined(__amigaos__)

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

#ifdef __amigaos__
int getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  return __getaddrinfo(node, service, hints, res);
}

void freeaddrinfo(struct addrinfo *res)
{
  return __freeaddrinfo(res);
}
#endif

#ifdef __WIN32__

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
  { "accept",                (fn_ptr *)&socksyms.accept },
  { "bind",                  (fn_ptr *)&socksyms.bind },
  { "closesocket",           (fn_ptr *)&socksyms.closesocket },
  { "connect",               (fn_ptr *)&socksyms.connect },
  { "gethostbyname",         (fn_ptr *)&socksyms.gethostbyname },
  { "htons",                 (fn_ptr *)&socksyms.htons },
  { "ioctlsocket",           (fn_ptr *)&socksyms.ioctlsocket },
  { "listen",                (fn_ptr *)&socksyms.listen },
  { "select",                (fn_ptr *)&socksyms.select },
  { "send",                  (fn_ptr *)&socksyms.send },
  { "sendto",                (fn_ptr *)&socksyms.sendto },
  { "setsockopt",            (fn_ptr *)&socksyms.setsockopt },
  { "socket",                (fn_ptr *)&socksyms.socket },
  { "recv",                  (fn_ptr *)&socksyms.recv },
  { "recvfrom",              (fn_ptr *)&socksyms.recvfrom },

  { "WSACancelBlockingCall", (fn_ptr *)&socksyms.WSACancelBlockingCall },
  { "WSACleanup",            (fn_ptr *)&socksyms.WSACleanup },
  { "WSAGetLastError",       (fn_ptr *)&socksyms.WSAGetLastError },
  { "WSAStartup",            (fn_ptr *)&socksyms.WSAStartup },
  { "__WSAFDIsSet",          (fn_ptr *)&socksyms.__WSAFDIsSet },

  { "freeaddrinfo",          (fn_ptr *)&socksyms.freeaddrinfo },
  { "getaddrinfo",           (fn_ptr *)&socksyms.getaddrinfo },

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
    void **sym_ptr = (void **)socksyms_map[i].sym_ptr;
    *sym_ptr = SDL_LoadFunction(socksyms.handle, socksyms_map[i].name);

    if(!*sym_ptr)
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

bool socksyms_init(struct config_info *conf)
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

void socksyms_exit(void)
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

bool platform_last_error_fatal(void)
{
  if(socksyms.WSAGetLastError() == WSAEWOULDBLOCK)
    return false;
  return __platform_last_error_fatal();
}

void platform_perror(const char *message)
{
  int code = socksyms.WSAGetLastError();
  LPSTR err_message = NULL;

  FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
    0,
    code,
    0,
    (LPSTR)&err_message,
    0,
    NULL
  );

  fprintf(stderr, "%s (code %d): %s", message, code, err_message);
}

int platform_accept(int sockfd,
 struct sockaddr *addr, socklen_t *addrlen)
{
  return socksyms.accept(sockfd, addr, addrlen);
}

int platform_bind(int sockfd,
 const struct sockaddr *addr, socklen_t addrlen)
{
  return socksyms.bind(sockfd, addr, addrlen);
}

void platform_close(int fd)
{
  socksyms.closesocket(fd);
}

int platform_connect(int sockfd,
 const struct sockaddr *serv_addr, socklen_t addrlen)
{
  return socksyms.connect(sockfd, serv_addr, addrlen);
}

struct hostent *platform_gethostbyname(const char *name)
{
  return socksyms.gethostbyname(name);
}

uint16_t platform_htons(uint16_t hostshort)
{
  return socksyms.htons(hostshort);
}

void platform_freeaddrinfo(struct addrinfo *res)
{
  if(socksyms.freeaddrinfo)
    socksyms.freeaddrinfo(res);
  else
    __freeaddrinfo(res);
}

int platform_getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  if(socksyms.getaddrinfo)
    return socksyms.getaddrinfo(node, service, hints, res);
  return __getaddrinfo(node, service, hints, res);
}

int platform_listen(int sockfd, int backlog)
{
  return socksyms.listen(sockfd, backlog);
}

int platform_select(int nfds, fd_set *readfds,
 fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  return socksyms.select(nfds, readfds, writefds, exceptfds, timeout);
}

ssize_t platform_send(int s, const void *buf, size_t len,
 int flags)
{
  return socksyms.send(s, buf, (int)len, flags);
}

ssize_t platform_sendto(int s, const void *buf, size_t len,
 int flags, const struct sockaddr *to, socklen_t tolen)
{
  return socksyms.sendto(s, buf, (int)len, flags, to, tolen);
}

int platform_setsockopt(int s, int level, int optname,
 const void *optval, socklen_t optlen)
{
  return socksyms.setsockopt(s, level, optname, optval, optlen);
}

int platform_socket(int af, int type, int protocol)
{
  return (int)socksyms.socket(af, type, protocol);
}

void platform_socket_blocking(int s, bool blocking)
{
  unsigned long mode = !blocking;
  socksyms.ioctlsocket(s, FIONBIO, &mode);
}

ssize_t platform_recv(int s, void *buf, size_t len, int flags)
{
  return socksyms.recv(s, buf, (int)len, flags);
}

ssize_t platform_recvfrom(int s, void *buf, size_t len,
 int flags, struct sockaddr *from, socklen_t *fromlen)
{
  return socksyms.recvfrom(s, buf, (int)len, flags, from, fromlen);
}

#endif /*__WIN32__*/
