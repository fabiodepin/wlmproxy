/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "log.h"

#include <stdarg.h>   // va_list
#include <stdio.h>
#include <syslog.h>

// in main.cc
extern int verbose;
extern bool use_syslog;

static void _log(int level, const char* fmt, va_list args);

void log_init(void) {
  if (use_syslog)
    openlog("wlmproxy", LOG_PID|LOG_CONS, LOG_DAEMON);
}

void _log(int level, const char* fmt, va_list args) {
  if (use_syslog) {
    vsyslog(level, fmt, args);
  } else {
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
  }
}

void log_warn(const char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  _log(LOG_CRIT, fmt, ap);
  va_end(ap);
}

void log_info(const char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  _log(LOG_INFO, fmt, ap);
  va_end(ap);
}

void log_debug(int level, const char* fmt, ...) {
  va_list ap;

  if (verbose >= level) {
    va_start(ap, fmt);
    _log(LOG_DEBUG, fmt, ap);
    va_end(ap);
  }
}
