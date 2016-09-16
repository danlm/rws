/* File serving.
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
 * $Id: file.c,v 1.15 2003/02/05 23:02:51 rich Exp $
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <pool.h>
#include <vector.h>
#include <hash.h>
#include <pstring.h>
#include <pre.h>

#include <pthr_pseudothread.h>
#include <pthr_http.h>
#include <pthr_iolib.h>

#include "process_rq.h"
#include "mime_types.h"
#include "errors.h"
#include "exec.h"
#include "exec_so.h"
#include "cfg.h"
#include "re.h"
#include "file.h"

/* XXX This code doesn't deal with the "If-Modified-Since" header
 * correctly. It is important to get this fixed in the near future.
 *
 * Similarly the code should send "Last-Modified" headers.
 */

struct hash_key
{
  dev_t st_dev;
  ino_t st_ino;
};

struct file_info
{
  struct pool *pool;
  struct stat statbuf;
  void *addr;
};

static pool file_pool = 0;

#define MAX_MMAP_SIZE (10 * 1024 * 1024)
#define MAX_ENTRIES   100
#define MAX_SIZE      (100 * 1024 * 1024)

static int total_size = 0;
static int nr_entries = 0;

/* This is the list of files (of type struct file_info) which are
 * currently memory mapped. It is stored in no particular order and
 * may contain blank entries (where a file has been unmapped for example).
 */
static vector file_list = 0;

/* This is a list of integers indexing into file_list, stored in LRU
 * order. Element 0 is the oldest, and larger numbered elements are
 * younger. New entries are pushed onto the back of this list.
 */
static vector lru_list = 0;

/* This hash of { device, inode } -> integer maps unique stat information
 * about files to their offset in the file_list array above.
 */
static hash file_hash = 0;

static void invalidate_entry (void *);
static int quickly_serve_it (process_rq p, const struct file_info *info, const char *mime_type);
static int slowly_serve_it (process_rq p, int fd, const char *mime_type);
static void expires_header (process_rq p, http_response http_response);

/* Initialize structures. */
void
file_init ()
{
  file_pool = new_subpool (global_pool);
  file_list = new_vector (file_pool, struct file_info);
  lru_list = new_vector (file_pool, int);
  file_hash = new_hash (file_pool, struct hash_key, int);
}

int
file_serve (process_rq p)
{
  vector extv;
  const char *mime_type = 0;
  int offset, fd;
  struct hash_key key;
  struct file_info info;
  void *m;

  /* If this file is an executable .so file, and we are allowed to
   * run .so files from this directory, then it's a shared object
   * script. Hand it off to exec_so.c to run.
   */
  if (cfg_get_bool (p->host, p->alias, "exec so", 0) &&
      prematch (p->pool, p->remainder, re_so, 0) &&
      (p->statbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
    return exec_so_file (p);

  /* If this file is executable, and we are allowed to run files from
   * this directory, then it's a CGI script. Hand it off to exec.c to
   * run.
   */
  if (cfg_get_bool (p->host, p->alias, "exec", 0) &&
      (p->statbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
    return exec_file (p);

  /* Are we permitted to show files in this directory? */
  if (!cfg_get_bool (p->host, p->alias, "show", 0))
    return bad_request_error (p,
			      "you are not permitted to view files "
			      "in this directory");

  /* Map the file's name to its MIME type. */
  if ((extv = prematch (p->pool, p->remainder, re_ext, 0)) != 0)
    {
      char *ext;

      vector_get (extv, 1, ext);
      mime_type = mime_types_get_type (ext);
    }
  if (!mime_type) mime_type = "application/octet-stream"; /* Default. */

  /* Check the hash to see if we know anything about this file already. */
  memset (&key, 0, sizeof key);
  key.st_dev = p->statbuf.st_dev;
  key.st_ino = p->statbuf.st_ino;
  if (hash_get (file_hash, key, offset))
    {
      /* Cache hit ... */
      vector_get (file_list, offset, info);

      /* ... but has the file on disk changed since we mapped it? */
      if (info.statbuf.st_mtime == p->statbuf.st_mtime)
	return quickly_serve_it (p, &info, mime_type);
      else
	/* File has changed: invalidate the cache entry. */
	delete_pool (info.pool);
    }

  /* Try to open the file. */
  fd = open (p->file_path, O_RDONLY);
  if (fd < 0) return file_not_found_error (p);

  /* Set the FD_CLOEXEC flag so that when we fork off CGI scripts, they
   * won't inherit the file descriptor.
   */
  if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0) { perror ("fcntl"); exit (1); }

  /* If the file's too large, don't mmap it. */
  if (p->statbuf.st_size > MAX_MMAP_SIZE)
    return slowly_serve_it (p, fd, mime_type);

  /* Map the file into memory. */
  m = mmap (0, p->statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (m == MAP_FAILED)
    return slowly_serve_it (p, fd, mime_type);

  close (fd);

  /* Evict some entries from the cache to make enough room. */
  while (nr_entries >= MAX_ENTRIES || total_size >= MAX_SIZE)
    {
      vector_get (lru_list, 0, offset);
      vector_get (file_list, offset, info);
      delete_pool (info.pool);
    }

  /* Add the entry to the cache. */
  info.pool = new_subpool (file_pool);
  info.statbuf = p->statbuf;
  info.addr = m;
  nr_entries++;
  total_size += p->statbuf.st_size;

  for (offset = 0; offset < vector_size (file_list); ++offset)
    {
      struct file_info entry;

      vector_get (file_list, offset, entry);
      if (entry.pool == 0)
	{
	  vector_replace (file_list, offset, info);
	  goto added_it;
	}
    }

  vector_push_back (file_list, info);

 added_it:
  hash_insert (file_hash, key, offset);
  vector_push_back (lru_list, offset);

  pool_register_cleanup_fn (info.pool, invalidate_entry, (void *) offset);

  /* Serve it from memory. */
  return quickly_serve_it (p, &info, mime_type);
}

static int
quickly_serve_it (process_rq p, const struct file_info *info,
		  const char *mime_type)
{
  http_response http_response;
  int cl;

  /* Not changed, so it's a real cache hit. */
  http_response = new_http_response (p->pool, p->http_request, p->io,
				     200, "OK");
  http_response_send_headers (http_response,
			      /* Content type. */
			      "Content-Type", mime_type,
			      /* Content length. */
			      "Content-Length", pitoa (p->pool,
						       info->statbuf.st_size),
			      /* End of headers. */
			      NULL);
  expires_header (p, http_response);
  cl = http_response_end_headers (http_response);

  if (http_request_is_HEAD (p->http_request)) return cl;

  io_fwrite (info->addr, info->statbuf.st_size, 1, p->io);

  return cl;
}

static int
slowly_serve_it (process_rq p, int fd, const char *mime_type)
{
  http_response http_response;
  const int n = 4096;
  char *buffer = alloca (n);
  int r, cl;

  /* Cannot memory map this file. Instead fall back to just reading
   * it and sending it back through the socket.
   */
  http_response = new_http_response (p->pool, p->http_request, p->io,
				     200, "OK");
  http_response_send_headers (http_response,
			      /* Content type. */
			      "Content-Type", mime_type,
			      /* Content length. */
			      "Content-Length", pitoa (p->pool,
						       p->statbuf.st_size),
			      /* End of headers. */
			      NULL);
  expires_header (p, http_response);
  cl = http_response_end_headers (http_response);

  if (http_request_is_HEAD (p->http_request)) return cl;

  while ((r = read (fd, buffer, n)) > 0)
    {
      io_fwrite (buffer, r, 1, p->io);
    }

  if (r < 0)
    {
      perror ("read");
    }

  close (fd);

  return cl;
}

/* Send the Expires header, if configured. */
static void
expires_header (process_rq p, http_response http_response)
{
  const char *expires;
  char pm, unit;
  int length;

  expires = cfg_get_string (p->host, p->alias, "expires", 0);
  if (!expires) return;

  /* Parse the configuration string. */
  if (sscanf (expires, "%c%d%c", &pm, &length, &unit) == 3 &&
      (pm == '+' || pm == '-') &&
      length > 0 &&
      (unit == 's' || unit == 'm' || unit == 'h' ||
       unit == 'd' || unit == 'y'))
    {
      time_t t;
      struct tm *tm;
      char header[64];

      time (&t);

      if (pm == '+')
	{
	  switch (unit)
	    {
	    case 's': t += length; break;
	    case 'm': t += length * 60; break;
	    case 'h': t += length * (60 * 60); break;
	    case 'd': t += length * (60 * 60 * 24); break;
	    case 'y': t += length * (60 * 60 * 24 * 366); break;
	    }
	}
      else
	{
	  switch (unit)
	    {
	    case 's': t -= length; break;
	    case 'm': t -= length * 60; break;
	    case 'h': t -= length * (60 * 60); break;
	    case 'd': t -= length * (60 * 60 * 24); break;
	    case 'y': t -= length * (60 * 60 * 24 * 366); break;
	    }
	}

      tm = gmtime (&t);
      strftime (header, sizeof header, "%a, %d %b %Y %H:%M:%S GMT", tm);

      http_response_send_header (http_response, "Expires", header);
    }
  else
    {
      fprintf (stderr, "file.c: expires_header: cannot parse '%s'\n",
	       expires);
    }
}

static void
invalidate_entry (void *offset_ptr)
{
  int offset = (int) offset_ptr;
  int i, j;
  struct file_info info;
  struct hash_key key;

  /* Pull the invalidated entry out of the file_list. */
  vector_get (file_list, offset, info);

  /* Remove from the file_hash. */
  memset (&key, 0, sizeof key);
  key.st_dev = info.statbuf.st_dev;
  key.st_ino = info.statbuf.st_ino;
  if (!hash_erase (file_hash, key)) abort ();

  /* Remove from the lru_list. */
  for (i = 0; i < vector_size (lru_list); ++i)
    {
      vector_get (lru_list, i, j);

      if (j == offset)
	{
	  vector_erase (lru_list, i);
	  goto found_it;
	}
    }
  abort ();

 found_it:
  /* Unmap the memory. */
  munmap (info.addr, info.statbuf.st_size);

  /* Invalidate this entry in the file_list. */
  info.pool = 0;
  vector_replace (file_list, offset, info);

  /* Update counters. */
  nr_entries--;
  total_size -= info.statbuf.st_size;
}
