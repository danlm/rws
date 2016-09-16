/* Various shared regular expressions.
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
 * $Id: re.h,v 1.1 2002/10/06 11:57:22 rich Exp $
 */

#ifndef RE_H
#define RE_H

#include <pcre.h>

/* Various shared regular expressions. These are actually defined
 * in main ().
 */
extern const pcre *re_alias_start,
  *re_alias_end,
  *re_begin,
  *re_conf_line,
  *re_ext,
  *re_icon,
  *re_so,
  *re_ws,
  *re_comma;

#endif /* RE_H */
