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
 * $Id: rewrite.h,v 1.4 2002/10/09 19:13:14 rich Exp $
 */

#ifndef REWRITE_H
#define REWRITE_H

#include <pool.h>

#include "process_rq.h"

/* Reset the rewrite rules. */
extern void rewrite_reset_rules (void);

/* This function applies internal and external rewrite rules found
 * in the configuration file. If there is no rewrite, *location is
 * left alone and the function returns 0. If there is an external
 * rewrite, *location points to the rewritten path and the function
 * returns 1. Internal rewrites are the same as external rewrites
 * except the function returns 2.
 */
extern int apply_rewrites (const process_rq p, const char *path, const char **location);

#endif /* REWRITE_H */
