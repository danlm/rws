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
 * $Id: process_rq.h,v 1.7 2002/09/02 07:50:09 rich Exp $
 */

#ifndef PROCESS_RQ_H
#define PROCESS_RQ_H

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <pool.h>

#include <pthr_pseudothread.h>
#include <pthr_http.h>
#include <pthr_iolib.h>

/* The PROCESS_RQ type is both the pseudothread object which handles
 * the request, and also the request information structure itself.
 */

struct process_rq
{
  pseudothread pth;		/* Pseudothread handle. */
  pool pool;			/* Thread pool for all allocations. */
  int sock;			/* Socket fd. */
  io_handle io;			/* IO handle. */
  http_request http_request;	/* HTTP request object. */
  const char *host_header;	/* Host header or "default". */

  /* These are used to establish context by the cfg (configuration) code. */
  void *host;			/* Host object. */
  void *alias;			/* Alias object. */

  /* The various different paths.
   *
   * REQUESTED_PATH is the path as requested by the user (sans query
   * string). This path is grotty, containing ".", "..", "///", etc. Do
   * not use it, except perhaps when displaying error messages.
   *
   * CANONICAL_PATH is the requested path cleaned up to remove ".", ".."
   * etc.
   *
   * REWRITTEN_PATH is the path after internal rewrite rules have been
   * applied.
   *
   * ALIASNAME is the alias which matches this path. REMAINDER is the
   * remaining part of the path. Thus (in theory at least), ALIASNAME +
   * REMAINDER == CANONICAL_PATH.
   *
   * ROOT is the document root corresponding to the matching alias. This
   * corresponds to the actual path of the file on disk. FILE_PATH is
   * the full path to the actual file on disk. Thus, ROOT + "/" + REMAINDER
   * == FILE_PATH.
   *
   * Directories are always followed by a "/". If a user requests a
   * directory which isn't followed by a "/" then the path parsing code
   * transparently issues a 301 (Permanently Moved) browser redirect
   * including the corrected path.
   *
   * Example (no internal rewrite):
   *   REQUESTED_PATH       "/cgi-bin/../docs/dir///file.html"
   *   CANONICAL_PATH       "/docs/dir/file.html"
   *   REWRITTEN_PATH       "/docs/dir/file.html"
   *   ALIASNAME            "/docs/"
   *   REMAINDER            "dir/file.html"
   *   ROOT                 "/home/rich/mydocs"
   *   FILE_PATH            "/home/rich/mydocs/dir/file.html"
   *
   * Example (with internal rewrite):
   *   REQUESTED_PATH       "/cgi-bin/../docs/dir///file.html"
   *   CANONICAL_PATH       "/docs/dir/file.html"
   *   REWRITTEN_PATH       "/newdocs/dir/file.html"
   *   ALIASNAME            "/newdocs/"
   *   REMAINDER            "dir/file.html"
   *   ROOT                 "/home/rich/mynewdocs"
   *   FILE_PATH            "/home/rich/mynewdocs/dir/file.html"
   */
  const char *requested_path;
  const char *canonical_path;
  const char *rewritten_path;
  const char *aliasname;
  const char *remainder;
  const char *root;
  const char *file_path;

  struct stat statbuf;		/* Stat of file. */
};

typedef struct process_rq *process_rq;

extern process_rq new_process_rq (int sock);

/* Define some RFC-compliant dates to represent past and future. */
#define DISTANT_PAST   "Thu, 01 Dec 1994 16:00:00 GMT"
#define DISTANT_FUTURE "Sun, 01 Dec 2030 16:00:00 GMT"

/* Headers which are sent to defeat caches. */
#define NO_CACHE_HEADERS "Cache-Control", "must-revalidate", "Expires", DISTANT_PAST, "Pragma", "no-cache"

#define CRLF "\r\n"

#endif /* PROCESS_RQ_H */
