/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "chat_session.h"

#include <ctime>
#include <cassert>

#include <event.h>
#include <evutil.h>

#include "connection.h"
#include "msn/msn.h"

ChatSession::ChatSession(const Connection* conn, const std::string& contact,
                         uint32_t id)
    : conn_(conn),
      contact_(contact),
      id_(id),
      warned_(false),
      renew_(false) {
  assert(init());
}

bool ChatSession::init() {
  timeout_ = new event;
  evtimer_set(timeout_, timeout_cb, this);

  return set_idle_timeout(60);  // TODO: hardcoded.
}

ChatSession::~ChatSession() {
  evtimer_del(timeout_);
  delete timeout_;
}

bool ChatSession::set_idle_timeout(int seconds) {
  struct timeval tv;

  evutil_timerclear(&tv);
  tv.tv_sec = seconds;

  if (event_add(timeout_, &tv))
    return false;
  return true;
}

// static
void ChatSession::timeout_cb(int fd, short flags, void* context) {
  ChatSession* that = static_cast<ChatSession*>(context);

  if (that->renew()) {
    that->set_idle_timeout(60);  // TODO: hardcoded.

    that->set_renew(false);
  } else {
    msn::drop_chat(const_cast<Connection*>(that->conn()), that->contact());

    delete that;
  }
}
