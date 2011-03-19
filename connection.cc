/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "connection.h"

#include <unistd.h>

#include <set>

#include <event.h>
#include <evutil.h>

#include "msn/msn.h"
#include "log.h"

std::set<Connection*> connections;

static uint32_t num_connections = 1;

Connection::Connection(int fd)
    : session(new Session()),
      client_bufev(NULL),
      server_bufev(NULL),
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
    evbuffer_write(client_bufev->output, client_fd);
  if (server_bufev && server_fd != -1)
    evbuffer_write(server_bufev->output, server_fd);

  if (client_fd != -1)
    EVUTIL_CLOSESOCKET(client_fd);
  if (server_fd != -1)
    EVUTIL_CLOSESOCKET(server_fd);

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
  client_bufev = bufferevent_new(client_fd, client_read_cb, NULL,
                                 client_error_cb, this);
  bufferevent_enable(client_bufev, EV_READ);

  server_bufev = bufferevent_new(server_fd, server_read_cb, NULL,
                                 server_error_cb, this);
  bufferevent_enable(server_bufev, EV_READ);
}

// static
void Connection::client_error_cb(struct bufferevent* bufev, short error,
                                 void* arg) {
  Connection* conn = static_cast<Connection*>(arg);
  bool done = false;

  DLOG(2, "%s: called", __func__);

  if (error & EVBUFFER_EOF) {
    // connection has been closed, do any clean up here
    done = true;
  } else if (error & EVBUFFER_ERROR) {
    // check errno to see what error occurred
    done = true;
  }
  if (done)
    delete conn;
}

// static
void Connection::client_read_cb(struct bufferevent* bufev, void* arg) {
  Connection* conn = static_cast<Connection*>(arg);
  struct evbuffer* input = EVBUFFER_INPUT(bufev);
  size_t len;

  DLOG(2, "%s: called", __func__);

  while (1) {
    len = EVBUFFER_LENGTH(input);
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

  if (error & EVBUFFER_EOF) {
    // connection has been closed, do any clean up here
    done = true;
  } else if (error & EVBUFFER_ERROR) {
    // check errno to see what error occurred
    done = true;
  }
  if (done)
    delete conn;
}

// static
void Connection::server_read_cb(struct bufferevent* bufev, void* arg) {
  Connection* conn = static_cast<Connection*>(arg);
  struct evbuffer* input = EVBUFFER_INPUT(bufev);
  size_t len;

  DLOG(2, "%s: called", __func__);

  while (1) {
    len = EVBUFFER_LENGTH(input);
    if (len <= 0)
      break;

    if (msn::parse_packet(1, input, conn) == -1)
      evbuffer_drain(input, len);
  }
}
