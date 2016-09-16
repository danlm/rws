/* Shared object scripts.
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
 * $Id: rws_request.c,v 1.5 2002/12/01 14:58:02 rich Exp $
 */

#include "config.h"

#include "cfg.h"
#include "rws_request.h"

struct rws_request
{
  http_request http_request;
  io_handle io;
  const char *host_header;
  const char *canonical_path;
  const char *file_path;

  /* These are used for retrieving configuration information.
   * XXX These are also a huge hack which will be removed when we
   * have a decent configuration object type in c2lib.
   */
  void *host;
  void *alias;
  const char * (*cfg_get_string) (void *, void *, const char *, const char *);
  int (*cfg_get_int) (void *, void *, const char *, int);
  int (*cfg_get_bool) (void *, void *, const char *, int);
};

rws_request
new_rws_request (pool pool, http_request http_request, io_handle io,
		 const char *host_header, const char *canonical_path,
		 const char *file_path, void *host, void *alias,
		 const char * (*cfg_get_string)
		 (void *, void *, const char *, const char *),
		 int (*cfg_get_int) (void *, void *, const char *, int),
		 int (*cfg_get_bool) (void *, void *, const char *, int))
{
  rws_request p = pmalloc (pool, sizeof *p);

  p->http_request = http_request;
  p->io = io;
  p->host_header = host_header;
  p->canonical_path = canonical_path;
  p->file_path = file_path;
  p->host = host;
  p->alias = alias;
  p->cfg_get_string = cfg_get_string;
  p->cfg_get_int = cfg_get_int;
  p->cfg_get_bool = cfg_get_bool;

  return p;
}

http_request
rws_request_http_request (rws_request p)
{
  return p->http_request;
}

io_handle
rws_request_io (rws_request p)
{
  return p->io;
}

const char *
rws_request_host_header (rws_request p)
{
  return p->host_header;
}

const char *
rws_request_canonical_path (rws_request p)
{
  return p->canonical_path;
}

const char *
rws_request_file_path (rws_request p)
{
  return p->file_path;
}

const char *
rws_request_cfg_get_string (rws_request p,
			    const char *key, const char *default_value)
{
  return p->cfg_get_string (p->host, p->alias, key, default_value);
}

int
rws_request_cfg_get_int (rws_request p,
			 const char *key, int default_value)
{
  return p->cfg_get_int (p->host, p->alias, key, default_value);
}

int
rws_request_cfg_get_bool (rws_request p,
			  const char *key, int default_value)
{
  return p->cfg_get_bool (p->host, p->alias, key, default_value);
}
