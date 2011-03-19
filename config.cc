/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "config.h"

#include <cstdlib>

#include <fstream>

Config* Config::instance_ = NULL;

Config::Config() {
}

Config::~Config() {
  instance_ = NULL;
}

// static
Config& Config::instance() {
  if (instance_ == NULL)
    instance_ = new Config;

  return *instance_;
}

// static
void Config::destroy() {
  delete instance_;
}

namespace {

std::string trim_ws(const std::string& s) {
  std::string::size_type pos = s.find_first_not_of(" \t\r\n");
  if (pos == std::string::npos) {
    return std::string();
  } else {
    std::string::size_type pos2 = s.find_last_not_of(" \t\r\n");
    return s.substr(pos, pos2 - pos + 1);
  }
}

}  // end namespace

bool Config::read(const char* filename) {
  std::ifstream file;
  file.open(filename);
  if (!file.is_open())
    return false;

  std::string s;
  size_t n;
  while (!file.eof()) {
    getline(file, s);
    // strip '#' comments and whitespace
    if ((n = s.find('#')) != std::string::npos)
      s = s.substr(0, n);
    s = trim_ws(s);

    if (!s.empty()) {
      if ((n = s.find('=')) != std::string::npos) {
        std::string name = trim_ws(s.substr(0, n));
        map_[name] = trim_ws(s.substr(n+1));
      }
    }
  }
  file.close();

  return true;
}

std::string Config::get(const std::string& name) {
  return map_[name];
}

int Config::getint(const std::string& name) {
  return static_cast<int>(strtol(map_[name].c_str(), NULL, 10));
}

std::string Config::operator[](const std::string& name) {
  return map_[name];
}
