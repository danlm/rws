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
 * $Id: exec_so.c,v 1.10 2003/01/31 14:36:22 rich Exp $
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <pool.h>
#include <hash.h>

#include <pthr_pseudothread.h>
#include <pthr_iolib.h>
#include <pthr_http.h>
#include <pthr_cgi.h>

#include "rws_request.h"
#include "process_rq.h"
#include "errors.h"
#include "cfg.h"
#include "exec_so.h"

/* XXX make+ configure should figure this out. */
#ifndef __OpenBSD__
#define HANDLE_REQUEST_SYM "handle_request"
#else
#define HANDLE_REQUEST_SYM "_handle_request"
#endif

static shash cache = 0;
struct shared_object
{
  void *dl_handle;		/* Handle returned by dlopen(3) */
				/* Pointer to 'handle_request' fn. */
  int (*handle_request) (rws_request rq);
  time_t mtime;			/* Modification time of this file at load. */
  int use_count;		/* Number of current users. */
};

/* This structure is used when jumping into the handle_request function,
 * so we can catch errors and return values from this function.
 */
struct fn_result
{
  struct shared_object *so;	/* Parameter to the call. */
  rws_request rq;		/* Parameter to the call. */
  int close;			/* Return value from the call. */
};

static void call_handle_request (void *data);
static int do_error (process_rq p, const char *msg);

void
exec_so_init ()
{
  cache = new_shash (global_pool, struct shared_object *);
}

int
exec_so_file (process_rq p)
{
  struct shared_object *so;
  const char *error;
  rws_request rq;
  struct fn_result fn_result;

  /* Check our cache of currently loaded .so files to see if this one
   * has already been loaded.
   */
  if (!shash_get (cache, p->file_path, so))
    {
      /* No: Need to dlopen this file. */
      so = pmalloc (global_pool, sizeof *so);

    reload:
      so->dl_handle = dlopen (p->file_path,
#ifndef __OpenBSD__
			      RTLD_NOW
#else
			      O_RDWR
#endif
                             );
      if (so->dl_handle == 0)
	{
	  fprintf (stderr, "%s\n", dlerror ());
	  return bad_request_error (p,
				    "failed to load shared object file");
	}

      /* Check it contains the 'handle_request' function. */
      so->handle_request = dlsym (so->dl_handle, HANDLE_REQUEST_SYM);
      if ((error = dlerror ()) != 0)
	{
	  fprintf (stderr, "%s\n", error);
	  dlclose (so->dl_handle);
	  return bad_request_error (p,
				    "shared object file does not contain "
				    "handle_request function");
	}

      so->mtime = p->statbuf.st_mtime;
      so->use_count = 0;

      /* Add it to the cache. */
      shash_insert (cache, p->file_path, so);
    }

  /* Check the modification time. We may need to reload this script if it's
   * changed on disk. But if there are other current users, then we can't
   * safely unload the library, so don't try (a later request will reload
   * it when it's quiet anyway).
   */
  if (p->statbuf.st_mtime > so->mtime && so->use_count == 0)
    {
      shash_erase (cache, p->file_path);
      dlclose (so->dl_handle);
      goto reload;
    }

  /* OK, we're now about to use this file. */
  so->use_count++;

  /* Generate the rws_request object. */
  rq = new_rws_request (p->pool,
			p->http_request,
			p->io,
			p->host_header,
			p->canonical_path,
			p->file_path,
			p->host,
			p->alias,
			cfg_get_string,
			cfg_get_int,
			cfg_get_bool);

  /* Call the 'handle_request' function.
   * XXX We could pass environment parameters here, but this requires
   * a change to pthrlib to allow environment variables to be handled
   * across context switches.
   */
  fn_result.so = so;
  fn_result.rq = rq;
  error = pth_catch (call_handle_request, &fn_result);

  /* Finished using the file. */
  so->use_count--;

  if (error)
    return do_error (p, error);

  return fn_result.close;
}

static void
call_handle_request (void *data)
{
  struct fn_result *fn_result = (struct fn_result *) data;

  fn_result->close = fn_result->so->handle_request (fn_result->rq);
}

static int
do_error (process_rq p, const char *msg)
{
  /* XXX In the future, we'd like to extend this function so that
   * other non-500 errors can be displayed (particularly for 404
   * Page Not Found errors).
   */
  return bad_request_error (p, msg);
}
