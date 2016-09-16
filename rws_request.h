/* RWS request object, passed to shared object scripts.
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
 * $Id: rws_request.h,v 1.4 2002/12/01 14:58:02 rich Exp $
 */

#ifndef RWS_REQUEST_H
#define RWS_REQUEST_H

#include <pool.h>
#include <pthr_pseudothread.h>
#include <pthr_http.h>
#include <pthr_iolib.h>

struct rws_request;
typedef struct rws_request *rws_request;

/* This is the private interface to building a new rws_request object. It
 * is called inside rwsd. Shared object scripts will never need to call
 * this. Use the public interface below only.
 */
extern rws_request new_rws_request (pool, http_request, io_handle, const char *host_header, const char *canonical_path, const char *file_path, void *host, void *alias, const char * (*cfg_get_string) (void *, void *, const char *, const char *), int (*cfg_get_int) (void *, void *, const char *, int), int (*cfg_get_bool) (void *, void *, const char *, int));

/* Function: rws_request_http_request - retrieve fields in rws_request object
 * Function: rws_request_io
 * Function: rws_request_host_header
 * Function: rws_request_canonical_path
 * Function: rws_request_file_path
 * Function: rws_request_cfg_get_string
 * Function: rws_request_cfg_get_int
 * Function: rws_request_cfg_get_bool
 *
 * These functions retrieve the fields in an @code{rws_request} object.
 * This object is passed to shared object scripts when they are invoked
 * by rws as:
 *
 * @code{int handle_request (rws_request rq)}
 *
 * @code{rws_request_http_request} returns the current HTTP request
 * (see @ref{new_http_request(3)}). To parse the CGI parameters, you
 * need to call @code{new_cgi} (see @ref{new_cgi(3)}).
 *
 * @code{rws_request_io} returns the IO handle connected to the
 * browser.
 *
 * @code{rws_request_host_header} returns the contents of the
 * HTTP @code{Host:} header, or the string @code{default} if none was given.
 *
 * @code{rws_request_canonical_path} returns the canonical path
 * requested by the browser (after removing @code{..}, @code{//}, etc.),
 * eg. @code{/so-bin/file.so}.
 *
 * @code{rws_request_file_path} returns the actual path to the
 * SO file being requested, eg. @code{/usr/share/rws/so-bin/file.so}.
 *
 * @code{rws_request_cfg_get_string} returns the configuration file
 * string for @code{key}. If there is no entry in the configuration
 * file, this returns @code{default_value}.
 *
 * @code{rws_request_cfg_get_int} returns the string converted to
 * an integer.
 *
 * @code{rws_request_cfg_get_bool} returns the string converted to
 * a boolean.
 *
 * See also: @ref{new_cgi(3)}, @ref{new_http_response(3)},
 * @ref{new_http_request(3)}, pthrlib tutorial, rws @code{examples/}
 * directory.
 */
extern http_request rws_request_http_request (rws_request);
extern io_handle rws_request_io (rws_request);
extern const char *rws_request_host_header (rws_request);
extern const char *rws_request_canonical_path (rws_request);
extern const char *rws_request_file_path (rws_request);
extern const char *rws_request_cfg_get_string (rws_request, const char *key, const char *default_value);
extern int rws_request_cfg_get_int (rws_request, const char *key, int default_value);
extern int rws_request_cfg_get_bool (rws_request, const char *key, int default_value);

#endif /* RWS_REQUEST_H */
