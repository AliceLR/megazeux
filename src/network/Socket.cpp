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

#include "Socket.hpp"

#include "../platform.h"
#include "../util.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>

#if defined(__WIN32__) || !defined(CONFIG_GETADDRINFO)

/**
 * gethostbyname may use a static struct that is shared across threads
 * in some implementations. For safety, acquire a lock until the call to it
 * and all use of its return value is complete.
 */
static platform_mutex gai_lock;

int Socket::getaddrinfo_alt(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  struct addrinfo *r = NULL, *r_head = NULL;
  struct hostent *hostent;
  int i;

  trace("--SOCKET-- Socket::getaddrinfo_alt\n");

  // This relies on hints being provided to initialize the addrinfo structs...
  if(!hints)
    return EAI_FAIL;

  /**
   * If hints.ai_family is provided as AF_UNSPEC, getaddrinfo is expected to
   * return anything found (including AF_INET), so it's acceptable here.
   * https://pubs.opengroup.org/onlinepubs/9699919799/functions/freeaddrinfo.html
   */
  if(hints->ai_family != AF_INET && hints->ai_family != AF_UNSPEC)
    return EAI_FAMILY;

  platform_mutex_lock(&gai_lock);

  hostent = Socket::gethostbyname(node);
  if(!hostent || hostent->h_addrtype != AF_INET)
  {
    platform_mutex_unlock(&gai_lock);
    return EAI_NONAME;
  }

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
      r->ai_next = (struct addrinfo *)cmalloc(sizeof(struct addrinfo));
      r = r->ai_next;
    }
    else
      r_head = r = (struct addrinfo *)cmalloc(sizeof(struct addrinfo));

    // Zero the fake addrinfo and fill out the essential fields
    memset(r, 0, sizeof(struct addrinfo));
    r->ai_family = hostent->h_addrtype;
    r->ai_socktype = hints->ai_socktype;
    r->ai_protocol = hints->ai_protocol;
    r->ai_addrlen = sizeof(struct sockaddr_in);
    r->ai_addr = (struct sockaddr *)cmalloc(r->ai_addrlen);

    // Zero the fake ipv4 addr and fill our all of the fields
    addr = (struct sockaddr_in *)r->ai_addr;
    memcpy(&addr->sin_addr.s_addr, hostent->h_addr_list[i], sizeof(uint32_t));
    addr->sin_family = r->ai_family;
    addr->sin_port = Socket::hton_short(atoi(service));
  }

  platform_mutex_unlock(&gai_lock);
  *res = r_head;
  return 0;
}

void Socket::freeaddrinfo_alt(struct addrinfo *res)
{
  struct addrinfo *r, *r_next;
  for(r = res; r; r = r_next)
  {
    r_next = r->ai_next;
    free(r->ai_addr);
    free(r);
  }
}

const char *Socket::gai_strerror_alt(int errcode)
{
  switch(errcode)
  {
    case EAI_NONAME:
      return "Node or service is not known.";
    case EAI_AGAIN:
      return "Temporary failure in name resolution";
    case EAI_FAIL:
      return "Non-recoverable failure in name resolution";
    case EAI_FAMILY:
      return "Address family is not supported.";
    default:
      return "Unknown error.";
  }
}
#endif // __WIN32__ || !CONFIG_GETADDRINFO

#if !defined(__WIN32__) && !defined(CONFIG_GETADDRINFO)
static int init_ref_count;

boolean Socket::init(struct config_info *conf)
{
  if(!init_ref_count)
  {
    if(!platform_socket_init())
      return false;

    platform_mutex_init(&gai_lock);
  }
  init_ref_count++;
  return true;
}

void Socket::exit()
{
  assert(init_ref_count);
  init_ref_count--;
  if(!init_ref_count)
  {
    platform_socket_exit();
    platform_mutex_destroy(&gai_lock);
  }
}

int Socket::getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  return Socket::getaddrinfo_alt(node, service, hints, res);
}

void Socket::freeaddrinfo(struct addrinfo *res)
{
  return Socket::freeaddrinfo_alt(res);
}

void Socket::getaddrinfo_perror(const char *message, int errcode)
{
  warn("%s (code %d): %s\n", message, errcode,
   Socket::gai_strerror_alt(errcode));
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
  int (PASCAL *getsockopt)(SOCKET, int, int, char *, int *);
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
  { "getsockopt",            (fn_ptr *)&socksyms.getsockopt },
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

static boolean socket_load_syms(void)
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

boolean Socket::init(struct config_info *conf)
{
  WORD version = MAKEWORD(1, 0);
  WSADATA ws_data;

  if(init_ref_count == 0)
  {
    if(!gai_lock)
      platform_mutex_init(&gai_lock);

    if(!socket_load_syms())
      return false;

    if(socksyms.WSAStartup(version, &ws_data) < 0)
      return false;
  }

  init_ref_count++;
  return true;
}

void Socket::exit(void)
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

  platform_mutex_destroy(&gai_lock);
  gai_lock = 0;
  socket_free_syms();
}

boolean Socket::is_last_error_fatal(void)
{
  if(socksyms.WSAGetLastError() == WSAEWOULDBLOCK)
    return false;
  return Socket::is_last_errno_fatal();
}

/**
 * It turns out perror is completely useless for Winsock error messages, so
 * as a workaround, use Win32 functions instead (these should be safe back to
 * Windows 95).
 */
void winsock_perror(const char *message, int code)
{
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

  warn("%s (code %d): %s", message, code, err_message);
  LocalFree(err_message);
}

void Socket::get_errno()
{
  return socksyms.WSAGetLastError();
}

void Socket::perror(const char *message)
{
  int code = socksyms.WSAGetLastError();
  winsock_perror(message, code);
}

int Socket::accept(int sockfd,
 struct sockaddr *addr, socklen_t *addrlen)
{
  return socksyms.accept(sockfd, addr, addrlen);
}

int Socket::bind(int sockfd,
 const struct sockaddr *addr, socklen_t addrlen)
{
  return socksyms.bind(sockfd, addr, addrlen);
}

void Socket::close(int fd)
{
  socksyms.closesocket(fd);
}

int Socket::connect(int sockfd,
 const struct sockaddr *serv_addr, socklen_t addrlen)
{
  return socksyms.connect(sockfd, serv_addr, addrlen);
}

struct hostent *Socket::gethostbyname(const char *name)
{
  return socksyms.gethostbyname(name);
}

int Socket::getsockopt(int sockfd, int level, int optname, void *optval,
 socklen_t *optlen)
{
  return socksyms.getsockopt(sockfd, level, optname, (char *)optval, optlen);
}

uint16_t Socket::hton_short(uint16_t hostshort)
{
  return socksyms.htons(hostshort);
}

void Socket::freeaddrinfo(struct addrinfo *res)
{
  if(socksyms.freeaddrinfo)
    socksyms.freeaddrinfo(res);
  else
    Socket::freeaddrinfo_alt(res);
}

int Socket::getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  if(socksyms.getaddrinfo)
    return socksyms.getaddrinfo(node, service, hints, res);
  return Socket::getaddrinfo_alt(node, service, hints, res);
}

/**
 * A Win32 version of gai_strerror is available but apparently uses a 1K
 * static buffer for the error message, making it non-thread safe.
 * Fortunately, the workaround for this is the same as for perror.
 */
void Socket::getaddrinfo_perror(const char *message, int errcode)
{
  winsock_perror(message, errcode);
}

int Socket::listen(int sockfd, int backlog)
{
  return socksyms.listen(sockfd, backlog);
}

int Socket::select(int nfds, fd_set *readfds,
 fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  return socksyms.select(nfds, readfds, writefds, exceptfds, timeout);
}

ssize_t Socket::send(int sockfd, const void *buf, size_t len,
 int flags)
{
  return socksyms.send(sockfd, (const char *)buf, (int)len, flags);
}

ssize_t Socket::sendto(int sockfd, const void *buf, size_t len,
 int flags, const struct sockaddr *to, socklen_t tolen)
{
  return socksyms.sendto(sockfd, (const char *)buf, (int)len, flags, to, tolen);
}

int Socket::setsockopt(int sockfd, int level, int optname,
 const void *optval, socklen_t optlen)
{
  return socksyms.setsockopt(sockfd, level, optname, (const char *)optval, optlen);
}

int Socket::socket(int af, int type, int protocol)
{
  return (int)socksyms.socket(af, type, protocol);
}

void Socket::set_blocking(int sockfd, boolean blocking)
{
  unsigned long mode = !blocking;
  socksyms.ioctlsocket(sockfd, FIONBIO, &mode);
}

ssize_t Socket::recv(int sockfd, void *buf, size_t len, int flags)
{
  return socksyms.recv(sockfd, (char *)buf, (int)len, flags);
}

ssize_t Socket::recvfrom(int sockfd, void *buf, size_t len,
 int flags, struct sockaddr *from, socklen_t *fromlen)
{
  return socksyms.recvfrom(sockfd, (char *)buf, (int)len, flags, from, fromlen);
}

int Socket::__WSAFDIsSet(int sockfd, fd_set *set)
{
  return socksyms.__WSAFDIsSet(sockfd, set);
}

#endif /*__WIN32__*/
