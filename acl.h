/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef ACL_H_
#define ACL_H_
#pragma once

#include <string>

struct event_base;

void acl_init(struct event_base* base);
bool acl_check_deny(const std::string& user, const std::string& who);

#endif // ACL_H_
