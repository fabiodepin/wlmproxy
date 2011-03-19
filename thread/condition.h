/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef THREAD_CONDITION_H_
#define THREAD_CONDITION_H_
#pragma once

#include <pthread.h>
#include <boost/noncopyable.hpp>

class Mutex;
class MutexLocker;

class Condition : private boost::noncopyable {
 public:
  Condition();
  ~Condition();

  void wait(MutexLocker& m);

  void wait(Mutex& m);

  void broadcast();

  void signal();

 private:
  pthread_cond_t condition_;
};

#endif  // THREAD_CONDITION_H_
