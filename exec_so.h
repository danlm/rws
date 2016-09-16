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
 * $Id: exec_so.h,v 1.1 2002/08/21 13:28:31 rich Exp $
 */

#ifndef EXEC_SO_H
#define EXEC_SO_H

#include "config.h"

#include "process_rq.h"

extern void exec_so_init (void);

extern int exec_so_file (process_rq p);

#endif /* EXEC_SO_H */
