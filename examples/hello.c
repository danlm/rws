/* Simplest possible example of a shared object script.
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
 * $Id: hello.c,v 1.3 2002/12/01 16:16:04 rich Exp $
 */

#include "rws_request.h"

int
handle_request (rws_request rq)
{
  http_request http_request = rws_request_http_request (rq);
  io_handle io = rws_request_io (rq);

  int close;
  http_response http_response;

  /* Begin response. */
  http_response = new_http_response (pth_get_pool (current_pth),
				     http_request, io,
				     200, "OK");
  http_response_send_headers (http_response,
			      /* Content type. */
			      "Content-Type", "text/plain",
			      /* End of headers. */
			      NULL);
  close = http_response_end_headers (http_response);

  if (http_request_is_HEAD (http_request)) return close;

  io_fprintf (io, "hello, world!");

  return close;
}
