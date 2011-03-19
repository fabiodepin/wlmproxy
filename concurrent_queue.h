/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef CONCURRENT_QUEUE_H_
#define CONCURRENT_QUEUE_H_
#pragma once

#include <deque>

#include "thread/condition.h"
#include "thread/mutex.h"

// helper class implemented as a blocking queue
template <typename T>
class ConcurrentQueue {
 public:
  ConcurrentQueue() {}

  ~ConcurrentQueue() {
    while (!queue_.empty()) {
      queue_.pop_front();
    }
  }

  void push(const T& t) {
    mutex_.lock();

    const bool was_empty = queue_.empty();
    queue_.push_back(t);

    mutex_.unlock();

    if (was_empty)
      condition_.signal();
  }

  T pop() {
    MutexLocker lock(mutex_);

    while (queue_.empty()) {
      condition_.wait(lock);
    }

    T tmp = queue_.front();
    queue_.pop_front();
    return tmp;
  }

 private:
  Mutex mutex_;
  Condition condition_;
  std::deque<T> queue_;
};

#endif  // CONCURRENT_QUEUE_H_
