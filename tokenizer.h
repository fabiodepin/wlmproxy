/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef TOKENIZER_H_
#define TOKENIZER_H_
#pragma once

#include <cctype>
#include <string>

// NOTE: most of the code here is based on chrome's
//   base/string_tokenizer.h
// The license block is:

/* ***** BEGIN LICENSE BLOCK *****
 * Copyright (c) 2010 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

template <
  typename str = std::string,
  typename const_iterator = std::string::const_iterator
>
class tokenizer {
 public:
  typedef typename str::value_type char_type;

  tokenizer(const_iterator first,
            const_iterator last,
            const str& delims = "")
      : first_(first),
        end_(first),
        last_(last),
        delims_(delims) { }

  tokenizer(const str& string, const str& delims = "")
      : first_(string.begin()),
        end_(string.begin()),
        last_(string.end()),
        delims_(delims) { }

  bool has_next() {
    for (;;) {
      begin_ = end_;
      if (end_ == last_)
        return false;
      ++end_;
      if (!is_delim(*begin_))
        break;
    }
    while (end_ != last_ && !is_delim(*end_))
      ++end_;
    return true;
  }

  void reset() {
    end_ = first_;
  }

  str token() const { return str(begin_, end_); }

 private:
  bool is_delim(char_type c) const {
    if (delims_.length())
      return delims_.find(c) != str::npos;
    else
      return isspace(c) || ispunct(c);  // default delimiter behavior.
  }

  const_iterator first_;
  const_iterator begin_;
  const_iterator end_;
  const_iterator last_;
  str delims_;
};

#endif  // TOKENIZER_H_
