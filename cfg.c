/* Configuration file parsing.
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
 * $Id: cfg.c,v 1.11 2002/10/15 21:28:32 rich Exp $
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <pool.h>
#include <pstring.h>
#include <hash.h>
#include <vector.h>
#include <pre.h>

#include "re.h"
#include "cfg.h"

struct config_data
{
  sash data;

  /* Aliases -- these are only present in the host-specific configuration
   * files, not in CFG_MAIN.
   */
  shash aliases;		/* Hash of string -> struct alias_data * */
};

struct alias_data
{
  sash data;
};

static pool cfg_pool = 0;
static shash cfg_hosts;		/* Hash of string -> struct config_data * */
static struct config_data *cfg_main;

static struct config_data *read_config (FILE *fp, int is_main, const char *filename);
static void config_err (const char *filename, const char *line, const char *msg);

/* The PATH argument will point to the base for configuration
 * files, eg. "/etc/rws". We append "/rws.conf" to get the main
 * configuration file and "/hosts/" to get the virtual hosts
 * directory.
 */
void
cfg_reread_config (const char *path)
{
  const char *config_file;
  const char *hosts_dir;
  FILE *fp;
  DIR *dir;
  struct dirent *d;
  pool tmp;

  /* Show the message about reloading the configuration file, but only
   * the second and subsequent times this function is run.
   */
  if (cfg_pool)
    fprintf (stderr, "reloading configuration file ...\n");

  /* Remove any configuration data from previous configuration run. */
  if (cfg_pool) delete_pool (cfg_pool);

  /* Create new data structures. */
  cfg_pool = new_subpool (global_pool);
  tmp = new_subpool (cfg_pool);
  cfg_hosts = new_shash (cfg_pool, struct config_data *);

  config_file = psprintf (tmp, "%s/rws.conf", path);
  hosts_dir = psprintf (tmp, "%s/hosts/", path);

  /* Read in main configuration file. */
  fp = fopen (config_file, "r");
  if (fp == 0) { perror (config_file); exit (1); }
  cfg_main = read_config (fp, 1, config_file);
  fclose (fp);

  /* Read in each virtual host configuration file. */
  dir = opendir (hosts_dir);
  if (dir)
    {
      while ((d = readdir (dir)) != 0)
	{
	  struct config_data *c;

	  if (d->d_name[0] != '.') /* Ignore ".", ".." and dotfiles. */
	    {
	      const char *p;

	      p = psprintf (tmp, "%s/%s", hosts_dir, d->d_name);

	      fp = fopen (p, "r");
	      if (fp == 0) { perror (p); exit (1); }
	      c = read_config (fp, 0, p);
	      fclose (fp);

	      shash_insert (cfg_hosts, d->d_name, c);
	    }
	}

      closedir (dir);
    }

  delete_pool (tmp);
}

/* Read in a config file from FP. */
static struct config_data *
read_config (FILE *fp, int is_main, const char *filename)
{
  pool tmp = new_subpool (cfg_pool);
  char *line = 0;
  struct config_data *c;
  struct alias_data *a = 0;

  c = pmalloc (cfg_pool, sizeof *c);
  c->data = new_sash (cfg_pool);
  if (!is_main) c->aliases = new_shash (cfg_pool, struct alias_data *);
  else c->aliases = 0;

  while ((line = pgetlinec (tmp, fp, line)))
    {
      vector v;

      if (!is_main && (v = prematch (tmp, line, re_alias_start, 0)))
	{
	  const char *aliasname;

	  if (a) config_err (filename, line, "nested alias");

	  vector_get (v, 1, aliasname);

	  a = pmalloc (cfg_pool, sizeof *a);
	  a->data = new_sash (cfg_pool);

	  if (shash_insert (c->aliases, aliasname, a))
	    config_err (filename, line, "duplicate alias");
	}
      else if (!is_main && prematch (tmp, line, re_alias_end, 0))
	{
	  if (!a)
	    config_err (filename, line,
		       "end alias found, but not inside an alias definition");

	  a = 0;
	}
      else if ((v = prematch (tmp, line, re_begin, 0)))
	{
	  const char *key;
	  char *value, *end_line;
	  sash s;

	  vector_get (v, 1, key);

	  /* Read the data lines until we get to 'end key' line. */
	  value = pstrdup (tmp, "");
	  end_line = psprintf (tmp, "end %s", key);
	  while ((line = pgetlinec (tmp, fp, line)))
	    {
	      if (strcmp (line, end_line) == 0)
		break;

	      value = pstrcat (tmp, value, line);
	      value = pstrcat (tmp, value, "\n");
	    }

	  if (!line)
	    config_err (filename, "EOF", "missing end <key> line");

	  if (a) s = a->data;
	  else s = c->data;

	  if (sash_insert (s, key, value))
	    config_err (filename, line, "duplicate definition");
	}
      else if ((v = prematch (tmp, line, re_conf_line, 0)))
	{
	  const char *key, *value;
	  sash s;

	  vector_get (v, 1, key);
	  vector_get (v, 2, value);

	  /* 'key:' means define key as the empty string (as opposed to
	   * commenting it out which leaves 'key' undefined).
	   */
	  if (value == 0) value = "";

	  if (a) s = a->data;
	  else s = c->data;

	  if (sash_insert (s, key, value))
	    config_err (filename, line, "duplicate definition");
	}
      else
	config_err (filename, line, "unexpected line");
    }

  delete_pool (tmp);

  return c;
}

static void
config_err (const char *filename, const char *line, const char *msg)
{
  fprintf (stderr,
	   "rws: %s: %s\n"
	   "rws: near ``%s''\n",
	   filename, msg,
	   line);
  exit (1);
}

void *
cfg_get_host (const char *host)
{
  struct config_data *c = 0;

  shash_get (cfg_hosts, host, c);
  return c;
}

void *
cfg_get_alias (void *host_ptr, const char *path)
{
  struct config_data *c = (struct config_data *) host_ptr;
  struct alias_data *a = 0;

  shash_get (c->aliases, path, a);
  return a;
}

const char *
cfg_get_string (void *host_ptr, void *alias_ptr,
		const char *key, const char *default_value)
{
  struct config_data *c = (struct config_data *) host_ptr;
  struct alias_data *a = (struct alias_data *) alias_ptr;
  const char *value;

  if (a && sash_get (a->data, key, value))
    return value;
  if (c && sash_get (c->data, key, value))
    return value;
  if (sash_get (cfg_main->data, key, value))
    return value;

  return default_value;
}

int
cfg_get_int (void *host_ptr, void *alias_ptr, const char *key, int default_value)
{
  const char *value = cfg_get_string (host_ptr, alias_ptr, key, 0);
  int r;

  if (!value) return default_value;

  if (sscanf (value, "%d", &r) != 1) return default_value;

  return r;
}

int
cfg_get_bool (void *host_ptr, void *alias_ptr, const char *key, int default_value)
{
  const char *value = cfg_get_string (host_ptr, alias_ptr, key, 0);

  if (!value) return default_value;

  if (value[0] == '0' ||
      value[0] == 'f' || value[0] == 'F' ||
      value[0] == 'n' || value[0] == 'N' ||
      (value[0] == 'o' && value[1] == 'f') ||
      (value[0] == 'O' && value[1] == 'F'))
    return 0;
  else if (value[0] == '1' ||
      value[0] == 't' || value[0] == 'T' ||
      value[0] == 'y' || value[0] == 'Y' ||
      (value[0] == 'o' && value[1] == 'n') ||
      (value[0] == 'O' && value[1] == 'N'))
    return 1;
  else
    return default_value;
}
