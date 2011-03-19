/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "word_filter.h"

#include <ctime>
#include <cctype>

#include <algorithm>
#include <set>
#include <vector>

#include <boost/regex.hpp>
#include <boost/scoped_ptr.hpp>
#include <dolphinconn/connection.h>
#include <dolphinconn/resultset.h>
#include <event.h>
#include <evutil.h>

#include "tokenizer.h"
#include "config.h"
#include "utils.h"
#include "log.h"

namespace {

struct ltstr {
  static bool nocase_compare(char c1, char c2) {
    return std::tolower(c1) < std::tolower(c2);
  }
  bool operator()(const std::string& s1, const std::string& s2) const {
    return std::lexicographical_compare(s1.begin(), s1.end(),
                                        s2.begin(), s2.end(),
                                        nocase_compare);
  }
};

}  // namespace

typedef std::set<std::string, ltstr> word_set;
typedef std::vector<std::string> pattern_list;

static word_set words;
static pattern_list patterns;

static struct event ev_timeout;

static bool load_words(word_set& foo, pattern_list& bar) {
  Config& config = Config::instance();
  boost::scoped_ptr<dolphinconn::Connection> db(new dolphinconn::Connection);
  if (!db->open(config["db_name"], config["db_user"], config["db_password"],
        config["db_host"], config.getint("db_port"), config["db_socket"])) {
    log_warn("MySQL error %d, SQLState %s: %s", db->get_last_errno(),
             db->get_sqlstate(), db->get_error_msg());
    return false;
  }

  boost::scoped_ptr<dolphinconn::ResultSet> res(db->execute_query("SELECT "
        "badword, isregex FROM badwords WHERE isenabled = 1"));
  if (!res)
    return false;

  std::string tmp;
  while (res->step()) {
    tmp = res->column_string(0);
    if (res->column_int(1) == 0)
      foo.insert(tmp);
    else
      bar.push_back(tmp);
  }

  return true;
}

static void reload_words(int fd, short event, void* arg) {
  struct timeval tv;

  evutil_timerclear(&tv);
  tv.tv_sec = 300;  // TODO: hardcoded.
  event_add(&ev_timeout, &tv);

  DLOG(1, "--== Reloading words ==--");

  word_set foo;
  pattern_list bar;

  if (!load_words(foo, bar))
    return;

  words.swap(foo);
  patterns.swap(bar);
}

void word_filter_init() {
  struct timeval tv;

  evtimer_set(&ev_timeout, reload_words, NULL);
  evutil_timerclear(&tv);
  tv.tv_sec = 300;  // TODO: hardcoded.
  event_add(&ev_timeout, &tv);

  // initial load
  load_words(words, patterns);
}

bool word_filter_check(const std::string& str) {
  tokenizer<> t(str);
  while (t.has_next()) {
    if (words.count(t.token()))
      return true;
  }
  boost::regex re;
  for (size_t i = 0; i < patterns.size(); ++i) {
    re.assign(patterns[i], boost::regex::perl|boost::regex::icase|boost::regex::no_except);
    if (re.empty())
      continue;
    if (boost::regex_match(str, re))
      return true;
  }
  return false;
}
