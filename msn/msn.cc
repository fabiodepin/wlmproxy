/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "msn/msn.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <boost/scoped_array.hpp>
#include <boost/lexical_cast.hpp>
#include <event.h>

#include "connection.h"
#include "chat_session.h"
#include "msn/msn_database.h"
#include "history/history.h"
#include "history/history_logger.h"
#include "acl.h"
#include "word_filter.h"
#include "tokenizer.h"
#include "defs.h"
#include "utils.h"
#include "log.h"

#define RAPIDXML_NO_EXCEPTIONS
#include "rapidxml.hpp"

namespace rapidxml {

void parse_error_handler(const char* what, void* where) {
  log_warn("rapidxml::parse_error: %s", what);
  exit(1);
}

} // namespace rapidxml

using boost::lexical_cast;

extern bool show_payload; // in main.cc

namespace {

typedef std::map<std::string, std::string> string_map;

string_map parse_headers(const std::string& headers) {
  using std::string;

  string_map map;

  if (!headers.size())
    return map;

  tokenizer<> tk(headers.begin(), headers.end(), "\r\n");
  while (tk.has_next()) {
    std::string token = tk.token();
    size_t separator = token.find(":");
    if (separator != string::npos) {
      string key = token.substr(0, separator);
      string value;
      if (separator + 1 < token.size()) {
        size_t value_index = token.find_first_not_of(" \t", separator + 1);
        if (value_index != string::npos)
          value = token.substr(value_index);
      }
      map[key] = value;
      DLOG(2, "\t%s: %s", key.c_str(), value.c_str());
    }
  }

  return map;
}

typedef void (*cmd_cb)(Command* cmd);
typedef void (*msg_cb)(Command* cmd, const std::string& mime,
                       const std::string& body);

typedef std::map<std::string, cmd_cb> cmd_map;
typedef std::map<std::string, msg_cb> msg_map;

typedef std::set<std::string> payload_set;

static cmd_map commands;
static msg_map messages;

static payload_set payload_commands_from_client;
static payload_set payload_commands_from_server;

static msn::MsnDatabase db;

const char* const circle = ";via=9:";

static void parse_cmd(Command* cmd);
static bool is_payload(const std::string& cmd, const payload_set& payload_cmds);
static void send_command(struct bufferevent* bufev, const std::string& cmd);
static void send_message(struct bufferevent* bufev,
                         const std::vector<std::string>& args,
                         const std::string& payload);
static bool check_login(const Command* cmd);
static bool check_filter(const History* hist, bool encrypted);
static void do_notifies(Command* cmd);
static void send_notify(Command* cmd, const std::string& msg);
static void send_cancel_message(Command* cmd);

static void ans_cmd(Command* cmd);
static void iro_cmd(Command* cmd);
static void usr_cmd(Command* cmd);
static void ver_cmd(Command* cmd);
static void msg_cmd(Command* cmd);
static void joi_cmd(Command* cmd);
static void bye_cmd(Command* cmd);
static void fln_cmd(Command* cmd);
static void nln_cmd(Command* cmd);
static void iln_cmd(Command* cmd);
static void adl_cmd(Command* cmd);
static void chg_cmd(Command* cmd);
static void rea_cmd(Command* cmd);
static void prp_cmd(Command* cmd);
static void ubx_cmd(Command* cmd);
static void uux_cmd(Command* cmd);
static void nfy_cmd(Command* cmd);
static void put_cmd(Command* cmd);
static void sdg_cmd(Command* cmd);
static void lst_cmd(Command* cmd);

static void plain_msg(Command* cmd, const std::string& mime,
                      const std::string& body);
static void control_msg(Command* cmd, const std::string& mime,
                        const std::string& body);
static void clientcaps_msg(Command* cmd, const std::string& mime,
                           const std::string& body);
static void emoticon_msg(Command* cmd, const std::string& mime,
                         const std::string& body);
static void datacast_msg(Command* cmd, const std::string& mime,
                         const std::string& body);
static void invite_msg(Command* cmd, const std::string& mime,
                       const std::string& body);
static void handwritten_msg(Command* cmd, const std::string& mime,
                            const std::string& body);
static void typing_msg(Command* cmd, const std::string& mime,
                       const std::string& body);
static void nudge_msg(Command* cmd, const std::string& mime,
                      const std::string& body);
static void voiceclip_msg(Command* cmd, const std::string& mime,
                          const std::string& body);
static void wink_msg(Command* cmd, const std::string& mime,
                     const std::string& body);

}  // anonymous namespace

namespace {

static void parse_cmd(Command* cmd) {
  const cmd_map::const_iterator it = commands.find(cmd->args[0]);
  if (it != commands.end())
    (*commands[cmd->args[0]])(cmd);
}

static bool is_payload(const std::string& cmd,
                       const payload_set& payload_cmds) {
  return payload_cmds.count(cmd) > 0;
}

// Send a command
static void send_command(struct bufferevent* bufev, const std::string& cmd) {
  struct evbuffer* databuf = evbuffer_new();

  if (databuf == NULL)
    goto fail;

  evbuffer_add_printf(databuf, "%s\r\n", cmd.c_str());
  bufferevent_write_buffer(bufev, databuf);

fail:
  if (databuf)
    evbuffer_free(databuf);
}

// Send a payload command
static void send_message(struct bufferevent* bufev,
                         const std::vector<std::string>& args,
                         const std::string& payload) {
  struct evbuffer* databuf = evbuffer_new();

  if (databuf == NULL)
    goto fail;

  // Build the command
  for (unsigned int i = 0; i < args.size(); i++) {
    if (i == args.size() - 1) {
      evbuffer_add_printf(databuf, "%s\r\n", args[i].c_str());
    } else {
      evbuffer_add_printf(databuf, "%s ", args[i].c_str());
    }
  }

  if (payload.size() > 0)
    evbuffer_add(databuf, payload.c_str(), payload.size());

  bufferevent_write_buffer(bufev, databuf);

fail:
  if (databuf)
    evbuffer_free(databuf);
}

static bool check_login(const Command* cmd) {
  const Connection* conn = cmd->conn;
  const SessionPointer sess = conn->session;

  db.add_user(cmd->args[4]);

  bool denied = false;
  if (!db.check_version(sess->version)) {
    if (db.can_login(cmd->args[4]))
      db.set_login_time(cmd->args[4]);
    else
      denied = true;
  } else {
    DLOG(1, "Unsupported version '%u'", sess->version);
    denied = true;
  }

  if (denied) {
    // server is unavailable, in response to USR
    send_command(conn->client_bufev, "601 " + lexical_cast<std::string>(cmd->trid));
  }

  return denied;
}

static bool check_filter(const History* hist, bool encrypted) {
  if (db.buddy_is_blocked(hist->local_im(), hist->remote_im()))
    return true;
  if (acl_check_deny(hist->local_im(), hist->remote_im()))
    return true;

  int rule_type = 0;
  switch (hist->type()) {
    case History::TYPE_MSG:
      if (encrypted)
        rule_type = 13;
      else
        rule_type = 14;
      break;
    case History::TYPE_FILE:
      rule_type = 3;
      break;
    case History::TYPE_CAPS:
      rule_type = 4;
      break;
    case History::TYPE_WEBCAM:
      rule_type = 5;
      break;
    case History::TYPE_REMOTEDESKTOP:
      rule_type = 6;
      break;
    case History::TYPE_APPLICATION:
      rule_type = 7;
      break;
    case History::TYPE_EMOTICON:
      rule_type = 8;
      break;
    case History::TYPE_INK:
      rule_type = 9;
      break;
    case History::TYPE_NUDGE:
      rule_type = 10;
      break;
    case History::TYPE_WINK:
      rule_type = 11;
      break;
    case History::TYPE_VOICECLIP:
      rule_type = 12;
      break;
    case History::TYPE_GAMES:
      rule_type = 15;
      break;
    case History::TYPE_PHOTO:
      rule_type = 16;
      break;
    default:
      break;
  }

  if (rule_type != 0) {
    if (db.has_rule(hist->local_im(), rule_type)) {
      if (rule_type == 14)
        return word_filter_check(hist->data());
      return true;
    }
  }

  return false;
}

static void do_notifies(Command* cmd) {
  Connection* conn = cmd->conn;
  SessionPointer sess = conn->session;
  History* history = cmd->hist;

  DLOG(2, "%s: called", __func__);

  if (conn->type == Connection::SB) {
    if (history->type() == History::TYPE_MSG && !sess->warned) {
      if (db.has_rule(history->local_im(), 2))
        send_notify(cmd, db.get_setting("default_warning"));
      sess->warned = true;
    }
  } else {
    ChatMap::const_iterator it =
        sess->chat_sessions.find(history->remote_im());
    ChatSession* chat = it->second;

    if (history->type() == History::TYPE_MSG && !chat->warned()) {
      if (db.has_rule(history->local_im(), 2))
        send_notify(cmd, db.get_setting("default_warning"));
      chat->set_warned(true);
    }
  }

  if (history->is_filtered() &&
      history->type() != History::TYPE_TYPING &&
      history->type() != History::TYPE_CAPS)
    send_notify(cmd, db.get_setting("filtered_msg"));
}

static void send_notify(Command* cmd, const std::string& msg) {
  Connection* conn = cmd->conn;
  SessionPointer sess = conn->session;
  History* history = cmd->hist;

  std::string body;
  std::vector<std::string> args;
  if (sess->version < msn::MSNP20) {
    body = "MIME-Version: 1.0\r\n"
      "Content-Type: text/plain; charset=UTF-8\r\n"
      "X-MMS-IM-Format: FN=Segoe%20UI; EF=B; CO=808080; CS=0; PF=0\r\n\r\n"
      + msg;
    args.push_back("MSG");
    if (cmd->is_inbound()) {
      args.push_back("1");
      args.push_back("U");
    } else {
      args.push_back(history->remote_im());
      args.push_back(history->remote_im());
    }
  } else {
    body = "Routing: 1.0\r\n"
      "To: 1:" + history->from() + "\r\n"
      "From: 1:" + history->to();

    if (cmd->is_inbound())
      body.append(";epid=" + sess->epid);

    body.append("\r\n"
        "Service-Channel: IM/Online\r\n\r\n"
        "Reliability: 1.0\r\n\r\n"
        "Messaging: 2.0\r\n"
        "Message-Type: Text\r\n"
        "Content-Transfer-Encoding: 7bit\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Length: " + lexical_cast<std::string>(msg.length()) + "\r\n"
        "X-MMS-IM-Format: FN=Segoe%20UI; EF=B; CO=808080; CS=0; PF=0\r\n\r\n"
        + msg);
    args.push_back("SDG");
    args.push_back("0");
  }

  args.push_back(lexical_cast<std::string>(body.size()));
  log_info("notifying %s", cmd->is_inbound() ?
      history->remote_im().c_str() : history->local_im().c_str());
  send_message(cmd->is_inbound() ?
      conn->server_bufev : conn->client_bufev, args, body);
}

// Send a cancel message
static void send_cancel_message(Command* cmd) {
  Connection* conn = cmd->conn;
  SessionPointer sess = conn->session;
  History* history = cmd->hist;

  std::string body;
  std::vector<std::string> args;
  if (sess->version < msn::MSNP20) {
    body = "MIME-Version: 1.0\r\n"
      "Content-Type: text/x-msmsgsinvite; charset=UTF-8\r\n\r\n"
      "Invitation-Command: CANCEL\r\n"
      "Invitation-Cookie: " + cmd->cookie + "\r\n"
      "Cancel-Code: REJECT_NOT_INSTALLED\r\n";
    args.push_back("MSG");
    if (cmd->is_inbound()) {
      args.push_back("1");
      args.push_back("U");
    } else {
      args.push_back(history->remote_im());
      args.push_back(history->remote_im());
    }
  } else {
    std::string content("Invitation-Command: CANCEL\r\n"
        "Invitation-Cookie: " + cmd->cookie + "\r\n"
        "Cancel-Code: REJECT_NOT_INSTALLED\r\n");

    body = "Routing: 1.0\r\n"
      "To: 1:" + history->from() + "\r\n"
      "From: 1:" + history->to();

    if (cmd->is_inbound())
      body.append(";epid=" + sess->epid);

    body.append("\r\n"
        "Service-Channel: IM/Online\r\n\r\n"
        "Reliability: 1.0\r\n\r\n"
        "Messaging: 2.0\r\n"
        "Message-Type: Invite\r\n"
        "Content-Transfer-Encoding: 7bit\r\n"
        "Content-Type: text/x-msmsgsinvite; charset=UTF-8\r\n"
        "Content-Length: " + lexical_cast<std::string>(content.length()) +
        "\r\n\r\n" + content);
    args.push_back("SDG");
    args.push_back("0");
  }

  args.push_back(lexical_cast<std::string>(body.size()));
  send_message(cmd->is_inbound() ?
      conn->server_bufev : conn->client_bufev, args, body);
}

// TODO: get rid of get_account and add_user
static std::string get_account(const std::string& user) {
  size_t idx = user.find_last_of(";");
  if (idx == std::string::npos)
    return user;
  return user.substr(0, idx);
}

static void add_user(const std::string& user,
                     std::vector<std::string>& members) {
  std::vector<std::string>::const_iterator it = std::find(members.begin(),
                                                          members.end(),
                                                          user);

  if (it == members.end()) {
    members.push_back(user);
    DLOG(1, "user=[%s], total=%zu", user.c_str(), members.size());
  }
}

uint32_t get_or_up(Connection* conn, const std::string& buddy) {
  SessionPointer sess = conn->session;

  uint32_t ret = 0;

  ChatMap::const_iterator it = sess->chat_sessions.find(buddy);
  if (it == sess->chat_sessions.end()) {
    ret = db.get_chat_id(sess->user);
    ChatSession* chat = new ChatSession(conn, buddy, ret);
    sess->chat_sessions[buddy] = chat;
  } else {
    ChatSession* chat = it->second;
    chat->set_renew(true);
    ret = chat->id();
  }

  return ret;
}

// TODO: Remove this hack.
void hack(Command* cmd, const std::string& mime) {
  Connection* conn = cmd->conn;

  if (conn->type == Connection::NS) {
    string_map header_map = parse_headers(mime);

    const std::string& to = header_map["To"];
    // Check for multiparty chat.
    if (to[0] == '9' ||
        to.compare(0, 2, "10") == 0) {
      delete cmd->hist;
      cmd->hist = NULL;

      return;
    }
    const std::string& from = header_map["From"];

    std::string buddy = cmd->is_inbound() ? from : to;
    size_t pos = buddy.find_first_of(':');
    if (pos != std::string::npos)
      buddy.erase(0, pos + 1);
    pos = buddy.find_last_of(';');
    if (pos != std::string::npos)
      buddy.resize(pos);

    cmd->hist->set_remote_im(buddy);
    cmd->hist->set_conversation_id(get_or_up(conn, buddy));
  }
}

}  // anonymous namespace

namespace {

static void ans_cmd(Command* cmd) {
  Connection* conn = cmd->conn;
  SessionPointer sess = conn->session;

  if (!cmd->is_inbound()) {
    if (cmd->args.size() >= 3) {
      sess->user = get_account(cmd->args[2]);
      if (sess->chat_id == 0)
        sess->chat_id = db.get_chat_id(sess->user);
      conn->type = Connection::SB;
    }
  }
}

static void iro_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->is_inbound()) {
    std::string buddy = cmd->args[4];
    size_t pos = buddy.find_last_of(";");
    if (pos != std::string::npos)
      buddy.resize(pos);

    if (buddy == sess->user)
      return;
    add_user(buddy, sess->members);
  }
}

static void usr_cmd(Command* cmd) {
  Connection* conn = cmd->conn;
  SessionPointer sess = conn->session;

  const bool inbound = cmd->is_inbound();
  if (!inbound) {
    if ((cmd->args[2] == "TWN" || cmd->args[2] == "SSO") &&
        cmd->args[3] == "I") {
      sess->connecting = true;
    } else if (cmd->args[2] == "SSO" && cmd->args[3] == "S" &&
               sess->version >= msn::MSNP20) {
      sess->epid = cmd->args[6];
    }
  } else {
    if (cmd->args[2] == "OK") {
      // authenticate OK
      sess->user = get_account(cmd->args[3]);
      if (cmd->args.size() >= 6) {
        if (sess->version <= msn::MSNP9)
          db.set_friendly_name(sess->user, utils::decode_url(cmd->args[4]));
        conn->type = Connection::NS;
      } else if (cmd->args.size() == 5) {
        if (sess->chat_id == 0)
          sess->chat_id = db.get_chat_id(sess->user);
        conn->type = Connection::SB;
      }
    }
  }
}

static void ver_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (!cmd->is_inbound()) {
    if (cmd->args.size() >= 3)
      sess->version = strtol(cmd->args[2].substr(4, 2).c_str(), NULL, 10);
  }
}

static void msg_cmd(Command* cmd) {
  const std::string& msg = cmd->payload;

  size_t index = msg.find("\r\n\r\n");
  std::string mime = msg.substr(0, index);

  string_map header_map = parse_headers(mime);
  std::string content_type = header_map["Content-Type"];

  size_t pos = content_type.find("; charset", 0);
  if (pos != std::string::npos)
    content_type.erase(pos);

  std::string body;
  const int n = msg.size() - mime.size() - 4;
  if (n > 0)
    body = msg.substr(index + 4);

  const msg_map::const_iterator it = messages.find(content_type);
  if (it != messages.end()) {
    if (!cmd->is_inbound() && (cmd->args[2] == "A" || cmd->args[2] == "D"))
      cmd->set_flags(Command::WAITING_FOR_ACK);

    (*messages[content_type])(cmd, mime, body);
  }
}

static void joi_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->is_inbound()) {
    std::string buddy = cmd->args[1];
    size_t pos = buddy.find_last_of(";");
    if (pos != std::string::npos)
      buddy.resize(pos);

    if (buddy == sess->user)
      return;
    add_user(buddy, sess->members);
  }
}

static void bye_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->is_inbound()) {
    std::string buddy = cmd->args[1];
    size_t pos = buddy.find_last_of(";");
    if (pos != std::string::npos)
      buddy.resize(pos);

    std::vector<std::string>& members = sess->members;

    for (std::vector<std::string>::iterator it = members.begin();
        it != members.end(); ++it) {
      if (*it == buddy) {
        members.erase(it);
        break;
      }
    }
  }
}

static void fln_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->is_inbound() &&
      cmd->args[1].find(circle) == std::string::npos) {
    std::string buddy = cmd->args[1];
    size_t pos = buddy.find_first_of(':');
    if (pos != std::string::npos)
      buddy.erase(0, pos + 1);

    db.buddy_logoff(sess->user, buddy);
  }
}

static void nln_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->is_inbound() &&
      cmd->args[2].find(circle) == std::string::npos) {
    const std::string& status = cmd->args[1];
    const std::string& friendly = cmd->args.size() > 6 ?
        cmd->args[4] : cmd->args[3];

    std::string buddy = cmd->args[2];
    size_t pos = buddy.find_first_of(':');
    if (pos != std::string::npos)
      buddy.erase(0, pos + 1);

    if (buddy == sess->user)
      return;
    db.update_buddy(sess->user, buddy, status, utils::decode_url(friendly));
  }
}

static void iln_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->is_inbound()) {
    const std::string& status = cmd->args[2];
    const std::string& buddy = cmd->args[3];
    const std::string& friendly = cmd->args.size() > 7 ?
        cmd->args[5] : cmd->args[4];

    db.update_buddy(sess->user, buddy, status, utils::decode_url(friendly));
  }
}

static void adl_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->payload.size() > 0) {
    boost::scoped_array<char> buffer(new char[cmd->payload.size() + 1]);
    memcpy(buffer.get(), cmd->payload.c_str(), cmd->payload.size() + 1);

    rapidxml::xml_document<char> doc;
    doc.parse<0>(buffer.get());

    rapidxml::xml_node<char>* adl = doc.first_node();
    std::string email;

    for (rapidxml::xml_node<char>* domain = adl->first_node("d");
         domain != NULL;
         domain = domain->next_sibling("d")) {
      const char* domain_name = domain->first_attribute("n")->value();
      for (rapidxml::xml_node<char>* contact = domain->first_node("c");
           contact != NULL;
           contact = contact->next_sibling("c")) {
        const char* contact_name = contact->first_attribute("n")->value();
        int type = strtol(contact->first_attribute("t")->value(), NULL, 10);
        if (type == 9 || type == 32)
          continue;

        // TODO: do something better
        email = contact_name;
        email += "@";
        email += domain_name;

        // NOTE: Wave 4 keeps the user as a contact.
        if (email != sess->user)
          db.add_buddy(sess->user, email);
      }
    }
  }
}

static void chg_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (!cmd->is_inbound())
    db.set_status(sess->user, cmd->args[2]);
}

static void rea_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->is_inbound()) {
    if (cmd->args[3] == sess->user)
      db.set_friendly_name(sess->user, utils::decode_url(cmd->args[4]));
  }
}

static void prp_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->is_inbound()) {
    if (cmd->args.size() == 4) {
      if (cmd->args[2] == "MFN")
        db.set_friendly_name(sess->user, utils::decode_url(cmd->args[3]));
    } else {
      if (cmd->args[1] == "MFN")
        db.set_friendly_name(sess->user, utils::decode_url(cmd->args[2]));
    }
  }
}

static void ubx_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (!cmd->payload.empty() &&
      cmd->args[1].find(circle) == std::string::npos) {
    std::string buddy = cmd->args[1];
    size_t pos = buddy.find_first_of(':');
    if (pos != std::string::npos)
      buddy.erase(0, pos + 1);

    if (buddy == sess->user)
      return;

    boost::scoped_array<char> buffer(new char[cmd->payload.size() + 1]);
    memcpy(buffer.get(), cmd->payload.c_str(), cmd->payload.size() + 1);

    rapidxml::xml_document<char> doc;
    doc.parse<0>(buffer.get());

    rapidxml::xml_node<char>* payload_node = doc.first_node();

    rapidxml::xml_node<char>* psm_node = payload_node->first_node("PSM");
    if (psm_node) {
      const char* psm = psm_node->value();
      db.set_buddy_status_message(sess->user, buddy, psm);
    }
  }
}

static void uux_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (!cmd->is_inbound()) {
    boost::scoped_array<char> buffer(new char[cmd->payload.size() + 1]);
    memcpy(buffer.get(), cmd->payload.c_str(), cmd->payload.size() + 1);

    rapidxml::xml_document<char> doc;
    doc.parse<0>(buffer.get());

    rapidxml::xml_node<char>* payload_node = doc.first_node();

    rapidxml::xml_node<char>* psm_node = payload_node->first_node("PSM");
    if (psm_node) {
      const char* psm = psm_node->value();
      db.set_status_message(sess->user, psm);
    }
  }
}

static void nfy_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->payload.empty())
    return;

  size_t routing_end_pos = cmd->payload.find("\r\n\r\n");
  std::string mime = cmd->payload.substr(0, routing_end_pos);
  string_map routing_headers = parse_headers(mime);

  std::string buddy = routing_headers["From"];
  if (buddy.empty())
    return;

  size_t pos = buddy.find_first_of(':');  // strip off the ':'
  // TODO: verify this, and/or implement correct parsing to handle
  // contact types.
  if (pos != std::string::npos)
    buddy.erase(0, pos + 1);

  if (buddy == sess->user)
    return;

  size_t reliability_end_pos = cmd->payload.find("\r\n\r\n",
                                                 routing_end_pos + 4);

  size_t content_end_pos = cmd->payload.find("\r\n\r\n",
                                             reliability_end_pos + 4);
  mime = cmd->payload.substr(
      reliability_end_pos + 4, content_end_pos - (reliability_end_pos + 4));
  string_map content_headers = parse_headers(mime);

  if (content_headers["Content-Type"] != "application/user+xml")
    return;

  if (cmd->args[1] == "PUT") {
    size_t len = cmd->payload.length() - content_end_pos - 4;
    boost::scoped_array<char> buffer(new char[len + 1]);
    buffer[len] = '\0';
    memcpy(buffer.get(), cmd->payload.data() + content_end_pos + 4, len);

    rapidxml::xml_document<char> doc;
    doc.parse<0>(buffer.get());

    rapidxml::xml_node<char>* user_node = doc.first_node();

    rapidxml::xml_node<char>* s_node = user_node->first_node("s");
    while (s_node) {
      const char* attr = s_node->first_attribute("n")->value();
      if (strcmp(attr, "IM") == 0) {
        rapidxml::xml_node<char>* status_node = s_node->first_node("Status");
        if (status_node) {
          // TODO: avoid copy.
          const std::string status = status_node->value();
          db.update_buddy_status(sess->user, buddy, status);
        }
      } else if (strcmp(attr, "PE") == 0) {
        rapidxml::xml_node<char>* display_name_node = s_node->first_node("FriendlyName");
        if (display_name_node) {
          // TODO: avoid copy.
          const std::string display_name = display_name_node->value();
          db.set_buddy_friendly_name(sess->user, buddy, utils::decode_url(display_name));
        }

        rapidxml::xml_node<char>* psm_node = s_node->first_node("PSM");
        if (psm_node) {
          const char* psm = psm_node->value();
          db.set_buddy_status_message(sess->user, buddy, psm);
        }
      }
      s_node = s_node->next_sibling("s");
    }
  } else if (cmd->args[1] == "DEL") {
    db.buddy_logoff(sess->user, buddy);
  }
}

static void put_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (!cmd->is_inbound()) {
    size_t routing_end_pos = cmd->payload.find("\r\n\r\n");

    size_t reliability_end_pos = cmd->payload.find("\r\n\r\n",
                                                   routing_end_pos + 4);

    size_t content_end_pos = cmd->payload.find("\r\n\r\n",
                                               reliability_end_pos + 4);
    std::string mime = cmd->payload.substr(
        reliability_end_pos + 4, content_end_pos - (reliability_end_pos + 4));
    string_map content_headers = parse_headers(mime);

    if (content_headers["Content-Type"] != "application/user+xml")
      return;

    size_t len = cmd->payload.length() - content_end_pos - 4;
    boost::scoped_array<char> buffer(new char[len + 1]);
    buffer[len] = '\0';
    memcpy(buffer.get(), cmd->payload.data() + content_end_pos + 4, len);

    rapidxml::xml_document<char> doc;
    doc.parse<0>(buffer.get());

    rapidxml::xml_node<char>* user_node = doc.first_node();

    rapidxml::xml_node<char>* s_node = user_node->first_node("s");
    while (s_node) {
      const char* attr = s_node->first_attribute("n")->value();
      if (strcmp(attr, "IM") == 0) {
        rapidxml::xml_node<char>* status_node = s_node->first_node("Status");
        if (status_node) {
          // TODO: avoid copy.
          const std::string status = status_node->value();
          db.set_status(sess->user, status);
        }
      } else if (strcmp(attr, "PE") == 0) {
        rapidxml::xml_node<char>* display_name_node = s_node->first_node("FriendlyName");
        if (display_name_node) {
          // TODO: avoid copy.
          const std::string display_name = display_name_node->value();
          db.set_friendly_name(sess->user, utils::decode_url(display_name));
        }

        rapidxml::xml_node<char>* psm_node = s_node->first_node("PSM");
        if (psm_node) {
          const char* psm = psm_node->value();
          db.set_status_message(sess->user, psm);
        }
      }
      s_node = s_node->next_sibling("s");
    }
  }
}

static void sdg_cmd(Command* cmd) {
  const std::string& msg = cmd->payload;

  size_t routing_end_pos = msg.find("\r\n\r\n");

  size_t reliability_end_pos = msg.find("\r\n\r\n", routing_end_pos + 4);

  size_t content_end_pos = msg.find("\r\n\r\n", reliability_end_pos + 4);

  std::string mime = msg.substr(
      reliability_end_pos + 4, content_end_pos - (reliability_end_pos + 4));
  string_map content_headers = parse_headers(mime);

  mime = msg.substr(0, routing_end_pos);

  std::string message_type = content_headers["Message-Type"];

  std::string body;
  const int n = msg.size() - content_end_pos - 4;
  if (n > 0)
    body = msg.substr(content_end_pos + 4);

  const msg_map::const_iterator it = messages.find(message_type);
  if (it != messages.end())
    (*messages[message_type])(cmd, mime, body);
}

static void lst_cmd(Command* cmd) {
  SessionPointer sess = cmd->conn->session;

  if (cmd->is_inbound()) {
    std::string buddy = cmd->args[1];
    if (buddy.compare(0, 2, "N=") == 0)
      buddy.erase(0, 2);

    //std::string friendly = cmd->args[2];
    //if (friendly.compare(0, 2, "F=") == 0)
    //  friendly.erase(0, 2);

    db.add_buddy(sess->user, buddy);
  }
}

}  // anonymous namespace

namespace {

static void plain_msg(Command* cmd, const std::string& mime,
                      const std::string& body) {
  static const std::string crypt_header[] = {
      "*** Encrypted :",  // Gaim-Encryption
      "?OTR"
  };

  bool is_encrypted = false;
  for (size_t i = 0; i < arraysize(crypt_header); ++i) {
    if (body.size() > crypt_header[i].size() &&
        !body.compare(0, crypt_header[i].size(), crypt_header[i])) {
      is_encrypted = true;
      break;
    }
  }

  cmd->hist = new History(cmd, History::TYPE_MSG);
  if (is_encrypted) {
    cmd->set_flags(Command::ENCRYPTED);
  } else {
    cmd->hist->set_data(body);
  }

  hack(cmd, mime);
}

static void control_msg(Command* cmd, const std::string& mime,
                        const std::string& body) {
  string_map header_map = parse_headers(mime);

  if (!header_map["TypingUser"].empty() ||
      !header_map["RecordingUser"].empty()) {
    typing_msg(cmd, mime, body);
  }
}

static void clientcaps_msg(Command* cmd, const std::string& mime,
                           const std::string& body) {
  cmd->hist = new History(cmd, History::TYPE_CAPS);
  cmd->hist->set_dont_log();
}

static void emoticon_msg(Command* cmd, const std::string& mime,
                         const std::string& body) {
  cmd->hist = new History(cmd, History::TYPE_EMOTICON);

  hack(cmd, mime);
}

static void datacast_msg(Command* cmd, const std::string& mime,
                         const std::string& body) {
  string_map header_map = parse_headers(body);

  const int id = strtol(header_map["ID"].c_str(), NULL, 10);
  switch (id) {
    case 1:
      nudge_msg(cmd, mime, body);
      break;
    case 2:
      wink_msg(cmd, mime, body);
      break;
    case 3:
      voiceclip_msg(cmd, mime, body);
      break;
    case 4:
      // TODO: what to do ?!?!
    default:
      break;
  }
}

static void invite_msg(Command* cmd, const std::string& mime,
                       const std::string& body) {
  // TODO implement this properly.
  string_map header_map;
  if (body.length())
    header_map = parse_headers(body);
  else
    header_map = parse_headers(mime);

  const std::string& command = header_map["Invitation-Command"];
  if (command == "INVITE") {
    const std::string& guid = header_map["Application-GUID"];
    if (guid.empty())
      return;

    if (guid == "{56b994a7-380f-410b-9985-c809d78c1bdc}") {
      cmd->hist = new History(cmd, History::TYPE_REMOTEDESKTOP);
    } else if (guid == "{1DF57D09-637A-4ca5-91B9-2C3EDAAF62FE}") {
      cmd->hist = new History(cmd, History::TYPE_INK);
    } else if (guid == "{02D3C01F-BF30-4825-A83A-DE7AF41648AA}") {
      cmd->hist = new History(cmd, History::TYPE_WEBCAM);
    } else if (guid == "{5D3E02AB-6190-11d3-BBBB-00C04F795683}") {
      cmd->hist = new History(cmd, History::TYPE_FILE);
      cmd->hist->set_data(header_map["Application-FileSize"] + " " +
                          header_map["Application-File"]);
    } else {
      cmd->hist = new History(cmd, History::TYPE_APPLICATION);
    }

    cmd->cookie = header_map["Invitation-Cookie"];

    hack(cmd, mime);
  }
}

static void handwritten_msg(Command* cmd, const std::string& mime,
                            const std::string& body) {
  cmd->hist = new History(cmd, History::TYPE_INK);
}

static void typing_msg(Command* cmd, const std::string& mime,
                       const std::string& body) {
  cmd->hist = new History(cmd, History::TYPE_TYPING);
  cmd->hist->set_dont_log();

  hack(cmd, mime);
}

static void nudge_msg(Command* cmd, const std::string& mime,
                      const std::string& body) {
  cmd->hist = new History(cmd, History::TYPE_NUDGE);

  hack(cmd, mime);
}

static void voiceclip_msg(Command* cmd, const std::string& mime,
                          const std::string& body) {
  cmd->hist = new History(cmd, History::TYPE_VOICECLIP);

  hack(cmd, mime);
}

static void wink_msg(Command* cmd, const std::string& mime,
                     const std::string& body) {
  cmd->hist = new History(cmd, History::TYPE_WINK);

  hack(cmd, mime);
}

}  // anonymous namespace

namespace msn {

void msn_init(void) {
  if (commands.size() == 0) {
    commands["CHG"] = &chg_cmd;
    commands["ADL"] = &adl_cmd;
    commands["ANS"] = &ans_cmd;
    commands["IRO"] = &iro_cmd;
    commands["USR"] = &usr_cmd;
    commands["VER"] = &ver_cmd;
    commands["REA"] = &rea_cmd;
    commands["PRP"] = &prp_cmd;
    commands["LST"] = &lst_cmd;
    commands["MSG"] = &msg_cmd;
    commands["JOI"] = &joi_cmd;
    commands["BYE"] = &bye_cmd;
    commands["FLN"] = &fln_cmd;
    commands["NLN"] = &nln_cmd;
    commands["ILN"] = &iln_cmd;
    commands["UBX"] = &ubx_cmd;
    commands["UUX"] = &uux_cmd;
    commands["NFY"] = &nfy_cmd;
    commands["PUT"] = &put_cmd;
    commands["SDG"] = &sdg_cmd;
  }

  if (messages.size() == 0) {
    messages["text/plain"] = &plain_msg;
    messages["text/x-msmsgscontrol"] = &control_msg;
    messages["text/x-clientcaps"] = &clientcaps_msg;
    messages["text/x-clientinfo"] = &clientcaps_msg;
    messages["text/x-mms-emoticon"] = &emoticon_msg;
    messages["text/x-mms-animemoticon"] = &emoticon_msg;
    messages["text/x-msnmsgr-datacast"] = &datacast_msg;
    messages["text/x-msmsgsinvite"] = &invite_msg;
    messages["image/gif"] = &handwritten_msg;
    messages["application/x-ms-ink"] = &handwritten_msg;
    messages["Text"] = &plain_msg;
    messages["Control/Typing"] = &typing_msg;
    messages["Control/Recording"] = &typing_msg;
    messages["Nudge"] = &nudge_msg;
    messages["CustomEmoticon"] = &emoticon_msg;
    messages["Voice"] = &voiceclip_msg;
    messages["Wink"] = &wink_msg;
    messages["Invite"] = &invite_msg;
  }

  if (payload_commands_from_client.size() == 0) {
    payload_commands_from_client.insert("MSG");
    payload_commands_from_client.insert("UBX");
    payload_commands_from_client.insert("UUX");
    payload_commands_from_client.insert("ADL");
    payload_commands_from_client.insert("RML");
    payload_commands_from_client.insert("FQY");
    payload_commands_from_client.insert("DEL");
    payload_commands_from_client.insert("PUT");
    payload_commands_from_client.insert("SDG");
    payload_commands_from_client.insert("QRY");
    payload_commands_from_client.insert("GCF");
    payload_commands_from_client.insert("NOT");
    payload_commands_from_client.insert("SDC");
    payload_commands_from_client.insert("UUN");
    payload_commands_from_client.insert("UBN");
    payload_commands_from_client.insert("UUM");
  }

  if (payload_commands_from_server.size() == 0) {
    payload_commands_from_server.insert("MSG");
    payload_commands_from_server.insert("UBX");
    payload_commands_from_server.insert("UUX");
    payload_commands_from_server.insert("FQY");
    payload_commands_from_server.insert("PUT");
    payload_commands_from_server.insert("NFY");
    payload_commands_from_server.insert("SDG");
    payload_commands_from_server.insert("GCF");
    payload_commands_from_server.insert("NOT");
    payload_commands_from_server.insert("UUN");
    payload_commands_from_server.insert("UBN");
    payload_commands_from_server.insert("IPG");
    payload_commands_from_server.insert("UBM");
    payload_commands_from_server.insert("801");
  }

  if (db.init())
    db.cleanup();
}

void destroy_cb(Connection* conn) {
  const SessionPointer sess = conn->session;

  if (conn->type == Connection::NS) {
    db.user_logoff(sess->user);

    if (sess->chat_sessions.size() > 0) {
      for (ChatMap::iterator it = sess->chat_sessions.begin();
           it != sess->chat_sessions.end(); ++it) {
        ChatSession* chat = it->second;
        db.delete_chat(chat->id());
        delete chat;
      }
      sess->chat_sessions.clear();
    }
  } else if (conn->type == Connection::SB) {
    if (sess->chat_id != 0)
      db.delete_chat(sess->chat_id);
  }
}

void drop_chat(Connection* conn, const std::string& buddy) {
  SessionPointer sess = conn->session;

  ChatMap::iterator it = sess->chat_sessions.find(buddy);
  ChatSession* chat = it->second;
  db.delete_chat(chat->id());
  sess->chat_sessions.erase(it);
}

int parse_packet(bool inbound, struct evbuffer* input, Connection* conn) {
  Command* cmd = conn->cmd[inbound]; // 0 for client to server

  const char* buf = reinterpret_cast<const char*>(EVBUFFER_DATA(input));
  size_t buf_len = EVBUFFER_LENGTH(input);

  bool done = false;
  if (!cmd->is_chunked()) {
    char* linebreak = strstr(const_cast<char*>(buf), "\r\n");

    if (linebreak == NULL) {  // no CRLF found
      DLOG(1, " -- ignoring line without crlf");
      return -1;
    }

    const std::string line(buf, linebreak - buf);

    DLOG(1, "%c: %u: %s", inbound ? 'S' : 'C', conn->id, line.c_str());

    const size_t line_len = linebreak - buf + 2;

    tokenizer<> t(line, " ");
    while (t.has_next())
      cmd->args.push_back(t.token());

    if (cmd->args.size() > 1) {
      cmd->trid = isdigit(cmd->args[1][0]) ? atoi(cmd->args[1].c_str()) : 0;
    } else {
      cmd->trid = 0;
    }

    bool has_payload = is_payload(cmd->args[0], inbound
                                  ? payload_commands_from_server
                                  : payload_commands_from_client);

    if (has_payload) {
      if (isdigit(cmd->args[ (cmd->args.size() - 1) ][0]))
        cmd->payload_len = atol(cmd->args[ (cmd->args.size() - 1) ].c_str());
      if (cmd->payload_len > 0) {
        linebreak += 2;
        if (buf_len - line_len >= cmd->payload_len) {
          // Completed chunk
          cmd->payload.assign(linebreak, cmd->payload_len);
          evbuffer_drain(input, cmd->payload_len + line_len);
          cmd->payload_len = 0;
          done = true;
        } else {
          // Read more!
          cmd->payload.assign(linebreak, buf_len - line_len);
          cmd->payload_len -= buf_len - line_len;
          evbuffer_drain(input, buf_len);
          cmd->set_flags(Command::CHUNKED);
        }
      } else {
        evbuffer_drain(input, line_len);
        done = true;
      }
    } else {
      evbuffer_drain(input, line_len);
      done = true;
    }

  } else {
    if (buf_len >= cmd->payload_len) {
      // Last chunk
      cmd->payload.append(buf, cmd->payload_len);
      evbuffer_drain(input, cmd->payload_len);
      cmd->payload_len = 0;
      cmd->clear_flags(Command::CHUNKED);
      done = true;
    } else {
      // Read more!
      cmd->payload.append(buf, buf_len);
      cmd->payload_len -= buf_len;
      evbuffer_drain(input, buf_len);
    }
  }

  if (done) {

    if (show_payload && cmd->payload.size() > 0) {
      const std::string escaped(
          utils::escape_payload(cmd->payload, cmd->payload.size()));
      fprintf(stdout, "\n======\n%s\n======\n", escaped.c_str());
    }

    parse_cmd(cmd);

    bool filtered = false;

    if (conn->session->connecting) {
      conn->session->connecting = false;
      filtered = check_login(cmd);
    } else if (cmd->should_ignore()) {
      cmd->clear_flags(Command::IGNORE);
      filtered = true;
    }

    // TODO: this isn't right.
    if (cmd->hist != NULL) {
      filtered = check_filter(cmd->hist, cmd->is_encrypted());

      if (cmd->is_encrypted()) {
        cmd->clear_flags(Command::ENCRYPTED);
        // Do nothing if the message is encrypted.
        if (!filtered)
          cmd->hist->set_dont_log();
      }

      if (!cmd->cookie.empty()) {
        if (filtered)
          send_cancel_message(cmd);
        cmd->cookie.clear();
      }

      cmd->hist->set_filtered(filtered);

      do_notifies(cmd);

      if (!db.has_rule(cmd->hist->local_im(), 1) ||
          cmd->hist->dont_log()) {
        delete cmd->hist;
      } else {
        HistoryLogger::instance()->log(cmd->hist);
      }

      cmd->hist = NULL;
    }

    // Send back "ACK" message to the client if needed.
    if (cmd->should_send_ack()) {
      cmd->clear_flags(Command::WAITING_FOR_ACK);
      if (filtered)
        send_command(conn->client_bufev, "ACK " + lexical_cast<std::string>(cmd->trid));
    }

    if (!filtered)
      send_message(inbound ? conn->client_bufev : conn->server_bufev, cmd->args, cmd->payload);

    cmd->payload.clear();
  }

  if (cmd->payload.empty())
    cmd->args.clear();

  return 0;
}

}  // namespace msn
