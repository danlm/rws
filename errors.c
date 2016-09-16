/* Deliver errors back to the user.
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
 * $Id: errors.c,v 1.5 2002/12/01 14:58:01 rich Exp $
 */

#include "config.h"

#include <pool.h>

#include <pthr_pseudothread.h>
#include <pthr_http.h>
#include <pthr_iolib.h>

#include "process_rq.h"
#include "cfg.h"
#include "errors.h"

int
bad_request_error (process_rq p, const char *text)
{
  http_response http_response;
  int close;
  const char *maintainer;

  maintainer = cfg_get_string (p->host, p->alias,
			       "maintainer", "(no maintainer)"); /* XXX */

  http_response = new_http_response (p->pool, p->http_request, p->io,
				     500, "Internal server error");
  http_response_send_headers (http_response,
			      /* Content type. */
			      "Content-Type", "text/html",
			      NO_CACHE_HEADERS,
			      /* End of headers. */
			      NULL);
  close = http_response_end_headers (http_response);

  if (http_request_is_HEAD (p->http_request)) return close;

  /* XXX Escaping. */
  io_fprintf (p->io,
	      "<html><head><title>Internal server error</title></head>" CRLF
	      "<body bgcolor=\"#ffffff\">" CRLF
	      "<h1>500 Internal server error</h1>" CRLF
	      "There was an error serving this request:" CRLF
	      "<pre>" CRLF
	      "%s" CRLF
	      "</pre>" CRLF
	      "<hr>" CRLF
	      "<address>%s</address>" CRLF
	      "</body></html>" CRLF,
	      text, maintainer);

  /* It's always a good idea to force the connection to close after an
   * error. This is particularly important with monolith applications
   * after they have thrown an exception.
   */
  /* return close; */
  return 1;
}

int
file_not_found_error (process_rq p)
{
  http_response http_response;
  int close;
  const char *maintainer;

  maintainer = cfg_get_string (p->host, p->alias,
			       "maintainer", "(no maintainer)");

  http_response = new_http_response (p->pool, p->http_request, p->io,
				     404, "File or directory not found");
  http_response_send_headers (http_response,
			      /* Content type. */
			      "Content-Type", "text/html",
			      NO_CACHE_HEADERS,
			      /* End of headers. */
			      NULL);
  close = http_response_end_headers (http_response);

  if (http_request_is_HEAD (p->http_request)) return close;

  io_fprintf (p->io,
	      "<html><head><title>File or directory not found</title></head>" CRLF
	      "<body bgcolor=\"#ffffff\">" CRLF
	      "<h1>404 File or directory not found</h1>" CRLF
	      "The file you requested was not found on this server." CRLF
	      "<hr>" CRLF
	      "<address>%s</address>" CRLF
	      "</body></html>" CRLF,
	      maintainer);

  return close;
}

int
moved_permanently (process_rq p, const char *location)
{
  http_response http_response;
  int close;

  http_response = new_http_response (p->pool, p->http_request, p->io,
				     301, "Moved permanently");
  http_response_send_headers (http_response,
			      /* Content length. */
			      "Content-Length", "0",
			      /* Location. */
			      "Location", location,
			      /* End of headers. */
			      NULL);
  close = http_response_end_headers (http_response);

  if (http_request_is_HEAD (p->http_request)) return close;

  return close;
}
