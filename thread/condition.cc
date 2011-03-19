/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "thread/condition.h"

#include <cassert>

#include "thread/mutex.h"

Condition::Condition() {
  int rv = pthread_cond_init(&condition_, NULL);
  assert(rv == 0);
}

Condition::~Condition() {
  int rv = pthread_cond_destroy(&condition_);
  assert(rv == 0);
}

void Condition::wait(MutexLocker& m) {
  int rv = pthread_cond_wait(&condition_, m.mutex()->mutex_handle());
  assert(rv == 0);
}

void Condition::wait(Mutex& m) {
  int rv = pthread_cond_wait(&condition_, m.mutex_handle());
  assert(rv == 0);
}

void Condition::broadcast() {
  int rv = pthread_cond_broadcast(&condition_);
  assert(rv == 0);
}

void Condition::signal() {
  int rv = pthread_cond_signal(&condition_);
  assert(rv == 0);
}
