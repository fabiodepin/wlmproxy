/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef CONFIG_H_
#define CONFIG_H_
#pragma once

#include <map>
#include <string>

class Config {
 public:
  static Config& instance();
  static void destroy();

  bool read(const char* filename);

  std::string get(const std::string& name);
  int getint(const std::string& name);

  std::string operator[](const std::string& name);

 private:
  Config();
  ~Config();

  static Config* instance_;

  std::map<std::string, std::string> map_;
};

#endif // CONFIG_H_
