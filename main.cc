/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include <set>

#include <openssl/crypto.h>
#include <event.h>

#include "connection.h"
#include "server.h"
#include "msn/msn.h"
#include "acl.h"
#include "word_filter.h"
#include "history/history_logger.h"
#include "config.h"
#include "log.h"
#include "version.h"

static void droppriv(const char* user);
static bool check_pid(const char* pid_file);
static void write_pid(const char* pid_file);
static void signal_cb(int sig, short event, void* arg);
static void cleanup(void);

// XXX: This is ugly.
extern std::set<Connection*> connections;

int verbose = 0;

uint16_t listen_port = 11863;
bool use_syslog = false;
bool show_payload = false;

static void droppriv(const char* user) {
  struct passwd* pw = NULL;

  if ((pw = getpwnam(user)) == NULL)
    errx(1, "%s: unknown user %s", __func__, user);

  if (setgroups(1, &pw->pw_gid) == -1)
    err(1, "%s: setgroups(%d) failed", __func__, pw->pw_gid);

  if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1)
    err(1, "%s: setresgid(%d) failed", __func__, pw->pw_gid);

  if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
    err(1, "%s: setresuid(%d) failed", __func__, pw->pw_uid);
}

static bool check_pid(const char* pid_file) {
  if (pid_file) {
    FILE* fp = fopen(pid_file, "r");

    if (fp) {
      pid_t pid;
      int result;

      result = fscanf(fp, "%d", &pid);
      fclose(fp);

      if (result != 1 || kill(pid, 0) == 0)
        return true;
    }
  }

  return false;
}

static void write_pid(const char* pid_file) {
  FILE* fp = fopen(pid_file, "w");
  if (fp) {
    fprintf(fp, "%d\n", getpid());
    fclose(fp);

    chmod(pid_file, 0644);
  } else {
    err(1, "fopen");
  }
}

static void signal_cb(int sig, short event, void* arg) {
  switch (sig) {
  case SIGTERM:
  case SIGINT:
  case SIGHUP:
    log_info("ohhh nooooo mr. bill!");
    cleanup();
    event_loopexit(NULL);
    break;
  }
}

static void cleanup(void) {
  DLOG(2, "%s: called", __func__);

  std::set<Connection*>::iterator c, nxt;

  for (c = connections.begin(); c != connections.end(); c = nxt) {
    nxt = c;
    ++nxt;
    delete *c;
  }

  connections.clear();
}

static void usage(const char* progname) {
  fprintf(stderr,
          "usage: %s [-c configfile] [-p port] [-l ipaddr] [-u user] "
          "[-F pidfile]\n"
          "\t -d run as a daemon\n"
          "\t -P show payload\n"
#ifdef DEBUG
          "\t -v verbosity level\n"
#endif
          "\t -V print version number of wlmproxy\n"
          "\t -h this message\n",
          progname);
}

int main(int argc, char** argv) {
  struct event_base* base;
  struct event ev_sighup, ev_sigint, ev_sigterm;
  const char* listen_ip = "0.0.0.0";
  const char* username = NULL;
  const char* config_file = "wlmproxy.conf";
  const char* pid_file = NULL;
  int c;
  bool show_version = false;
  bool daemonize = false;

  while ((c = getopt(argc, argv, "Vp:hvdl:u:F:Pc:")) > 0)
    switch (c) {
    case 'V':
      show_version = true;
      break;
    case 'p':
      listen_port = atoi(optarg);
      if (!listen_port) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
      }
      break;
    case 'h':
      usage(argv[0]);
      exit(EXIT_SUCCESS);
#ifdef DEBUG
    case 'v':
      verbose++;
      break;
#endif
    case 'l':
      listen_ip = optarg;
      break;
    case 'd':
      daemonize = true;
      break;
    case 'u':
      username = optarg;
      break;
    case 'F':
      pid_file = optarg;
      break;
    case 'P':
      show_payload = true;
      break;
    case 'c':
      config_file = optarg;
      break;
    default:
      usage(argv[0]);
      exit(EXIT_FAILURE);
    }

  if (show_version) {
    fprintf(stderr, "wlmproxy %s\n", VERSION);
    exit(EXIT_SUCCESS);
  }

  argc -= optind;
  argv += optind;

  if (username != NULL && *username != '\0') {
    if (geteuid())
      errx(1, "need root privileges");
    droppriv(username);
  }

  if (check_pid(pid_file))
    errx(1, "already running");

  if (daemonize) {
    if (daemon(0, 0) == -1)
      errx(1, "unable to daemonize");
    use_syslog = true;
  }

  log_init();

  base = event_init();

  setlocale(LC_ALL, "");

  if (!Config::instance().read(config_file))
    errx(1, "config file '%s' not found", config_file);

  msn::msn_init();
  acl_init();
  word_filter_init();

  HistoryLogger* logger = HistoryLogger::instance();

  if (pid_file)
    write_pid(pid_file);

  // Setup signal handler
  signal(SIGPIPE, SIG_IGN);
  signal_set(&ev_sighup, SIGHUP, signal_cb, NULL);
  signal_set(&ev_sigint, SIGINT, signal_cb, NULL);
  signal_set(&ev_sigterm, SIGTERM, signal_cb, NULL);
  signal_add(&ev_sighup, NULL);
  signal_add(&ev_sigint, NULL);
  signal_add(&ev_sigterm, NULL);

  Server* server = new Server(listen_ip, listen_port);

  log_info("starting up on %s:%hu", listen_ip, listen_port);

  event_dispatch();

  delete server;

  // Cleanup
  event_base_free(base);

  // Shutdown
  logger->destroy();
  Config::destroy();

  // Make valgrind happy
  CRYPTO_cleanup_all_ex_data();

  if (pid_file)
    unlink(pid_file);

  return EXIT_SUCCESS;
}
