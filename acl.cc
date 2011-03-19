/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "acl.h"

#include <ctime>

#include <map>
#include <vector>

#include <boost/scoped_ptr.hpp>
#include <dolphinconn/connection.h>
#include <dolphinconn/resultset.h>
#include <event.h>
#include <evutil.h>

#include "config.h"
#include "utils.h"
#include "log.h"

typedef std::pair<std::string, std::string> bar_pair;
typedef std::vector<bar_pair> foo_vector;
typedef std::map<bar_pair, bool> foo_map;

static foo_vector allowed;
static foo_vector denied;
static foo_map cache;

static struct event ev_refresh;

static bool load_acl(foo_vector& goodness, foo_vector& badness) {
  Config& config = Config::instance();
  boost::scoped_ptr<dolphinconn::Connection> db(new dolphinconn::Connection);
  if (!db->open(config["db_name"], config["db_user"], config["db_password"],
        config["db_host"], config.getint("db_port"), config["db_socket"])) {
    log_warn("MySQL error %d, SQLState %s: %s", db->get_last_errno(),
             db->get_sqlstate(), db->get_error_msg());
    return false;
  }

  boost::scoped_ptr<dolphinconn::ResultSet> res(db->execute_query("SELECT "
        "localim, remoteim, action FROM acls"));
  if (!res)
    return false;

  bar_pair bar;
  while (res->step()) {
    bar.first = res->column_string(0);
    bar.second = res->column_string(1);
    if (res->column_int(2) == 1)
      goodness.push_back(bar);
    else
      badness.push_back(bar);
  }

  return true;
}

// update everything.
static void refresh_acl(int fd, short event, void* arg) {
  struct timeval tv;

  evutil_timerclear(&tv);
  tv.tv_sec = 120;  // TODO: hardcoded.
  event_add(&ev_refresh, &tv);

  DLOG(1, "--== Refreshing acls ==--");

  foo_vector good_patterns;
  foo_vector bad_patterns;

  if (!load_acl(good_patterns, bad_patterns))
    return;

  allowed.swap(good_patterns);
  denied.swap(bad_patterns);
  cache.clear();
}

void acl_init() {
  struct timeval tv;

  evtimer_set(&ev_refresh, refresh_acl, NULL);
  evutil_timerclear(&tv);
  tv.tv_sec = 120;  // TODO: hardcoded.
  event_add(&ev_refresh, &tv);

  // initial load
  load_acl(allowed, denied);
}

static bool check_deny(const std::string& user, const std::string& who) {
  for (foo_vector::const_iterator it = allowed.begin();
       it != allowed.end(); ++it) {
    if (utils::match(user, it->first) &&
        utils::match(who, it->second)) {
      return false;
    }
  }

  for (foo_vector::const_iterator it = denied.begin();
       it != denied.end(); ++it) {
    if (utils::match(user, it->first) &&
        utils::match(who, it->second)) {
      return true;
    }
  }
  return false;
}

bool acl_check_deny(const std::string& user, const std::string& who) {
  if (allowed.empty() && denied.empty())
    return false;

  bar_pair bar(user, who);
  const foo_map::const_iterator it = cache.find(bar);
  if (it != cache.end()) {
    DLOG(2, "found cached result");
    return it->second;
  }

  bool ret = check_deny(user, who);

  cache.insert(std::make_pair(bar, ret));

  return ret;
}
