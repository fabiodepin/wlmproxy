/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "history/history.h"

#include "connection.h"
#include "command.h"

History::History(bool inbound)
    : conversation_id_(0),
      type_(TYPE_UNKNOWN),
      inbound_(inbound),
      filtered_(false),
      dont_log_(false) {
  timestamp_ = time(NULL);
}

History::History(Command* cmd, Type type)
    : address_(cmd->conn->client_addr),
      type_(type),
      inbound_(cmd->is_inbound()),
      filtered_(false),
      dont_log_(false) {
  timestamp_ = time(NULL);

  const SessionPointer sess = cmd->conn->session;
  local_im_ = sess->user;

  if (cmd->conn->type == Connection::SB) {
    remote_im_ = inbound_ ? cmd->args[1] : sess->members.front();
    conversation_id_ = sess->chat_id;
  }
}

// static
const char* History::type_to_text(Type type) {
  switch (type) {
  case TYPE_MSG:
    return "msg";
  case TYPE_FILE:
    return "file";
  case TYPE_TYPING:
    return "typing";
  case TYPE_CAPS:
    return "caps";
  case TYPE_WEBCAM:
    return "webcam";
  case TYPE_REMOTEDESKTOP:
    return "remotedesktop";
  case TYPE_APPLICATION:
    return "application";
  case TYPE_EMOTICON:
    return "emoticon";
  case TYPE_INK:
    return "ink";
  case TYPE_NUDGE:
    return "nudge";
  case TYPE_WINK:
    return "wink";
  case TYPE_VOICECLIP:
    return "voiceclip";
  case TYPE_GAMES:
    return "games";
  case TYPE_PHOTO:
    return "photo";
  case TYPE_UNKNOWN:
  default:
    return "unknown";
  }
}
