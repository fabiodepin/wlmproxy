/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef THREAD_MUTEX_H_
#define THREAD_MUTEX_H_
#pragma once

#include <pthread.h>
#include <boost/noncopyable.hpp>

class Mutex : private boost::noncopyable {
 public:
  Mutex();
  ~Mutex();

  bool try_lock();

  void lock();

  void unlock();

  pthread_mutex_t* mutex_handle() { return &mutex_; }

 private:
  pthread_mutex_t mutex_;
};

class MutexLocker : private boost::noncopyable {
 public:
  explicit MutexLocker(Mutex& m) : mutex_(&m) {
    mutex_->lock();
  }

  ~MutexLocker() { mutex_->unlock(); }

  Mutex* mutex() const { return mutex_; }

 private:
  Mutex* mutex_;
};

#endif  // THREAD_MUTEX_H_
