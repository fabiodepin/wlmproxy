/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef WORD_FILTER_H_
#define WORD_FILTER_H_
#pragma once

#include <string>

struct event_base;

void word_filter_init(struct event_base* base);
bool word_filter_check(const std::string& str);

#endif // WORD_FILTER_H_
