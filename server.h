/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef SERVER_H_
#define SERVER_H_
#pragma once

#include <boost/noncopyable.hpp>

struct event;
struct event_base;

// TODO: Refactor the following class.
class Server : private boost::noncopyable {
 public:
  Server(struct event_base* base, const char* address, int port);
  ~Server();

  struct event_base* ev_base() const { return base_; }

 private:
  static void accept_cb(int listen_fd, short event, void* arg);

  struct event_base* base_;
  struct event* ev_;
  int fd_;
};

#endif // SERVER_H_
