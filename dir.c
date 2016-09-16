/* Directory serving.
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
 * $Id: dir.c,v 1.13 2003/02/05 23:02:51 rich Exp $
 */

#include "config.h"

#include <assert.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#include <pool.h>
#include <pstring.h>
#include <vector.h>
#include <pre.h>

#include <pthr_pseudothread.h>
#include <pthr_http.h>
#include <pthr_iolib.h>

#include "process_rq.h"
#include "mime_types.h"
#include "file.h"
#include "errors.h"
#include "cfg.h"
#include "re.h"
#include "dir.h"

static void choose_icon (process_rq p,
			 const char *filename, const struct stat *statbuf,
			 const char **icon, const char **icon_alt,
			 int *icon_width, int *icon_height);
static void standard_icon (process_rq p, const char *name,
			   const char **icon, const char **icon_alt,
			   int *icon_width, int *icon_height);
static void unknown_icon (process_rq p,
			  const char **icon, const char **icon_alt,
			  int *icon_width, int *icon_height);
static int parse_icon_str (process_rq p, const char *icon_str,
			   const char **icon, const char **icon_alt,
			   int *icon_width, int *icon_height);
static const char *get_printable_size (process_rq p,
				       const struct stat *statbuf);
static const char *get_link_field (process_rq p, const char *filename);

static int
my_strcmp (const char **p1, const char **p2)
{
  return strcmp (*p1, *p2);
}

int
dir_serve (process_rq p)
{
  http_response http_response;
  int close, i;
  char *index_file;
  struct stat index_statbuf;
  DIR *dir;
  struct dirent *d;
  vector files;

  /* Is there an index file in this directory? If so, internally redirect
   * the request to that file.
   */
  index_file = psprintf (p->pool, "%s/index.html", p->file_path);
  if (stat (index_file, &index_statbuf) == 0 &&
      S_ISREG (index_statbuf.st_mode))
    {
      /* Update the request structure appropriately. */
      p->file_path = index_file;
      p->remainder = psprintf (p->pool, "%s/index.html", p->remainder);
      p->statbuf = index_statbuf;

      /* Serve the file. */
      return file_serve (p);
    }

  /* Are we allowed to generate a directory listing? */
  if (!cfg_get_bool (p->host, p->alias, "list", 0))
    return bad_request_error (p, "directory listing not allowed");

  /* Yes: read the files into a local vector. */
  dir = opendir (p->file_path);
  if (dir == 0)
    return bad_request_error (p, "error opening directory");

  files = new_vector (p->pool, const char *);
  while ((d = readdir (dir)) != 0)
    {
      if (d->d_name[0] != '.')	/* Ignore hidden files. */
	{
	  char *name = pstrdup (p->pool, d->d_name);
	  vector_push_back (files, name);
	}
    }
  closedir (dir);

  /* Sort them into alphabetical order. */
  psort (files, my_strcmp);

  /* Not changed, so it's a real cache hit. */
  http_response = new_http_response (p->pool, p->http_request, p->io,
				     200, "OK");
  http_response_send_headers (http_response,
			      /* Content type. */
			      "Content-Type", "text/html",
			      /* End of headers. */
			      NULL);
  close = http_response_end_headers (http_response);

  if (http_request_is_HEAD (p->http_request)) return close;

  io_fprintf (p->io,
	      "<html><head><title>Directory listing: %s</title></head>" CRLF
	      "<body bgcolor=\"#ffffff\">" CRLF
	      "<h1>Directory listing: %s</h1>" CRLF
	      "<a href=\"..\">Go up to parent directory</a>" CRLF
	      "<table border=\"0\">" CRLF,
	      p->canonical_path, p->canonical_path);

  for (i = 0; i < vector_size (files); ++i)
    {
      const char *name, *pathname, *icon, *icon_alt;
      const char *size = "", *link_field = "";
      int icon_width, icon_height;
      struct stat file_statbuf;

      vector_get (files, i, name);

      /* Generate the full pathname. */
      pathname = psprintf (p->pool, "%s/%s", p->file_path, name);

      /* Stat the file to get type and size information. */
      if (lstat (pathname, &file_statbuf) == 0)
	{
	  /* Choose an icon type. */
	  choose_icon (p, name, &file_statbuf, &icon, &icon_alt,
		       &icon_width, &icon_height);

	  /* Get the size. */
	  if (S_ISREG (file_statbuf.st_mode))
	    size = get_printable_size (p, &file_statbuf);

	  /* If it's a link, get the link field. */
	  if (S_ISLNK (file_statbuf.st_mode))
	    link_field = get_link_field (p, pathname);

	  /* Print the pathname. */
	  io_fprintf (p->io,
		      "<tr><td><img src=\"%s\" alt=\"%s\" width=\"%d\" height=\"%d\"></td><td><a href=\"%s%s\">%s</a> %s</td><td>%s</td></tr>" CRLF,
		      icon, icon_alt, icon_width, icon_height,
		      name,
		      S_ISDIR (file_statbuf.st_mode) ? "/" : "",
		      name,
		      link_field,
		      size);
	}
    }

  io_fprintf (p->io,
	      "</table>" CRLF
	      "<hr>%s<br>" CRLF
	      "</body></html>" CRLF,
	      http_get_servername ());

  return close;
}

static void
choose_icon (process_rq p,
	     const char *filename, const struct stat *statbuf,
	     const char **icon, const char **icon_alt,
	     int *icon_width, int *icon_height)
{
  if (S_ISREG (statbuf->st_mode))
    {
      const char *mime_type = 0;
      const char *icon_str;
      vector v;

      /* Get the file extension and map it to a MIME type. */
      if ((v = prematch (p->pool, filename, re_ext, 0)) != 0)
	{
	  char *ext;

	  vector_get (v, 1, ext);
	  mime_type = mime_types_get_type (ext);
	}
      if (!mime_type)
	{
	  standard_icon (p, "no type",
			 icon, icon_alt, icon_width, icon_height);
	  return;
	}

      /* If there a icon specified for this MIME type? */
      icon_str = cfg_get_string (p->host, p->alias,
				 psprintf (p->pool, "icon for %s", mime_type),
				 0);
      if (!icon_str)
	{
	  /* Try looking for an icon for class / * instead. */
	  v = pstrcsplit (p->pool, mime_type, '/');
	  if (vector_size (v) >= 1)
	    {
	      const char *mime_class;

	      vector_get (v, 0, mime_class);
	      icon_str = cfg_get_string (p->host, p->alias,
					 psprintf (p->pool, "icon for %s/*",
						   mime_class), 0);
	    }
	}

      if (!icon_str)
	{
	  unknown_icon (p, icon, icon_alt, icon_width, icon_height);
	  return;
	}

      /* Split up the string. */
      if (!parse_icon_str (p, icon_str, icon, icon_alt, icon_width, icon_height))
	{
	  fprintf (stderr,
		   "cannot parse icon description: %s (mime_type = %s)\n",
		   icon_str, mime_type);
	  unknown_icon (p, icon, icon_alt, icon_width, icon_height);
	  return;
	}
    }
  else if (S_ISDIR (statbuf->st_mode))
    {
      standard_icon (p, "directory", icon, icon_alt, icon_width, icon_height);
    }
  else if (S_ISLNK (statbuf->st_mode))
    {
      standard_icon (p, "link", icon, icon_alt, icon_width, icon_height);
    }
  else
    {
      standard_icon (p, "special", icon, icon_alt, icon_width, icon_height);
    }
}

static void
standard_icon (process_rq p, const char *name,
	       const char **icon, const char **icon_alt,
	       int *icon_width, int *icon_height)
{
  const char *icon_str;

  icon_str = cfg_get_string (p->host, p->alias,
			     psprintf (p->pool, "%s icon", name), 0);
  if (!icon_str)
    {
      unknown_icon (p, icon, icon_alt, icon_width, icon_height);
      return;
    }

  if (!parse_icon_str (p, icon_str, icon, icon_alt, icon_width, icon_height))
    {
      fprintf (stderr, "cannot parse icon description: %s\n", icon_str);
      unknown_icon (p, icon, icon_alt, icon_width, icon_height);
      return;
    }
}

static void
unknown_icon (process_rq p,
	      const char **icon, const char **icon_alt,
	      int *icon_width, int *icon_height)
{
  const char *icon_str;

  icon_str = cfg_get_string (p->host, p->alias, "unknown icon", 0);
  if (!icon_str)
    {
      fprintf (stderr,
	       "``unknown icon'' must be present in configuration file\n");
      exit (1);
    }

  if (!parse_icon_str (p, icon_str, icon, icon_alt, icon_width, icon_height))
    {
      fprintf (stderr, "cannot parse icon description: %s\n", icon_str);
      exit (1);
    }
}

static int
parse_icon_str (process_rq p, const char *icon_str,
		const char **icon, const char **icon_alt,
		int *icon_width, int *icon_height)
{
  vector v;
  char *s;

  /* Split up the string. */
  if ((v = prematch (p->pool, icon_str, re_icon, 0)) == 0)
    return 0;

  if (vector_size (v) != 5) return 0;

  vector_get (v, 1, *icon);
  vector_get (v, 2, s);
  sscanf (s, "%d", icon_width);
  vector_get (v, 3, s);
  sscanf (s, "%d", icon_height);
  vector_get (v, 4, *icon_alt);

  return 1;
}

static const char *
get_printable_size (process_rq p,
		    const struct stat *statbuf)
{
  unsigned long size = statbuf->st_size;

  if (size < 1024)
    return psprintf (p->pool, "%lu bytes", size);
  else if (size < 1024 * 1024)
    return psprintf (p->pool, "%.1f KB", size / 1024.0);
  else
    return psprintf (p->pool, "%.1f MB", size / (1024 * 1024.0));
}

static const char *
get_link_field (process_rq p, const char *filename)
{
  const char prefix[] = "-&gt; ";
  const int prefix_sz = sizeof prefix - 1;
  char *buffer;
  int n;

#ifndef NAME_MAX
  /* Solaris defines NAME_MAX on a per-filesystem basis.
   * See: http://lists.spine.cx/archives/everybuddy/2002-May/001419.html
   */
  long NAME_MAX = pathconf (filename, _PC_NAME_MAX);
#endif

  buffer = pmalloc (p->pool, NAME_MAX + 1 + prefix_sz);

  memcpy (buffer, prefix, prefix_sz);

  n = readlink (filename, buffer + prefix_sz, NAME_MAX + 1);
  if (n == -1) return "";

  buffer[n + prefix_sz] = '\0';
  return buffer;
}
