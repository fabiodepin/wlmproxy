/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef LOG_H_
#define LOG_H_
#pragma once

#ifdef DEBUG
#define DLOG log_debug
#else
#define DLOG
#endif

void log_init(void);
void log_warn(const char* fmt, ...);
void log_info(const char* fmt, ...);
void log_debug(int level, const char* fmt, ...);

#endif // LOG_H_
