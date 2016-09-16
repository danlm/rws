/* MIME types.
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
 * $Id: mime_types.c,v 1.3 2002/10/06 11:57:22 rich Exp $
 */

#include "config.h"

#include <stdio.h>

#include <pool.h>
#include <hash.h>
#include <pstring.h>
#include <pre.h>

#include "re.h"
#include "mime_types.h"

static pool mt_pool = 0;
static sash mt_map = 0;

void
mime_types_reread_config (const char *path)
{
  FILE *fp;
  char *line = 0;
  pool tmp;
  vector v;
  int i;
  char *mt, *ext;

  if (mt_pool) delete_pool (mt_pool);
  mt_pool = new_subpool (global_pool);

  mt_map = new_sash (mt_pool);

  tmp = new_subpool (mt_pool);

  /* Read the /etc/mime.types file. */
  fp = fopen (path, "r");
  if (fp == 0) { perror (path); exit (1); }

  while ((line = pgetlinec (tmp, fp, line)) != 0)
    {
      v = pstrresplit (tmp, line, re_ws);

      switch (vector_size (v))
	{
	case 0:
	  abort ();

	case 1:
	  break;

	default:
	  vector_get (v, 0, mt);
	  for (i = 1; i < vector_size (v); ++i)
	    {
	      vector_get (v, i, ext);
	      sash_insert (mt_map, ext, mt);
	    }
	  break;
	}
    }

  fclose (fp);

  delete_pool (tmp);
}

const char *
mime_types_get_type (const char *ext)
{
  const char *mt = 0;

  sash_get (mt_map, ext, mt);
  return mt;
}
