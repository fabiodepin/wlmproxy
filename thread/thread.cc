/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "thread/thread.h"

#include <cassert>

void Thread::start() {
  pthread_attr_t attr;
  pthread_attr_init(&attr);

  if (!joinable_) {
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  }

  bool success = !pthread_create(&thread_, &attr, &do_run, this);

  pthread_attr_destroy(&attr);

  assert(success);
}

void Thread::join() {
  pthread_join(thread_, NULL);
  joined_ = true;
}

// static
void* Thread::do_run(void* param) {
  Thread* thr = reinterpret_cast<Thread*>(param);
  thr->run();
  return NULL;
}
