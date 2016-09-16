/* Request processing thread.
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
 * $Id: process_rq.c,v 1.24 2002/12/01 14:58:02 rich Exp $
 */

#include "config.h"

#include <stdio.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <pool.h>
#include <vector.h>
#include <hash.h>
#include <pstring.h>

#include <pthr_pseudothread.h>
#include <pthr_iolib.h>
#include <pthr_http.h>
#include <pthr_cgi.h>

#include "cfg.h"
#include "file.h"
#include "dir.h"
#include "errors.h"
#include "rewrite.h"
#include "process_rq.h"

/* Maximum number of requests to service in one thread. This just acts
 * as a check on the size of the thread pool, preventing it from growing
 * out of control.
 */
#define MAX_REQUESTS_IN_THREAD 30

#define PR_DEBUG 0		/* Set this to enable debugging. */

static void run (void *vp);

process_rq
new_process_rq (int sock)
{
  pool pool;
  process_rq p;

  pool = new_pool ();
  p = pmalloc (pool, sizeof *p);

  memset (p, 0, sizeof *p);

  /* Set the FD_CLOEXEC flag so that when we fork off CGI scripts, they
   * won't inherit the socket.
   */
  if (fcntl (sock, F_SETFD, FD_CLOEXEC) < 0) { perror ("fcntl"); exit (1); }

  p->sock = sock;
  p->pth = new_pseudothread (pool, run, p, "process_rq");

  pth_start (p->pth);

  return p;
}

#define THREAD_NAME "rws process request thread"

static void
run (void *vp)
{
  process_rq p = (process_rq) vp;
  int close = 0;
  int request_timeout;
  vector path_comps, v;
  int i, is_dir, nr_requests = 1;
  const char *location;

  p->pool = pth_get_pool (p->pth);
  p->io = io_fdopen (p->sock);

  request_timeout = cfg_get_int (0, 0, "request timeout", 60);

  /* Sit in a loop reading HTTP requests. */
  while (!close && nr_requests <= MAX_REQUESTS_IN_THREAD)
    {
      /* Generic name for this thread. */
      pth_set_name (THREAD_NAME " (idle)");

      /* Count the number of requests serviced in this thread. */
      nr_requests++;

      /* Timeout requests. */
      pth_timeout (request_timeout);

      /* Read the request. */
      p->http_request = new_http_request (p->pool, p->io);
      if (p->http_request == 0) /* Normal end of file. */
        break;

      /* Reset timeout. */
      pth_timeout (0);

      /* Choose the correct configuration file based on the Host: header. */
      p->host_header = http_request_get_header (p->http_request, "Host");
      if (p->host_header)
	p->host_header = pstrlwr (pstrdup (p->pool, p->host_header));
      else
	p->host_header = "default";

      if ((p->host = cfg_get_host (p->host_header)) == 0)
	{
	  fprintf (stderr, "unknown virtual host: %s\n", p->host_header);
	  close = bad_request_error (p, "unknown virtual host");
	  continue;
	}

      /* Get the originally requested path. */
      p->requested_path = http_request_path (p->http_request);
      if (!p->requested_path || p->requested_path[0] != '/')
	{
	  close = bad_request_error (p, "bad pathname");
	  continue;
	}

      /* Path may contain % sequences. Unescape them. */
      p->requested_path = cgi_unescape (p->pool, p->requested_path);

      /* If the path ends in a /, then it's a request for a directory.
       * Record this fact now, because pstrcsplit will forget about the
       * trailing slash otherwise.
       */
      is_dir = p->requested_path[strlen (p->requested_path)-1] == '/';

      /* Split up the path into individual components. */
      path_comps = pstrcsplit (p->pool, p->requested_path, '/');

      /* Remove "", "." and ".." components. */
      for (i = 0; i < vector_size (path_comps); ++i)
	{
	  char *comp;

	  vector_get (path_comps, i, comp);

	  if (strcmp (comp, "") == 0 || strcmp (comp, ".") == 0)
	    {
	      vector_erase (path_comps, i);
	      i--;
	    }
	  else if (strcmp (comp, "..") == 0)
	    {
	      if (i > 0)
		{
		  vector_erase_range (path_comps, i-1, i+1);
		  i -= 2;
		}
	      else
		{
		  vector_erase (path_comps, i);
		  i--;
		}
	    }
	}

      /* Construct the canonical path. Add a trailing slash if the
       * original request was for a directory.
       */
      p->canonical_path = psprintf (p->pool, "/%s",
				    pjoin (p->pool, path_comps, "/"));
      if (strlen (p->canonical_path) > 1 && is_dir)
	p->canonical_path = psprintf (p->pool, "%s/", p->canonical_path);

#if PR_DEBUG
      fprintf (stderr, "canonical path is %s\n", p->canonical_path);
#endif

      /* Update the name of the thread with the full request URL. */
      pth_set_name (psprintf (p->pool, THREAD_NAME " http://%s%s",
			      p->host_header,
			      p->canonical_path));

      /* Apply internal and external rewrite rules. */
      i = apply_rewrites (p, p->canonical_path, &location);
      if (i == 1)		/* External rewrite. */
	{
#if PR_DEBUG
	  fprintf (stderr, "external rewrite rule to %s\n", location);
#endif
	  close = moved_permanently (p, location);
	  continue;
	}
      else if (i == 2)		/* Internal rewrite. */
	{
#if PR_DEBUG
	  fprintf (stderr, "internal rewrite rule to %s\n", location);
#endif

	  /* Update the http_request object with the new path. This also
	   * changes the query string held in this object so that the cgi
	   * library works correctly.
	   */
	  http_request_set_url (p->http_request, location);

	  /* Get the path, minus query string. */
	  p->rewritten_path = http_request_path (p->http_request);

	  /* Resplit the path. */
	  path_comps = pstrcsplit (p->pool, p->rewritten_path, '/');
	}

      /* Look for longest matching alias. */
      for (i = vector_size (path_comps); i >= 0; --i)
	{
	  if (i > 0)
	    {
	      v = new_subvector (p->pool, path_comps, 0, i);
	      p->aliasname =
		psprintf (p->pool, "/%s/", pjoin (p->pool, v, "/"));
	    }
	  else
	    p->aliasname = "/";

#if PR_DEBUG
	  fprintf (stderr, "try to find alias matching %s\n", p->aliasname);
#endif

	  if ((p->alias = cfg_get_alias (p->host, p->aliasname)) != 0)
	    goto found_alias;
	}

#if PR_DEBUG
      fprintf (stderr, "no matching alias found\n");
#endif

      /* No alias. */
      close = file_not_found_error (p);
      continue;

    found_alias:
      /* Build up the remainder of the path and the file. */
      v = new_subvector (p->pool, path_comps, i, vector_size (path_comps));
      p->remainder = pjoin (p->pool, v, "/");

      /* Find the root path for this alias. */
      p->root = cfg_get_string (p->host, p->alias, "path", 0);
      if (p->root == 0)
	{
	  close = file_not_found_error (p);
	  continue;
	}

      /* Construct the file path. */
      p->file_path = psprintf (p->pool, "%s/%s", p->root, p->remainder);

#if PR_DEBUG
      fprintf (stderr,
	       "rp = %s, cp = %s, rew = %s, "
	       "an = %s, rem = %s, root = %s, fp = %s, qs = %s\n",
	       p->requested_path, p->canonical_path, p->rewritten_path,
	       p->aliasname, p->remainder, p->root,
	       p->file_path,
	       http_request_query_string (p->http_request) ? : "(null)");
#endif

      /* Find the file to serve and stat it. */
      if (stat (p->file_path, &p->statbuf) == -1)
	{
	  close = file_not_found_error (p);
	  continue;
	}

      /* If it's a directory, but the last component of the name isn't
       * a "/" character, then we need to add a "/" character and send
       * a browser redirect back.
       */
      if (S_ISDIR (p->statbuf.st_mode) &&
	  p->canonical_path[strlen(p->canonical_path)-1] != '/')
	{
	  location = psprintf (p->pool, "%s/", p->requested_path);
	  close = moved_permanently (p, location);
	  continue;
	}

      /* What type of file are we serving? */
      if (S_ISREG (p->statbuf.st_mode))
	{
	  close = file_serve (p);
	  continue;
	}
      else if (S_ISDIR (p->statbuf.st_mode))
	{
	  close = dir_serve (p);
	  continue;
	}

      /* Bad request. */
      close = bad_request_error (p, "not a regular file or directory");
    }

  io_fclose (p->io);

  pth_exit ();
}
