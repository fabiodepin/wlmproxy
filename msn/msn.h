/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef MSN_MSN_H_
#define MSN_MSN_H_
#pragma once

#include <string>

struct evbuffer;  // From libevent

class Connection;

namespace msn {

enum ProtocolVersion {
  MSNP21 = 21,
  MSNP20 = 20,
  MSNP18 = 18,
  MSNP16 = 16,
  MSNP15 = 15,
  MSNP14 = 14,
  MSNP13 = 13,
  MSNP12 = 12,
  MSNP9 = 9,
  MSNP8 = 8
};

void msn_init(void);
void destroy_cb(Connection* conn);
void drop_chat(Connection* conn, const std::string& buddy);
int parse_packet(bool inbound, struct evbuffer* input, Connection* conn);

} // namespace msn

#endif // MSN_MSN_H_
