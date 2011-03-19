/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef HISTORY_HISTORY_CONSUMER_H_
#define HISTORY_HISTORY_CONSUMER_H_
#pragma once

#include <sstream>
#include <string>

#include <dolphinconn/connection.h>

#include "concurrent_queue.h"
#include "thread/thread.h"
#include "history/history.h"
#include "config.h"
#include "log.h"

class HistoryConsumer : public Thread {
 public:
  typedef ConcurrentQueue<History*> HistoryQueue;

  explicit HistoryConsumer(HistoryQueue& queue)
      : queue_(queue) {}

  void run() {
    dolphinconn::Connection db;

    std::ostringstream sql;

    Config& config = Config::instance();
    bool ok = db.open(
        config["db_name"], config["db_user"], config["db_password"],
        config["db_host"], config.getint("db_port"), config["db_socket"]);

    if (!ok) {
      log_warn("MySQL error %d, SQLState %s: %s", db.get_last_errno(),
               db.get_sqlstate(), db.get_error_msg());
    }

    for (;;) {
      History* hist = queue_.pop();

      bool quit_loop = hist == NULL;
      if (quit_loop)
        break;

      if (ok) {
        sql.str("");

        sql << "INSERT INTO messages(timestamp, conversation_id, clientip, ";
        sql << "inbound, type, localim, remoteim, filtered, content) VALUES ('";
        sql << hist->timestamp() << "', ";
        sql << hist->conversation_id() << ", ";
        sql << hist->address() << ", ";
        sql << hist->is_inbound() << ", ";
        sql << hist->type() << ", '";
        sql << hist->local_im() << "', '";
        sql << hist->remote_im() << "', ";
        sql << hist->is_filtered() << ", '";
        sql << db.escape(hist->data()) << "')";

        db.execute(sql.str());
      }

      delete hist;
    }
  }

  void stop() {
    queue_.push(NULL);
  }

 private:
  HistoryQueue& queue_;
};

#endif // HISTORY_HISTORY_CONSUMER_H_
