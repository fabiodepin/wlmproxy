/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "connection.h"

#include <unistd.h>

#include <set>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>

#include "msn/msn.h"
#include "log.h"

std::set<Connection*> connections;

static uint32_t num_connections = 1;

Connection::Connection(struct event_base* base, int fd)
    : session(new Session()),
      client_bufev(NULL),
      server_bufev(NULL),
      ev_base(base),
      client_addr(0),
      server_addr(0),
      client_port(0),
      server_port(0),
      id(num_connections++),
      client_fd(fd),
      server_fd(-1),
      type(NONE) {
  cmd[0] = new Command(this);
  cmd[1] = new Command(this, Command::INBOUND);

  connections.insert(this);
}

Connection::~Connection() {
  log_info("destroying connection %u", id);

  if (client_bufev && client_fd != -1)
    evbuffer_write(bufferevent_get_output(client_bufev), client_fd);
  if (server_bufev && server_fd != -1)
    evbuffer_write(bufferevent_get_output(server_bufev), server_fd);

  if (client_fd != -1)
    evutil_closesocket(client_fd);
  if (server_fd != -1)
    evutil_closesocket(server_fd);

  if (client_bufev != NULL)
    bufferevent_free(client_bufev);
  if (server_bufev != NULL)
    bufferevent_free(server_bufev);

  delete cmd[0];
  delete cmd[1];

  msn::destroy_cb(this);

  delete session;

  connections.erase(this);
}

void Connection::start() {
  client_bufev = bufferevent_socket_new(ev_base, client_fd,
                                        BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(client_bufev, client_read_cb, NULL, client_error_cb, this);
  bufferevent_enable(client_bufev, EV_READ);

  server_bufev = bufferevent_socket_new(ev_base, server_fd,
                                        BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(server_bufev, server_read_cb, NULL, server_error_cb, this);
  bufferevent_enable(server_bufev, EV_READ);
}

// static
void Connection::client_error_cb(struct bufferevent* bufev, short error,
                                 void* arg) {
  Connection* conn = static_cast<Connection*>(arg);
  bool done = false;

  DLOG(2, "%s: called", __func__);

  if (error & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
    done = true;
  }
  if (done)
    delete conn;
}

// static
void Connection::client_read_cb(struct bufferevent* bufev, void* arg) {
  Connection* conn = static_cast<Connection*>(arg);
  struct evbuffer* input = bufferevent_get_input(bufev);
  size_t len;

  DLOG(2, "%s: called", __func__);

  while (1) {
    len = evbuffer_get_length(input);
    if (len <= 0)
      break;

    if (msn::parse_packet(0, input, conn) == -1)
      evbuffer_drain(input, len);
  }
}

// static
void Connection::server_error_cb(struct bufferevent* bufev, short error,
                                 void* arg) {
  Connection* conn = static_cast<Connection*>(arg);
  bool done = false;

  DLOG(2, "%s: called", __func__);

  if (error & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
    done = true;
  }
  if (done)
    delete conn;
}

// static
void Connection::server_read_cb(struct bufferevent* bufev, void* arg) {
  Connection* conn = static_cast<Connection*>(arg);
  struct evbuffer* input = bufferevent_get_input(bufev);
  size_t len;

  DLOG(2, "%s: called", __func__);

  while (1) {
    len = evbuffer_get_length(input);
    if (len <= 0)
      break;

    if (msn::parse_packet(1, input, conn) == -1)
      evbuffer_drain(input, len);
  }
}
