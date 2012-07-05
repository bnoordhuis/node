/*
 * Copyright (c) 2012, Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "uv.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <assert.h>

#include <netinet/in.h>

#define container_of(ptr, type, member)                                       \
  ((type *) ((char *) (ptr) - offsetof(type, member)))

struct config
{
  struct sockaddr_in addr;
  unsigned int concurrency;
  unsigned int requests;
  unsigned int timelimit;
};

struct udp_ctx
{
  uv_udp_t handle;
  uv_udp_send_t req;
  struct config *config;
};

static const char *argv0;
static uv_buf_t payload;

static void send_cb(uv_udp_send_t* req, int status)
{
  struct udp_ctx *u = container_of(req, struct udp_ctx, req);
  struct config *c = u->config;

  if (status) {
    if (uv_last_error(u->handle.loop).code == UV_EINTR)
      return;
    else
      abort();
  }

  if (c->requests == 0) {
    uv_close((uv_handle_t *) &u->handle, NULL);
    return;
  }

  if (uv_udp_send(&u->req, &u->handle, &payload, 1, c->addr, send_cb))
    abort();

  c->requests--;
}

static void timer_cb(uv_timer_t* handle, int status)
{
  struct udp_ctx *u;
  struct config *c;
  unsigned int i;

  u = handle->data;
  c = u->config;

  for (i = 0; i < c->concurrency; i++)
    uv_close((uv_handle_t *) &u[i].handle, NULL);
}

static int pummel(uv_loop_t* loop, struct config *c)
{
  struct udp_ctx *uctx;
  struct udp_ctx *u;
  uv_timer_t timer;
  unsigned int i;

  assert(c->requests >= c->concurrency);
  assert(c->requests >= 1);

  uctx = malloc(c->concurrency * sizeof(uctx[0]));
  if (uctx == NULL)
    return perror("malloc"), -1;

  payload = uv_buf_init("PING", 4);

  for (i = 0; i < c->concurrency; i++) {
    u = uctx + i;
    u->config = c;

    if (uv_udp_init(loop, &u->handle))
      abort();

    if (uv_udp_send(&u->req, &u->handle, &payload, 1, c->addr, send_cb))
      abort();

    c->requests--;
  }

  if (uv_timer_init(loop, &timer))
    abort();

  uv_unref((uv_handle_t *) &timer);
  timer.data = uctx;

  if (c->timelimit)
    uv_timer_start(&timer, timer_cb, c->timelimit * 1000, 0);

  uv_run(loop);
  free(uctx);

  return 0;
}

static void usage(void)
{
  fprintf(stderr,
          "Usage:\n"
          " %s [options] <address> <port>\n"
          "\n"
          "Options are:\n"
          " -c concurrency  Number of multiple requests to make.\n"
          " -n requests     Number of requests to make.\n"
          " -t timelimit    Seconds to wait for responses.\n",
          argv0);
  exit(1);
}

int main(int argc, char **argv)
{
  struct config c;
  int ch;

  argv0 = argv[0];

  memset(&c, 0, sizeof(c));
  c.concurrency = 1;

  while ((ch = getopt(argc, argv, "c:hn:t:")) != -1) {
    if (ch == 'c')
      c.concurrency = atoi(optarg);
    else if (ch == 'n')
      c.requests = (unsigned int) atoi(optarg);
    else if (ch == 't')
      c.timelimit = (unsigned int) atoi(optarg);
    else
      usage();
  }

  if (c.requests == 0)
    c.requests = c.timelimit ? UINT_MAX : 1;

  if (c.concurrency > c.requests)
    c.concurrency = c.requests;

  if (argc < optind + 2)
    usage();

  {
    const char *host = argv[optind];
    const char *port = argv[optind + 1];
    c.addr = uv_ip4_addr(host, atoi(port));
  }

  if (pummel(uv_default_loop(), &c))
    exit(1);

  return 0;
}
