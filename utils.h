/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef UTILS_H_
#define UTILS_H_
#pragma once

#include <inttypes.h>
#include <string>

namespace utils {

std::string decode_url(const std::string& url);
std::string escape_payload(const std::string& data, size_t datalen);
std::string base64_encode(const std::string& input, bool olb64);
std::string base64_decode(const std::string& input, bool olb64);
std::string u16_to_u8(const uint16_t* src, size_t n);
bool match(const std::string& str, const std::string& against);
std::string ip_to_string(uint32_t addr);

} // namespace utils

#endif // UTILS_H_
