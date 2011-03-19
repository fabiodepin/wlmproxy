/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef CHAT_SESSION_H_
#define CHAT_SESSION_H_
#pragma once

#include <stdint.h>

#include <map>
#include <string>

#include <boost/noncopyable.hpp>

struct event;

class Connection;

class ChatSession : private boost::noncopyable {
 public:
  ChatSession(const Connection* conn, const std::string& contact, uint32_t id);
  ~ChatSession();

  bool set_idle_timeout(int seconds);

  bool warned() const { return warned_; }
  void set_warned(bool value) { warned_ = value; }

  bool renew() const { return renew_; }
  void set_renew(bool value) { renew_ = value; }

  const Connection* conn() const { return conn_; }
  const std::string& contact() const { return contact_; }
  uint32_t id() const { return id_; }

 private:
  bool init();

  static void timeout_cb(int fd, short flags, void* context);

  const Connection* conn_;
  const std::string contact_;
  event* timeout_;
  const uint32_t id_;
  bool warned_;
  bool renew_;
};

typedef std::map<std::string, ChatSession*> ChatMap;

#endif // CHAT_SESSION_H_
