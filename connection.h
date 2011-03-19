/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef CONNECTION_H_
#define CONNECTION_H_
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include <boost/noncopyable.hpp>

#include "command.h"
#include "chat_session.h"

struct bufferevent;

class Connection : private boost::noncopyable {
 public:
  enum ConnType {
    NONE,
    NS = 1,
    SB = 2
  };

  struct Session {
    Session()
        : chat_id(0),
          version(0),
          connecting(false),
          warned(false) {}

    std::vector<std::string> members;
    ChatMap chat_sessions;
    std::string user;
    std::string epid;
    uint32_t chat_id;
    uint8_t version;
    bool connecting;
    bool warned;
  };

  explicit Connection(int fd);
  ~Connection();

  void start();

  Command* cmd[2];
  Session* session;

  struct bufferevent* client_bufev;
  struct bufferevent* server_bufev;
  uint32_t client_addr;
  uint32_t server_addr;
  uint16_t client_port;
  uint16_t server_port;
  uint32_t id;
  int client_fd;
  int server_fd;
  ConnType type;

 private:
  static void client_error_cb(struct bufferevent* bufev, short error,
                              void* arg);
  static void client_read_cb(struct bufferevent* bufev, void* arg);
  static void server_error_cb(struct bufferevent* bufev, short error,
                              void* arg);
  static void server_read_cb(struct bufferevent* bufev, void* arg);
};

typedef Connection::Session* SessionPointer;

#endif // CONNECTION_H_
