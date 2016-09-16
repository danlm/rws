/* Deliver errors back to the user.
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
 * $Id: errors.h,v 1.1 2001/03/24 17:26:29 rich Exp $
 */

#ifndef ERRORS_H
#define ERRORS_H

#include "config.h"

#include <pool.h>

#include <pthr_pseudothread.h>
#include <pthr_http.h>
#include <pthr_iolib.h>

extern int bad_request_error (process_rq p, const char *text);
extern int file_not_found_error (process_rq p);
extern int moved_permanently (process_rq p, const char *location);

#endif /* ERRORS_H */
