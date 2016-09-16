/* Rewrite rules.
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
 * $Id: rewrite.c,v 1.7 2002/10/20 13:09:10 rich Exp $
 */

#include "config.h"

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <pcre.h>

#include <pool.h>
#include <vector.h>
#include <hash.h>
#include <pstring.h>
#include <pre.h>

#include "cfg.h"
#include "process_rq.h"
#include "re.h"
#include "rewrite.h"

#define RW_DEBUG 0		/* Set this to enable debugging. */

static pool rw_pool = 0;

/* Cache of host -> struct rw *. The null host is stored with key = "". */
static shash rw_cache;

/* Pre-parsed rules. */
struct rw
{
  vector rules;			/* Vector of struct rw_rule. */
};

/* Each rule. */
struct rw_rule
{
  const char *pattern_text;
  const pcre *pattern;
  const char *sub;
  int flags;
#define RW_RULE_EXTERNAL 0x0001
#define RW_RULE_LAST     0x0002
#define RW_RULE_QSA      0x0004
};

static struct rw *parse_rules (const char *cfg);
static const char *append_qs (process_rq p, const char *path);

void
rewrite_reset_rules ()
{
  if (rw_pool) delete_pool (rw_pool);
  rw_pool = new_subpool (global_pool);

  rw_cache = new_shash (rw_pool, struct rw *);
}

int
apply_rewrites (const process_rq p, const char *path, const char **location)
{
  struct rw *rw = 0;
  const char *host = p->host_header ? p->host_header : "";
  int i, matches = 0, qsa = 0;

#if RW_DEBUG
  fprintf (stderr, "apply_rewrites: original path = %s\n",
	   p->canonical_path);
#endif

  /* Get the configuration entry. (Note the alias is not known
   * yet, so rewrite rules inside alias sections have no effect).
   */
  if (!shash_get (rw_cache, host, rw))
    {
      const char *cfg;

      cfg = cfg_get_string (p->host, 0, "rewrite", 0);
      if (cfg) rw = parse_rules (cfg);
      shash_insert (rw_cache, host, rw);
    }

  if (!rw)			/* No matching rule. */
    {
#if RW_DEBUG
      fprintf (stderr, "apply_rewrites: no matching rule\n");
#endif
      return 0;
    }

  /* Look for a matching rule. */
  for (i = 0; i < vector_size (rw->rules); ++i)
    {
      struct rw_rule rule;
      const char *old_path = path;

      vector_get (rw->rules, i, rule);

#if RW_DEBUG
      fprintf (stderr, "apply_rewrites: try matching against %s\n",
	       rule.pattern_text);
#endif

      path = presubst (p->pool, old_path, rule.pattern, rule.sub, 0);
      if (path != old_path) /* It matched. */
	{
	  matches = 1;
	  if (rule.flags & RW_RULE_QSA) qsa = 1;

#if RW_DEBUG
	  fprintf (stderr, "apply_rewrites: it matches %s\n",
		   rule.pattern_text);
#endif

	  /* External link? If so, send a redirect. External rules are
	   * always 'last'.
	   */
	  if (rule.flags & RW_RULE_EXTERNAL)
	    {
#if RW_DEBUG
	      fprintf (stderr,
		       "apply_rewrites: external: send redirect to %s\n",
		       path);
#endif
	      *location = qsa ? append_qs (p, path) : path;
	      return 1;
	    }

	  /* Last rule? */
	  if (rule.flags & RW_RULE_LAST)
	    {
#if RW_DEBUG
	      fprintf (stderr,
		       "apply_rewrites: last rule: finished with %s\n",
		       path);
#endif
	      *location = qsa ? append_qs (p, path) : path;
	      return 2;
	    }

	  /* Jump back to the beginning of the list. */
	  i = -1;
	}
    }

#if RW_DEBUG
  fprintf (stderr,
	   "apply_rewrites: finished with %s\n",
	   path);
#endif

  if (matches)
    {
      *location = qsa ? append_qs (p, path) : path;
      return 2;
    }

  return 0;
}

/* Append query string, if there is one. */
static const char *
append_qs (process_rq p, const char *path)
{
  pool pool = p->pool;
  const char *qs = http_request_query_string (p->http_request);

  if (qs && strlen (qs) > 0)
    {
      const char *t = strchr (path, '?');

      if (t)			/* Path already has a query string? */
	{
	  if (t[1] != '\0')
	    return psprintf (pool, "%s&%s", path, qs);
	  else
	    return psprintf (pool, "%s%s", path, qs);
	}
      else			/* Path doesn't have a query string. */
	return psprintf (pool, "%s?%s", path, qs);
    }
  return path;			/* No query string. */
}

static void parse_error (const char *line, const char *msg);

static struct rw *
parse_rules (const char *cfg)
{
  pool tmp = new_subpool (rw_pool);
  vector lines;
  const char *line;
  struct rw *rw;
  struct rw_rule rule;
  int i;

  /* Split up the configuration string into lines. */
  lines = pstrcsplit (tmp, cfg, '\n');

  /* Remove any empty lines (these have probably already been removed,
   * but we can safely do this again anyway).
   */
  for (i = 0; i < vector_size (lines); ++i)
    {
      vector_get (lines, i, line);

      if (strcmp (line, "") == 0)
	{
	  vector_erase (lines, i);
	  i--;
	}
    }

  if (vector_size (lines) == 0) { delete_pool (tmp); return 0; }

  /* Allocate space for the return structure. */
  rw = pmalloc (rw_pool, sizeof *rw);
  rw->rules = new_vector (rw_pool, struct rw_rule);

  /* Each line is a separate rule in the current syntax, so examine
   * each line and turn it into a rule.
   */
  for (i = 0; i < vector_size (lines); ++i)
    {
      vector v;

      vector_get (lines, i, line);

      v = pstrresplit (tmp, line, re_ws);

      if (vector_size (v) < 2 || vector_size (v) > 3)
	parse_error (line, "unrecognised format");
      vector_get (v, 0, rule.pattern_text);
      rule.pattern_text = pstrdup (rw_pool, rule.pattern_text);
      rule.pattern = precomp (rw_pool, rule.pattern_text, 0);
      vector_get (v, 1, rule.sub);
      rule.sub = pstrdup (rw_pool, rule.sub);

      /* Parse the flags. */
      rule.flags = 0;
      if (vector_size (v) == 3)
	{
	  const char *flags;
	  int j;

	  vector_get (v, 2, flags);
	  v = pstrresplit (tmp, flags, re_comma);

	  for (j = 0; j < vector_size (v); ++j)
	    {
	      const char *flag;

	      vector_get (v, j, flag);

	      if (strcasecmp (flag, "external") == 0)
		rule.flags |= RW_RULE_EXTERNAL;
	      else if (strcasecmp (flag, "last") == 0)
		rule.flags |= RW_RULE_LAST;
	      else if (strcasecmp (flag, "qsa") == 0)
		rule.flags |= RW_RULE_QSA;
	      else
		parse_error (line, "unknown flag");
	    }
	}

#if RW_DEBUG
      fprintf (stderr,
	       "parse rule: pattern=%s sub=%s flags=0x%04x\n",
	       rule.pattern_text, rule.sub, rule.flags);
#endif

      vector_push_back (rw->rules, rule);
    }

  delete_pool (tmp);
  return rw;
}

static void
parse_error (const char *line, const char *msg)
{
  fprintf (stderr,
	   "rewrite rule: %s\n"
	   "at line: %s\n",
	   msg, line);
  exit (1);
}
