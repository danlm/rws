/* RWS main program.
 * - by Richard W.M. Jones <rich@annexia.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: main.c,v 1.16 2002/11/27 18:45:23 rich Exp $
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <pool.h>
#include <pstring.h>
#include <pre.h>

#include <pthr_http.h>
#include <pthr_server.h>

#include "cfg.h"
#include "file.h"
#include "exec_so.h"
#include "mime_types.h"
#include "process_rq.h"
#include "rewrite.h"
#include "re.h"

static void startup (int argc, char *argv[]);
static void start_thread (int sock, void *data);
static void catch_reload_signal (int sig);
static void catch_quit_signal (int sig);
static void catch_child_signal (int sig);
static void reload_config (void);

const char *config_path = "/etc/rws";
FILE *access_log;

const pcre *re_alias_start,
  *re_alias_end,
  *re_begin,
  *re_conf_line,
  *re_ext,
  *re_icon,
  *re_so,
  *re_ws,
  *re_comma;

int
main (int argc, char *argv[])
{
  const char *user, *name, *stderr_file;
  int c, stack_size;
  struct sigaction sa;
  int foreground = 0;
  int debug = 0;

  /* Initialise various shared regular expressions. */
  re_alias_start = precomp (global_pool, "^alias[[:space:]]+(.*)$", 0);
  re_alias_end = precomp (global_pool, "^end[[:space:]]+alias$", 0);
  re_begin = precomp (global_pool, "^begin[[:space:]]+(.*):?[[:space:]]*$", 0);
  re_conf_line = precomp (global_pool, "^(.*):[[:space:]]*(.*)?$", 0);
  re_ext = precomp (global_pool, "\\.([^.]+)$", 0);
  re_icon = precomp (global_pool,
    "([^[:space:]]+)[[:space:]]+([0-9]+)x([0-9]+)[[:space:]]+\"(.*)\"", 0);
  re_so = precomp (global_pool, "\\.so$", 0);
  re_ws = precomp (global_pool, "[ \t]+", 0);
  re_comma = precomp (global_pool, "[,;]+", 0);

  while ((c = getopt (argc, argv, "C:p:a:fd")) != -1)
    {
      switch (c)
	{
	case 'p':
	  /* ignore */
	  break;
 
        case 'a':
          /* ignore */
          break;

	case 'C':
	  config_path = optarg;
	  break;

        case 'f':
          foreground = 1;
          break;
 
        case 'd':
          debug = 1;
          break;

	default:
	  fprintf (stderr, "usage: rws [-d] [-f] [-a address] [-p port] [-C configpath]\n");
	  exit (1);
	}
    }

  /* Read configuration file. Do this early so we have configuration
   * data available for other initializations.
   */
  reload_config ();

  /* Change the thread stack size? */
  stack_size = cfg_get_int (0, 0, "stack size", 0);
  if (stack_size)
    pseudothread_set_stack_size (stack_size * 1024);

  /* Initialize the file cache. */
  file_init ();

  /* Initialize the shared object script cache. */
  exec_so_init ();

  /* Intercept signals. */
  memset (&sa, 0, sizeof sa);
  sa.sa_handler = catch_reload_signal;
  sa.sa_flags = SA_RESTART;
  sigaction (SIGHUP, &sa, 0);

  sa.sa_handler = catch_quit_signal;
  sigaction (SIGINT, &sa, 0);
  sigaction (SIGQUIT, &sa, 0);
  sigaction (SIGTERM, &sa, 0);

  sa.sa_handler = catch_child_signal;
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigaction (SIGCHLD, &sa, 0);

  /* ... but ignore SIGPIPE errors. */
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = SA_RESTART;
  sigaction (SIGPIPE, &sa, 0);

  /* Change user on startup. */
  user = cfg_get_string (0, 0, "user", "nobody");
  pthr_server_username (user);

  if (foreground)
    {
      pthr_server_disable_chdir ();
      pthr_server_disable_fork ();
    }

  if (debug)
    pthr_server_disable_close ();
  else
    {
      /* Errors to error log file. */
      stderr_file = cfg_get_string (0, 0, "error log", "/tmp/error_log");
      pthr_server_stderr_file (stderr_file);

      /* Enable stack trace on SIGSEGV. */
      pthr_server_enable_stack_trace_on_segv ();
    }

  /* Set server name. */
  name = psprintf (global_pool,
		   PACKAGE "/" VERSION " %s",
		   http_get_servername ());
  http_set_servername (name);

  /* Extra startup. */
  pthr_server_startup_fn (startup);

  /* Start up the server. */
  pthr_server_main_loop (argc, argv, start_thread);

  exit (0);
}

static void
startup (int argc, char *argv[])
{
  FILE *access_log;

  /* Open the access log. */
  access_log
    = fopen (cfg_get_string (0, 0, "access log", "/tmp/access_log"), "a");
  if (access_log == 0)
    {
      perror ("open: access log");
      exit (1);
    }
  if (fcntl (fileno (access_log), F_SETFD, FD_CLOEXEC) < 0)
    { perror ("fcntl"); exit (1); }

  http_set_log_file (access_log);
}

static void
start_thread (int sock, void *data)
{
  (void) new_process_rq (sock);
}

static void
catch_reload_signal (int sig)
{
  reload_config ();
}

static void
catch_quit_signal (int sig)
{
  /* Exit gracefully (how!?!) XXX */
  exit (0);
}

static void
catch_child_signal (int sig)
{
  /* Clean up the child process. */
  wait (0);
}

static void
reload_config ()
{
  /* Reread configuration file. */
  cfg_reread_config (config_path);

  /* Read /etc/mime.types file. */
  mime_types_reread_config (cfg_get_string (0, 0, "mime types file",
					    "/etc/mime.types"));

  /* Reset rewrite rules. */
  rewrite_reset_rules ();
}
