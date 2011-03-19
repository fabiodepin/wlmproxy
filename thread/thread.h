/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef THREAD_THREAD_H_
#define THREAD_THREAD_H_
#pragma once

#include <pthread.h>
#include <boost/noncopyable.hpp>

class Thread : private boost::noncopyable {
 public:
  Thread() : joined_(false), joinable_(false) { }

  virtual ~Thread() { }

  void set_joinable(bool value) { joinable_ = value; }

  void start();

  void join();

  virtual void run() = 0;

  pthread_t thread_handle() { return thread_; }

  bool joined() const { return joined_; }

 private:
  static void* do_run(void* param);

  pthread_t thread_;
  bool joined_;
  bool joinable_;
};

#endif // THREAD_THREAD_H_
