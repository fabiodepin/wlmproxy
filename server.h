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

// TODO: Refactor the following class.
class Server : private boost::noncopyable {
 public:
  Server(const char* address, int port);
  ~Server();

 private:
  static void accept_cb(int listen_fd, short event, void* arg);

  struct event* ev_;
  int fd_;
};

#endif // SERVER_H_
