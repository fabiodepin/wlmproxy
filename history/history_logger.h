/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef HISTORY_HISTORY_LOGGER_H_
#define HISTORY_HISTORY_LOGGER_H_
#pragma once

#include <boost/noncopyable.hpp>

#include "concurrent_queue.h"

class History;
class HistoryConsumer;

class HistoryLogger : private boost::noncopyable {
 public:
  ~HistoryLogger();

  static HistoryLogger* instance();

  void destroy();

  void log(History* history);

 private:
  HistoryLogger();

  static HistoryLogger* instance_;

  ConcurrentQueue<History*> queue_;

  HistoryConsumer* consumer_;
};

#endif // HISTORY_HISTORY_LOGGER_H_
