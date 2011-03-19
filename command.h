/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef COMMAND_H_
#define COMMAND_H_
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "history/history.h"
#include "defs.h"

class Connection;

class Command {
 public:
  // flags that a command takes
  enum Flags {
    INBOUND = SET_BIT(1),
    CHUNKED = SET_BIT(2),
    ENCRYPTED = SET_BIT(3),
    WAITING_FOR_ACK = SET_BIT(4),
    IGNORE = SET_BIT(5)
  };

  explicit Command(Connection* conn)
    : payload_len(0), conn(conn), hist(NULL), trid(0), flags_(0) {}

  Command(Connection* conn, uint16_t flags)
    : payload_len(0), conn(conn), hist(NULL), trid(0), flags_(flags) {}

  ~Command() {
    if (hist != NULL)
      delete hist;
  }

  uint16_t flags() const { return flags_; }
  void set_flags(uint16_t flags) { flags_ |= flags; }
  void clear_flags(uint16_t flags) { flags_ &= ~flags; }

  bool is_inbound() const { return flags_ & INBOUND; }
  bool is_chunked() const { return flags_ & CHUNKED; }
  bool is_encrypted() const { return flags_ & ENCRYPTED; }
  bool should_send_ack() const { return flags_ & WAITING_FOR_ACK; }
  bool should_ignore() const { return flags_ & IGNORE; }

  std::vector<std::string> args;
  std::string payload;
  size_t payload_len;

  std::string cookie;

  Connection* conn;
  History* hist;

  uint32_t trid;

 private:
  uint16_t flags_;
};

#endif // COMMAND_H_
