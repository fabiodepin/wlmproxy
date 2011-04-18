/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include "msn/msn_database.h"

#include <boost/scoped_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <dolphinconn/resultset.h>

#include "config.h"
#include "log.h"

using std::string;
using boost::lexical_cast;

namespace msn {

bool MsnDatabase::init() {
  Config& config = Config::instance();
  bool ret = db_.open(
      config["db_name"], config["db_user"], config["db_password"],
      config["db_host"], config.getint("db_port"), config["db_socket"]);

  if (!ret) {
    log_warn("MySQL error %d, SQLState %s: %s", db_.get_last_errno(),
             db_.get_sqlstate(), db_.get_error_msg());
  }
  return ret;
}

// TODO: This method should ONLY be called after a crash.
bool MsnDatabase::cleanup() {
  string sql("UPDATE conversations SET status=0 WHERE status=1");
  return db_.execute(sql);
}

uint64_t MsnDatabase::get_chat_id(const string& user) {
  MutexLocker lk(mutex_);

  string sql("INSERT INTO conversations(user_id, timestamp) "
             "SELECT users.id, NOW() FROM users "
             "WHERE users.username = '");
  sql.append(user);
  sql.append("'");

  if (!db_.execute(sql))
    return 0;
  return db_.get_last_insert_id();
}

bool MsnDatabase::delete_chat(uint64_t chat_id) {
  string sql("UPDATE conversations SET status=0 WHERE id = ");
  sql.append(lexical_cast<string>(chat_id));
  return db_.execute(sql);
}

bool MsnDatabase::add_user(const string& user) {
  string sql("CALL sp_add_user('" + user + "')");
  return db_.execute(sql);
}

bool MsnDatabase::can_login(const string& user) {
  string sql("SELECT COUNT(*) FROM users u JOIN usergroups g ON u.group_id = g.id "
             "WHERE u.username = '");
  sql.append(user);
  sql.append("' AND u.isenabled = 1 AND g.isactive = 1");

  boost::scoped_ptr<dolphinconn::ResultSet> sp(db_.execute_query(sql));
  if (sp.get() && sp->step())
    return sp->column_bool(0);
  return false;
}

bool MsnDatabase::set_login_time(const string& user) {
  string sql("UPDATE users SET lastlogin=NOW() WHERE username = '");
  sql.append(user);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::set_status(const string& user, const string& status) {
  string sql("UPDATE users SET status = '" + status + "'");
  sql.append(" WHERE username = '");
  sql.append(user);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::set_friendly_name(const string& user, const string& name) {
  string sql("UPDATE users SET displayname = '" + db_.escape(name) + "'");
  sql.append(" WHERE username = '");
  sql.append(user);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::set_status_message(const string& user, const char* msg) {
  string sql("UPDATE users SET psm = '");
  if (msg != NULL)
    sql.append(db_.escape(msg));
  sql.append("'");
  sql.append(" WHERE username = '");
  sql.append(user);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::user_logoff(const string& user) {
  string sql("UPDATE users SET status = 'FLN' WHERE username = '");
  sql.append(user);
  sql.append("'");
  if (!db_.execute(sql))
    return false;

  sql = "UPDATE buddies ";
  sql.append("JOIN users ON users.username = '");
  sql.append(user);
  sql.append("'");
  sql += " SET buddies.status = 'FLN' WHERE user_id = users.id";
  if (!db_.execute(sql))
    return false;

  return true;
}

bool MsnDatabase::add_buddy(const string& user, const string& who) {
  string sql("INSERT IGNORE INTO buddies(user_id, username) "
             "SELECT users.id, '" + who + "'");
  sql.append(" FROM users WHERE users.username = '");
  sql.append(user);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::buddy_logoff(const string& user, const string& who) {
  string sql("UPDATE buddies "
             "JOIN users ON users.username = '" + user + "'");
  sql.append(" SET buddies.status = 'FLN' "
             "WHERE user_id = users.id AND buddies.username = '");
  sql.append(who);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::update_buddy(const string& user, const string& who,
                               const string& status, const string& name) {
  string sql("UPDATE buddies "
             "JOIN users ON users.username = '" + user + "'");
  sql.append(" SET buddies.status = '" + status + "', ");
  sql.append("buddies.displayname = '" + db_.escape(name) + "' "
             "WHERE user_id = users.id AND buddies.username = '");
  sql.append(who);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::update_buddy_status(const string& user,
                                      const string& who,
                                      const string& status) {
  string sql("UPDATE buddies "
             "JOIN users ON users.username = '" + user + "'");
  sql.append(" SET buddies.status = '" + status + "' "
             "WHERE user_id = users.id AND buddies.username = '");
  sql.append(who);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::set_buddy_friendly_name(const string& user,
                                          const string& who,
                                          const string& name) {
  string sql("UPDATE buddies "
             "JOIN users ON users.username = '" + user + "'");
  sql.append(" SET buddies.displayname = '" + db_.escape(name) + "' "
             "WHERE user_id = users.id AND buddies.username = '");
  sql.append(who);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::set_buddy_status_message(const string& user,
                                           const string& who,
                                           const char* msg) {
  string sql("UPDATE buddies "
             "JOIN users ON users.username = '" + user + "'");
  sql.append(" SET buddies.psm = '");
  if (msg != NULL)
    sql.append(db_.escape(msg));
  sql.append("'");
  sql.append(" WHERE user_id = users.id AND buddies.username = '");
  sql.append(who);
  sql.append("'");
  return db_.execute(sql);
}

bool MsnDatabase::buddy_is_blocked(const string& user, const string& who) {
  string sql("SELECT COUNT(*) FROM buddies b JOIN users u ON u.username = '");
  sql.append(user);
  sql.append("' WHERE user_id = u.id AND b.username = '");
  sql.append(who);
  sql.append("' AND b.isblocked = 1");

  boost::scoped_ptr<dolphinconn::ResultSet> sp(db_.execute_query(sql));
  if (sp.get() && sp->step())
    return sp->column_bool(0);
  return false;
}

bool MsnDatabase::check_version(int version) {
  string sql("SELECT fn_check_version(");
  sql.append(lexical_cast<string>(version));
  sql.append(")");

  boost::scoped_ptr<dolphinconn::ResultSet> sp(db_.execute_query(sql));
  if (sp.get() && sp->step())
    return sp->column_bool(0);
  return false;
}

bool MsnDatabase::has_rule(const string& user, int type) {
  string sql("SELECT COUNT(*) FROM grouprules r JOIN users u ON u.username = '");
  sql.append(user);
  sql.append("' WHERE rule_id = ");
  sql.append(lexical_cast<string>(type));
  sql.append(" AND r.group_id = u.group_id");

  boost::scoped_ptr<dolphinconn::ResultSet> sp(db_.execute_query(sql));
  if (sp.get() && sp->step())
    return sp->column_bool(0);
  return false;
}

string MsnDatabase::get_rule_value(int type) {
  string sql("SELECT rulevalue FROM rules WHERE id = ");
  sql.append(lexical_cast<string>(type));

  boost::scoped_ptr<dolphinconn::ResultSet> sp(db_.execute_query(sql));
  if (sp.get() && sp->step())
    return sp->column_string(0);
  return "";
}

string MsnDatabase::get_setting(const string& name) {
  string sql("SELECT value FROM settings WHERE name = '");
  sql.append(name);
  sql.append("'");

  boost::scoped_ptr<dolphinconn::ResultSet> sp(db_.execute_query(sql));
  if (sp.get() && sp->step())
    return sp->column_string(0);
  return "";
}

}  // namespace msn
