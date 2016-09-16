/* More complex example shared object script showing parameter parsing.
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
 * $Id: show_params.c,v 1.4 2002/12/01 16:16:04 rich Exp $
 */

#include <pool.h>
#include <vector.h>
#include <pthr_cgi.h>

#include "rws_request.h"

int
handle_request (rws_request rq)
{
  pool pool = pth_get_pool (current_pth);
  http_request http_request = rws_request_http_request (rq);
  io_handle io = rws_request_io (rq);

  cgi cgi;
  int close, i;
  http_response http_response;
  vector headers, params;
  struct http_header header;
  const char *name, *value;

  /* Parse CGI parameters. */
  cgi = new_cgi (pool, http_request, io);

  /* Begin response. */
  http_response = new_http_response (pool, http_request, io,
				     200, "OK");
  http_response_send_headers (http_response,
			      /* Content type. */
			      "Content-Type", "text/plain",
			      /* End of headers. */
			      NULL);
  close = http_response_end_headers (http_response);

  if (http_request_is_HEAD (http_request)) return close;

  io_fprintf (io, "This is the show_params shared object script.\r\n\r\n");
  io_fprintf (io, "Your browser sent the following headers:\r\n\r\n");

  headers = http_request_get_headers (http_request);
  for (i = 0; i < vector_size (headers); ++i)
    {
      vector_get (headers, i, header);
      io_fprintf (io, "\t%s: %s\r\n", header.key, header.value);
    }

  io_fprintf (io, "----- end of headers -----\r\n");

  io_fprintf (io, "The URL was: %s\r\n",
	      http_request_get_url (http_request));
  io_fprintf (io, "The path component was: %s\r\n",
	      http_request_path (http_request));
  io_fprintf (io, "The query string was: %s\r\n",
	      http_request_query_string (http_request));
  io_fprintf (io, "The query arguments were:\r\n");

  params = cgi_params (cgi);
  for (i = 0; i < vector_size (params); ++i)
    {
      vector_get (params, i, name);
      value = cgi_param (cgi, name);
      io_fprintf (io, "\t%s=%s\r\n", name, value);
    }

  io_fprintf (io, "----- end of parameters -----\r\n");

  return close;
}
