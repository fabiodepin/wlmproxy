/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "thread/mutex.h"

#include <cassert>
#include <errno.h>

Mutex::Mutex() {
  int rv = pthread_mutex_init(&mutex_, NULL);
  assert(rv == 0);
}

Mutex::~Mutex() {
  int rv = pthread_mutex_destroy(&mutex_);
  assert(rv == 0);
}

bool Mutex::try_lock() {
  int rv = pthread_mutex_trylock(&mutex_);
  assert(rv == 0 || rv == EBUSY);
  return rv == 0;
}

void Mutex::lock() {
  int rv = pthread_mutex_lock(&mutex_);
  assert(rv == 0);
}

void Mutex::unlock() {
  int rv = pthread_mutex_unlock(&mutex_);
  assert(rv == 0);
}
