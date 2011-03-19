/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "server.h"

#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <climits>
#include <cstring>

#include <linux/netfilter_ipv4.h>

#include <event.h>
#include <evutil.h>

#include "connection.h"
#include "log.h"
#include "utils.h"

Server::Server(const char* address, int port)
    : ev_(new struct event),
      fd_(-1) {
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  inet_aton(address, &sin.sin_addr);

  // TODO: error handling
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  evutil_make_socket_nonblocking(fd_);

  int on = 1;
  setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<char*>(&on), sizeof(on));

  if (bind(fd_, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) < 0) {
    err(1, "bind");
  }

  if (listen(fd_, 16) < 0) {
    err(1, "listen");
  }

  event_set(ev_, fd_, EV_READ|EV_PERSIST, accept_cb, this);
  event_add(ev_, NULL);
}

Server::~Server() {
  event_del(ev_);
  delete ev_;
  if (fd_ != -1)
    EVUTIL_CLOSESOCKET(fd_);
}

// static
void Server::accept_cb(int listen_fd, short event, void* arg) {
  struct sockaddr_in client_sa;
  struct sockaddr_in server_sa;
  socklen_t slen;
  int client_fd;

  DLOG(2, "%s: called", __func__);

  slen = sizeof(client_sa);
  client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&client_sa), &slen);
  if (client_fd == -1) {
    if (errno != EAGAIN && errno != EINTR)
      warn("%s: bad accept", __func__);
    return;
  }

  if (evutil_make_socket_nonblocking(client_fd) < 0)
    return;

  Connection* conn = new Connection(client_fd);

  slen = sizeof(server_sa);
  if (getsockopt(conn->client_fd, SOL_IP, SO_ORIGINAL_DST, &server_sa,
                 &slen) < 0) {
    warn("%s: getsockopt for %d", __func__, conn->client_fd);
    goto out;
  }

  conn->client_addr = client_sa.sin_addr.s_addr;
  conn->server_addr = server_sa.sin_addr.s_addr;
  conn->client_port = ntohs(client_sa.sin_port);
  conn->server_port = ntohs(server_sa.sin_port);

  if ((conn->server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    warn("socket");
    goto out;
  }

  if (evutil_make_socket_nonblocking(conn->server_fd) < 0)
    goto out;

  if (connect(conn->server_fd, reinterpret_cast<sockaddr*>(&server_sa),
              sizeof(server_sa)) < 0 && errno != EINPROGRESS) {
    warn("connect");
    goto out;
  }

  log_info("%u: new connection to %s:%hu for %s",
           conn->id,
           utils::ip_to_string(conn->server_addr).c_str(),
           conn->server_port,
           utils::ip_to_string(conn->client_addr).c_str());

  conn->start();

  return;

out:
  delete conn;
}
