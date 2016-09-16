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
 * $Id: dir.h,v 1.3 2001/03/24 17:26:28 rich Exp $
 */

#ifndef DIR_H
#define DIR_H

#include "config.h"

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <pool.h>

#include <pthr_pseudothread.h>
#include <pthr_http.h>
#include <pthr_iolib.h>

#include "process_rq.h"

extern int dir_serve (process_rq p);

#endif /* DIR_H */
