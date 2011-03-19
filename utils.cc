/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "utils.h"

#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>

#include <boost/scoped_array.hpp>
#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

namespace utils {

std::string decode_url(const std::string& url) {
  size_t pos = 0, next;
  std::string out;
  out.reserve(url.length());

  while ((next = url.find('%', pos)) != std::string::npos) {
    out.append(url, pos, next - pos);
    pos = next;
    switch (url[pos]) {
      case '%': {
        if (url.length() - next < 3)
          return "";
        char hex[3] = { url[next + 1], url[next + 2], '\0' };
        char* end_ptr;
        out += static_cast<char>(strtol(hex, &end_ptr, 16));
        if (*end_ptr)
          return "";
        pos = next + 3;
        break;
      }
    }
  }

  out.append(url, pos, url.length());

  return out;
}

std::string escape_payload(const std::string& data, size_t datalen) {
  std::string payload;
  size_t i, count = 0;

  for (i = 0; i < datalen; ++i) {
    unsigned char c = static_cast<unsigned char>(data[i]);
    if (isprint(c) || isblank(c)) {
      count += 1;
    } else {
      count += 3;
    }
  }

  payload.reserve(count + 1);

  for (i = 0; i < datalen; ++i) {
    unsigned char c = static_cast<unsigned char>(data[i]);
    if (isprint(c) || isblank(c)) {
      payload.append(1, static_cast<char>(c));
    } else {
      char buf[4];

      snprintf(buf, sizeof(buf), "\\%02x", c);

      payload.append(static_cast<const char*>(buf), 3);
    }
  }

  return payload;
}

std::string base64_encode(const std::string& input, bool olb64) {
  BIO *bmem = NULL, *b64 = NULL, *bio = NULL;
  BUF_MEM *bptr = NULL;

  int input_size = static_cast<int>(input.size());
  bmem = BIO_new(BIO_s_mem());
  bio = bmem;
  b64 = BIO_new(BIO_f_base64());
  if (olb64)
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  bio = BIO_push(b64, bio);
  BIO_write(bio, const_cast<char*>(input.data()), input_size);
  static_cast<void>(BIO_flush(bio));
  BIO_get_mem_ptr(bio, &bptr);
  int output_size = bptr->length;

  boost::scoped_array<char> buf(new char[output_size]);
  memcpy(buf.get(), bptr->data, output_size);

  BIO_free_all(bio);

  return std::string(buf.get(), output_size);
}

std::string base64_decode(const std::string& input, bool olb64) {
  BIO *bmem = NULL, *b64 = NULL, *bio = NULL;

  int input_size = static_cast<int>(input.size());
  bmem = BIO_new_mem_buf(const_cast<char*>(input.data()), input_size);
  bio = bmem;
  b64 = BIO_new(BIO_f_base64());
  if (olb64)
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  bio = BIO_push(b64, bio);
  int output_size = BIO_ctrl_pending(bio);

  boost::scoped_array<char> buf(new char[output_size]);
  output_size = BIO_read(bio, buf.get(), output_size);

  BIO_free(bmem);
  BIO_free(b64);
  if (output_size <= 0)
    return "";
  return std::string(buf.get(), output_size);
}

namespace {

void u16_to_u8(uint16_t wc, std::string& out) {
  if (wc < 0x80u) {
    out += static_cast<char>(static_cast<uint8_t>(wc));
  } else if (wc < 0x800u) {
    out += static_cast<char>(static_cast<uint8_t>(0xc0u | (wc >> 6)));
    out += static_cast<char>(static_cast<uint8_t>(0x80u | (wc & 0x3fu)));
  } else {
    out += static_cast<char>(static_cast<uint8_t>(0xe0u | (wc >> 12)));
    out += static_cast<char>(static_cast<uint8_t>(0x80u | ((wc >> 6) & 0x3fu)));
    out += static_cast<char>(static_cast<uint8_t>(0x80u | (wc & 0x3fu)));
  }
}

}  // namespace

std::string u16_to_u8(const uint16_t* src, size_t n) {
  std::string ret;
  ret.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    if (src[i] == 0)
      break;

    u16_to_u8(src[i], ret);
  }
  return ret;
}

static bool wildcard_match(const char* eval, const char* pattern) {
  while (*pattern) {
    if (pattern[0] == '*') {
      if (!*++pattern) {
        return true;
      }

      while (*eval) {
        if ((tolower(*pattern) == tolower(*eval)) && wildcard_match(eval + 1, pattern + 1))
          return true;
        eval++;
      }
      return false;
    } else {
      if (tolower(*pattern) != tolower(*eval)) {
        return false;
      }
      eval++;
      pattern++;
    }
  }

  return !*eval;
}

bool match(const std::string& str, const std::string& against) {
  return wildcard_match(str.c_str(), against.c_str());
}

std::string ip_to_string(uint32_t addr) {
  char ip_buf[INET_ADDRSTRLEN];
  const char* ip =
    inet_ntop(AF_INET, &addr, ip_buf, sizeof(ip_buf));
  if (ip == NULL)
    return std::string();
  return ip;
}

} // namespace utils
