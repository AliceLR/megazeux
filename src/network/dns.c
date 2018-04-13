/* MegaZeux
 *
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

// Asynchronous wrapper for getaddrinfo. This was easier to implement than
// trying to work with platform-dependent solutions.

#include "dns.h"
#include "socksyms.h"

#include "../platform.h"
#include "../util.h"

// Time in ms for threads to sleep between checks.
#define DNS_DELAY 50

#define LOCK(d)   platform_mutex_lock(&(d->mutex))
#define UNLOCK(d) platform_mutex_unlock(&(d->mutex))

enum dns_state
{
  STATE_INIT = 0,
  STATE_EXIT,
  STATE_STANDBY,
  STATE_LOOKUP,
  STATE_SUCCESS,
  STATE_ABORT,
};

struct dns_data
{
  platform_thread thread;
  platform_mutex mutex;
  enum dns_state state;
  char *node;
  char *service;
  struct addrinfo hints;
  struct addrinfo *res;
  int ret;
};

static struct dns_data *threads;
static int threads_count;
static int threads_max;

static void set_dns_thread_data(struct dns_data *data,
 const char *node, const char *service, const struct addrinfo *hints)
{
  size_t node_len = strlen(node) + 1;
  size_t service_len = strlen(service) + 1;

  data->node = cmalloc(node_len);
  data->service = cmalloc(service_len);
  memcpy(data->node, node, node_len);
  memcpy(data->service, service, service_len);
  memcpy(&(data->hints), hints, sizeof(struct addrinfo));
  data->res = NULL;
}

static void free_dns_thread_data(struct dns_data *data, bool free_result)
{
  free(data->node);
  free(data->service);
  if(free_result && data->res)
    platform_freeaddrinfo(data->res);

  data->node = NULL;
  data->service = NULL;
  data->res = NULL;
}

static void *run_dns_thread(void *_data)
{
  struct dns_data *data = (struct dns_data *)_data;
  int ret;

  debug("--DNS-- New thread running.\n");

  do
  {
    while(data->state == STATE_STANDBY || data->state == STATE_SUCCESS)
      delay(DNS_DELAY);

    if(data->state == STATE_LOOKUP)
    {
      debug("--DNS-- Starting lookup.\n");
      ret = platform_getaddrinfo(data->node, data->service, &(data->hints),
       &(data->res));
    }

    if(data->state == STATE_EXIT)
    {
      debug("--DNS-- Thread exited.\n");
      return NULL;
    }

    LOCK(data);

    switch(data->state)
    {
      case(STATE_LOOKUP):
      {
        debug("--DNS-- Lookup completed.\n");
        data->state = STATE_SUCCESS;
        data->ret = ret;
        break;
      }

      default:
      {
        debug("--DNS-- Lookup completed (discarding; %d).\n", data->state);
        free_dns_thread_data(data, true);
        data->state = STATE_STANDBY;
        break;
      }
    }

    UNLOCK(data);
  }
  while(true);
}

static void create_dns_thread(struct dns_data *data)
{
  platform_mutex_init(&(data->mutex));
  data->state = STATE_STANDBY;

  if(platform_thread_create(&(data->thread),
   (platform_thread_fn)run_dns_thread, (void *)data))
  {
    platform_mutex_destroy(&(data->mutex));
    data->state = STATE_INIT;
  }
}

static void destroy_dns_thread(struct dns_data *data)
{
  LOCK(data);
  data->state = STATE_EXIT;
  UNLOCK(data);

  platform_thread_join(&(data->thread));
  platform_mutex_destroy(&(data->mutex));
  free_dns_thread_data(data, true);
}

static void wait_dns_thread(struct dns_data *data)
{
  int i;

  // FIXME variable timeout value
  for(i = 0; i < 1000; i += DNS_DELAY)
  {
    delay(DNS_DELAY);
    if(data->state == STATE_SUCCESS)
      return;
  }
}

int dns_getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  int ret;
  int i;

  for(i = 0; i < threads_max; i++)
  {
    struct dns_data *data = threads + i;

    if(data->state == STATE_INIT)
    {
      debug("--DNS-- Starting DNS thread %d.\n", threads_count);
      create_dns_thread(data);
      threads_count++;
    }

    if(data->state == STATE_STANDBY)
    {
      set_dns_thread_data(data, node, service, hints);
      data->state = STATE_LOOKUP;

      debug("--DNS-- Waiting for response.\n");
      wait_dns_thread(data);

      LOCK(data);

      if(data->state == STATE_SUCCESS)
      {
        ret = data->ret;
        *res = data->res;
        debug("--DNS-- Completed successfully (return code %d)\n", ret);
        free_dns_thread_data(data, false);
        data->state = STATE_STANDBY;
      }
      else
      {
        debug("--DNS-- Timed out.\n");
        data->state = STATE_ABORT;
        ret = EAI_AGAIN;
        *res = NULL;
      }

      UNLOCK(data);
      return ret;
    }
  }

  // Try to run manually instead.
  debug("--DNS-- No DNS threads available; running lookup in main thread.\n");
  ret = platform_getaddrinfo(node, service, hints, res);
  debug("--DNS-- Completed successfully (return code %d)\n", ret);
  return ret;
}

bool dns_init(struct config_info *conf)
{
  threads = ccalloc(conf->update_host_count, sizeof(struct dns_data));
  threads_max = conf->update_host_count;
  threads_count = 0;
  return true;
}

void dns_exit(void)
{
  int i;
  for(i = 0; i < threads_count; i++)
  {
    if(threads[i].state != STATE_INIT)
    {
      debug("--DNS-- Waiting for DNS thread %d\n", i);
      destroy_dns_thread(threads + i);
      debug("--DNS-- Thread %d joined.\n", i);
    }
  }

  free(threads);
}
