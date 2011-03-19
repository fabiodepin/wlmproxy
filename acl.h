/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef ACL_H_
#define ACL_H_
#pragma once

#include <string>

void acl_init();
bool acl_check_deny(const std::string& user, const std::string& who);

#endif // ACL_H_
