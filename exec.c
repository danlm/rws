/* CGI scripts
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
 * $Id: exec.c,v 1.6 2003/02/05 23:02:51 rich Exp $
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <pool.h>
#include <pstring.h>

#include <pthr_http.h>

#include "process_rq.h"
#include "errors.h"
#include "exec.h"

#ifndef HAVE_SETENV
#ifdef HAVE_PUTENV
/* setenv implementation for architectures which only have putenv. The
 * apparent memory leak in this code does not actually matter because
 * this is only called in the child process just before invoking exec.
 */
static inline void
setenv (const char *name, const char *value, int overwrite)
{
  int len = strlen (name) + strlen (value) + 2;
  char *str = malloc (len);

  snprintf (str, len, "%s=%s", name, value);
  putenv (str);
}
#else
#error "no setenv or putenv in your libc"
#endif
#endif

/* Note: For performance reasons and because I wanted to simplify the
 * server, this code only handles NPH scripts.
 *
 * You must ensure that your CGI program can generate full NPH headers.
 * This is generally quite simple. For example, with Perl's CGI.pm,
 * do this:
 *
 * use CGI qw(:standard -nph);
 */
int
exec_file (process_rq p)
{
  int pid, to_script[2], from_script[2], len, i;
  io_handle to_io, from_io;
  const char *content_length;

  content_length
    = http_request_get_header (p->http_request, "Content-Length");

  /* Set up two pipes between us and the script, one for reading, one
   * for writing.
   */
  if (pipe (to_script) == -1 || pipe (from_script) == -1)
    return bad_request_error (p, "cannot create pipes to script");

  /* Fork off a process to run the request. */
  pid = fork ();
  if (pid == -1)
    {
      close (to_script[0]); close (to_script[1]);
      close (from_script[0]); close (from_script[1]);
      return bad_request_error (p, "cannot fork");
    }

  if (pid == 0)			/* Child process -- runs the script. */
    {
      int j;
      const char *query_string, *content_type;
      int major, minor, method;
      char *header, *env;
      vector headers;
      struct sockaddr_in addr;
      socklen_t addrlen;

      /* XXX Currently all fds will be correctly closed over the exec
       * except the accepting socket. This requires a small change to
       * pthrlib to fix. Ignore it for now.
       */
      /* Set up fds 0 and 1 to point to the pipes connecting us to
       * the main rwsd process. Fd 2 points to the error log, so just
       * leave that one alone.
       */
      close (to_script[1]);
      if (to_script[0] != 0)
	{
	  dup2 (to_script[0], 0);
	  close (to_script[0]);
	}
      close (from_script[0]);
      if (from_script[1] != 1)
	{
	  dup2 (from_script[1], 1);
	  close (from_script[1]);
	}

      /* Query string environment variable. */
      query_string = http_request_query_string (p->http_request);
      if (query_string)
	setenv ("QUERY_STRING", query_string, 1);

      /* Set server protocol. */
      http_request_version (p->http_request, &major, &minor);
      setenv ("SERVER_PROTOCOL",
	      psprintf (p->pool, "HTTP/%d.%d", major, minor), 1);

      /* Set request method. */
      method = http_request_method (p->http_request);
      setenv ("REQUEST_METHOD",
	      (method == HTTP_METHOD_GET ? "GET" :
	       (method == HTTP_METHOD_POST ? "POST" :
		(method == HTTP_METHOD_HEAD ? "HEAD" :
		 "unknown"))), 1);

      /* Content length, content type. */
      if (content_length) setenv ("CONTENT_LENGTH", content_length, 1);
      content_type
	= http_request_get_header (p->http_request, "Content-Type");
      if (content_type) setenv ("CONTENT_TYPE", content_type, 1);

      /* Get peer address. */
      addrlen = sizeof addr;
      getpeername (p->sock, (struct sockaddr *) &addr, &addrlen);

      /* General CGI environment variables. */
      setenv ("SERVER_SOFTWARE", http_get_servername (), 1);
      setenv ("SERVER_NAME", p->host_header, 1);
      setenv ("GATEWAY_INTERFACE", "CGI/1.1", 1);
      /*setenv ("SERVER_PORT", pitoa (p->pool, port), 1); XXX */
      setenv ("PATH_INFO", p->canonical_path, 1);
      setenv ("PATH_TRANSLATED", p->file_path, 1);
      setenv ("SCRIPT_NAME", p->canonical_path, 1);
      setenv ("REMOTE_ADDR", inet_ntoa (addr.sin_addr), 1);

      /* Convert any other headers into HTTP_* environment variables. */
      headers = http_request_get_headers (p->http_request);
      for (i = 0; i < vector_size (headers); ++i)
	{
	  vector_get (headers, i, header);
	  env = pstrdup (p->pool, header);
	  pstrupr (env);
	  for (j = 0; j < strlen (env); ++j)
	    if (env[j] == '-') env[j] = '_';
	  env = psprintf (p->pool, "HTTP_%s", env);
	  setenv (env, http_request_get_header (p->http_request, header), 1);
	}

      /* Run the CGI script. */
      execl (p->file_path, p->file_path, 0);

      perror ("exec");
      exit (1);
    }

  /* Close the unneeded halves of each pipe. */
  close (to_script[0]);
  close (from_script[1]);

  /* Set the ends of the pipes to non-blocking mode. */
  if (fcntl (to_script[1], F_SETFL, O_NONBLOCK) < 0 ||
      fcntl (from_script[0], F_SETFL, O_NONBLOCK) < 0)
    { perror ("fcntl"); exit (1); }

  /* Associate the ends of the pipe with IO handles. This will also
   * close them automagically in case of error.
   */
  to_io = io_fdopen (to_script[1]);
  from_io = io_fdopen (from_script[0]);
  if (to_io == 0 || from_io == 0)
    return bad_request_error (p, "error associating pipes with IO handles");

  /* If this is a POST method, copy the required amount of data
   * to the CGI script.
   */
  if (http_request_method (p->http_request) == HTTP_METHOD_POST)
    {
      /* How much to copy? Is content-length set? */
      len = -1;
      if (content_length) sscanf (content_length, "%d", &len);

      /* Copy the data to the script. */
      io_copy (p->io, to_io, len);
    }

  /* Read data back from the script and out to the client. */
  io_copy (from_io, p->io, -1);

  /* Force us to close the connection back to the client now. */
  return 1;
}
