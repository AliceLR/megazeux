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
#include "dns.h"
#include "socksyms.h"

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
#pragma GCC diagnostic ignored "-Wsign-compare"
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
  Uint32 timeout_ms;
};

#ifndef __amigaos__
#define CONFIG_IPV6
#endif

static struct config_info *conf;

bool host_layer_init(struct config_info *in_conf)
{
  if(!socksyms_init(in_conf))
    return false;

  if(!dns_init(in_conf))
    return false;

  conf = in_conf;
  return true;
}

void host_layer_exit(void)
{
  dns_exit();
  socksyms_exit();
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
    platform_perror("socket");
    return NULL;
  }

#if defined(CONFIG_IPV6) && defined(IPV6_V6ONLY)
  if(af == AF_INET6)
  {
    const uint32_t off = 0;
    err = platform_setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&off, 4);
    if(err < 0)
    {
      platform_perror("setsockopt(IPV6_V6ONLY)");
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
    platform_perror("setsockopt(SO_REUSEADDR)");
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
      platform_perror("setsockopt(SO_LINGER)");
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
  h->timeout_ms = HOST_TIMEOUT_DEFAULT;
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

void host_set_timeout_ms(struct host *h, int timeout_ms)
{
  h->timeout_ms = timeout_ms;
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

    // time out if no data is sent
    if(now - start > h->timeout_ms)
      return false;

    // normally it won't all get through at once
    count = platform_send(h->fd, &buf[pos], len - pos, 0);
    if(count < 0)
    {
      // non-blocking socket, so can fail and still be ok
      if(!platform_last_error_fatal())
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

    // time out if no data is received
    if(now - start > h->timeout_ms)
      return false;

    // normally it won't all get through at once
    count = platform_recv(h->fd, &buf[pos], len - pos, 0);
    if(count < 0 && platform_last_error_fatal())
    {
      return false;
    } else if (count <= 0) {
      // non-blocking socket, so can fail and still be ok
      // Add a short delay to prevent excessive CPU use
      delay(10);
      count = 0;
      continue;
    }

    if(h->cancel_cb && h->cancel_cb())
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

static struct addrinfo *connect_op(int fd, struct addrinfo *ais, void *priv,
 Uint32 timeout)
{
  struct addrinfo *ai;
  struct timeval tv;
  fd_set mask;
  int res;

  FD_ZERO(&mask);
  FD_SET(fd, &mask);

  // Disable blocking on the socket so a timeout can be enforced.
  platform_socket_blocking(fd, false);

#ifdef CONFIG_IPV6
  /* First try to connect to an IPv6 address (if any)
   */
  for(ai = ais; ai; ai = ai->ai_next)
  {
    if(ai->ai_family != AF_INET6)
      continue;

    platform_connect(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen);

    reset_timeout(&tv, timeout);
    res = platform_select(fd + 1, NULL, &mask, NULL, &tv);

    if(res == 1)
      break;

    if(res == 0)
      info("Connection timed out.\n");

    else
      platform_perror("connect");

    platform_close(fd);
  }

  if(ai)
  {
    platform_socket_blocking(fd, true);
    return ai;
  }
#endif

  /* No IPv6 addresses could be connected; try IPv4 (if any)
   */
  for(ai = ais; ai; ai = ai->ai_next)
  {
    if(ai->ai_family != AF_INET)
      continue;

    platform_connect(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen);

    reset_timeout(&tv, timeout);
    res = platform_select(fd + 1, NULL, &mask, NULL, &tv);

    if(res == 1)
      break;

    if(res == 0)
      info("Connection timed out.\n");

    else
      platform_perror("connect");

    platform_close(fd);
  }

  if(ai)
  {
    platform_socket_blocking(fd, true);
    return ai;
  }

  return NULL;
}

static bool host_address_op(struct host *h, const char *hostname,
 int port, void *priv, struct addrinfo *(*op)(int fd, struct addrinfo *ais,
  void *priv, Uint32 timeout))
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
  ret = dns_getaddrinfo(hostname, port_str, &hints, &ais, h->timeout_ms);
  if(ret != 0)
  {
    warn("Failed to look up '%s' (%s)\n", hostname, platform_gai_strerror(ret));
    return false;
  }

  ai = op(h->fd, ais, priv, h->timeout_ms);
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
static enum proxy_status proxy_connect(struct host *h, const char *target_host,
 int target_port)
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
  ret = dns_getaddrinfo(target_host, port_str, &target_hints, &target_ais,
   h->timeout_ms);

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

static bool http_read_status(struct http_info *result, const char *status,
 size_t status_len)
{
  /* These conditionals check the status line is formatted:
   *   "HTTP/1.? XXX ..." (where ? is 0 or 1, XXX is the status code,
   *                      and ... is the status message)
   *
   * This is because partially HTTP/1.1 capable servers supporting
   * pipelining may not advertise full HTTP/1.1 compliance (e.g.
   * `perlbal' httpd).
   *
   * MegaZeux only cares about pipelining.
   */
  char ver[2];
  char code[4];
  int res;
  int c;

  result->status_message[0] = 0;

  res = sscanf(status, "HTTP/1.%1s %3s %31[^\r\n]", ver, code,
   result->status_message);

  debug("Status: %s (%s, %s, %s)\n", status, ver, code, result->status_message);
  if(res != 3 || (ver[0] != '0' && ver[0] != '1'))
    return false;

  // Make sure the code is a 3-digit number
  c = strtol(code, NULL, 10);
  if(c < 100)
    return false;

  result->status_code = c;
  result->status_type = c / 100;

  return true;
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

enum host_status host_recv_file(struct host *h, struct http_info *req,
 FILE *file)
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
  snprintf(line, LINE_BUF_LEN, "GET %s HTTP/1.1", req->url);
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

  if(!http_read_status(req, line, line_len))
  {
    warn("Invalid status: %s\nFailed for url '%s'\n", line, req->url);
    return -HOST_HTTP_INVALID_STATUS;
  }

  // Unhandled status categories
  switch(req->status_type)
  {
    case 1:
      return -HOST_HTTP_INFO;

    case 3:
      return -HOST_HTTP_REDIRECT;

    case 4:
      return -HOST_HTTP_CLIENT_ERROR;

    case 5:
      return -HOST_HTTP_SERVER_ERROR;
  }

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
      strncpy(req->content_type, value, 63);

      if(strcmp(value, req->expected_type) != 0)
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

// FIXME splitting out socksyms broke this. Make it a platform function.
#undef  FD_ISSET
#define FD_ISSET(fd,set) socksyms.__WSAFDIsSet((SOCKET)(fd),(fd_set *)(set))

bool host_last_error_fatal(void)
{
  return platform_last_error_fatal();
}

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

bool host_bind(struct host *h, const char *hostname, int port)
{
  return host_address_op(h, hostname, port, NULL, bind_op);
}

bool host_listen(struct host *h)
{
  if(platform_listen(h->fd, 0) < 0)
  {
    platform_perror("listen");
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
