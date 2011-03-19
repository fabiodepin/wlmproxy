/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef HISTORY_HISTORY_H_
#define HISTORY_HISTORY_H_
#pragma once

#include <stdint.h>
#include <ctime>

#include <string>

class Command;

class History {
 public:
  enum Type {
    TYPE_UNKNOWN = 0,
    TYPE_MSG,
    TYPE_FILE,
    TYPE_TYPING,
    TYPE_CAPS,
    TYPE_WEBCAM,
    TYPE_REMOTEDESKTOP,
    TYPE_APPLICATION,
    TYPE_EMOTICON,
    TYPE_INK,
    TYPE_NUDGE,
    TYPE_WINK,
    TYPE_VOICECLIP,
    TYPE_GAMES,
    TYPE_PHOTO
  };

  static const char* type_to_text(Type type);

  explicit History(bool inbound);

  History(Command* cmd, Type type);

  Type type() const {
    return type_;
  }

  void set_type(Type type) {
    type_ = type;
  }

  uint32_t conversation_id() const {
    return conversation_id_;
  }

  void set_conversation_id(uint32_t conversation_id) {
    conversation_id_ = conversation_id;
  }

  const char* type_string() const {
    return type_to_text(type());
  }

  std::string timestamp() {
    char date[64];
    strftime(date, sizeof(date),
             "%Y-%m-%d %H:%M:%S", localtime(&timestamp_));
    return std::string(date);
  }

  bool is_inbound() const {
    return inbound_;
  }

  bool is_filtered() const {
    return filtered_;
  }

  void set_filtered(bool value) {
    filtered_ = value;
  }

  bool dont_log() const {
    return dont_log_;
  }

  void set_dont_log() {
    dont_log_ = true;
  }

  uint32_t address() const {
    return address_;
  }

  void set_address(const uint32_t address) {
    address_ = address;
  }

  const std::string& local_im() const {
    return local_im_;
  }

  void set_local_im(const std::string& local_im) {
    local_im_ = local_im;
  }

  const std::string& remote_im() const {
    return remote_im_;
  }

  void set_remote_im(const std::string& remote_im) {
    remote_im_ = remote_im;
  }

  const std::string& data() const {
    return data_;
  }

  void set_data(const std::string& data) {
    data_ = data;
  }

  const std::string& to() const {
    return inbound_ ? local_im_ : remote_im_;
  }

  const std::string& from() const {
    return inbound_ ? remote_im_ : local_im_;
  }

 private:
  time_t timestamp_;
  std::string local_im_;
  std::string remote_im_;
  std::string data_;
  uint32_t address_;
  uint32_t conversation_id_;
  Type type_;
  bool inbound_;
  bool filtered_;
  bool dont_log_;
};

#endif // HISTORY_HISTORY_H_
