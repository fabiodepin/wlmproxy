/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef MSN_MSN_DATABASE_H_
#define MSN_MSN_DATABASE_H_
#pragma once

#include <stdint.h>
#include <string>

#include <boost/noncopyable.hpp>
#include <dolphinconn/connection.h>

#include "thread/mutex.h"

namespace msn {

class MsnDatabase : private boost::noncopyable {
 public:
  MsnDatabase() { }

  bool init();
  bool cleanup();

  dolphinconn::Connection& db() { return db_; }

  uint64_t get_chat_id(const std::string& user);
  bool delete_chat(uint64_t chat_id);
  bool add_user(const std::string& user);
  bool can_login(const std::string& user);
  bool set_login_time(const std::string& user);
  bool set_status(const std::string& user, const std::string& status);
  bool set_friendly_name(const std::string& user, const std::string& name);
  bool set_status_message(const std::string& user, const char* msg);
  bool user_logoff(const std::string& user);
  bool add_buddy(const std::string& user, const std::string& who);
  bool buddy_logoff(const std::string& user, const std::string& who);
  bool update_buddy(const std::string& user, const std::string& who,
                    const std::string& status, const std::string& name);
  bool update_buddy_status(const std::string& user, const std::string& who,
                           const std::string& status);
  bool set_buddy_friendly_name(const std::string& user,
                               const std::string& who,
                               const std::string& name);
  bool set_buddy_status_message(const std::string& user,
                                const std::string& who,
                                const char* msg);
  bool buddy_is_blocked(const std::string& user, const std::string& who);
  bool check_version(int version);
  bool has_rule(const std::string& user, int type);
  std::string get_rule_value(int type);
  std::string get_setting(const std::string& name);

 private:
  Mutex mutex_;
  dolphinconn::Connection db_;
};

} // namespace msn

#endif // MSN_MSN_DATABASE_H_
