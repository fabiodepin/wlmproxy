/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "history/history_logger.h"

#include <cstdio>

#include "history/history.h"
#include "history/history_consumer.h"
#include "utils.h"
#include "log.h"

HistoryLogger* HistoryLogger::instance_ = NULL;

HistoryLogger::HistoryLogger()
    : consumer_(NULL) {
  consumer_ = new HistoryConsumer(queue_);
  consumer_->set_joinable(true);
  consumer_->start();
}

HistoryLogger::~HistoryLogger() {
}

HistoryLogger* HistoryLogger::instance() {
  if (instance_ == NULL)
    instance_ = new HistoryLogger();

  return instance_;
}

void HistoryLogger::destroy() {
  if (instance_) {
    consumer_->stop();
    consumer_->join();
    delete consumer_;
    consumer_ = NULL;

    delete instance_;
    instance_ = NULL;
  }
}

void HistoryLogger::log(History* history) {
  log_info("(%s) %s : %u : %s %s",
           history->timestamp().c_str(),
           utils::ip_to_string(history->address()).c_str(),
           history->is_inbound(),
           history->type_string(),
           history->is_filtered() ? "(filtered)" : "(unfiltered)");

  queue_.push(history);
}
