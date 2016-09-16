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
 * $Id: cfg.h,v 1.3 2001/03/24 17:26:28 rich Exp $
 */

#ifndef CFG_H
#define CFG_H

#include <hash.h>

/* Reread the configuration file. */
extern void cfg_reread_config (const char *path);

/* If there is host matching HOST, return an opaque pointer to the host's
 * configuration data.
 */
extern void *cfg_get_host (const char *host);

/* If there is an alias exactly matching PATH for host HOST_PTR, return
 * an opaque pointer to the alias's configuration data.
 */
extern void *cfg_get_alias (void *host_ptr, const char *path);

/* Return the configuration string named KEY.
 *
 * HOST_PTR and ALIAS_PTR may be optionally given to narrow the search
 * down to a particular host/alias combination.
 *
 * If the configuration string named KEY cannot be found, then DEFAULT_
 * VALUE is returned instead.
 */
extern const char *cfg_get_string (void *host_ptr, void *alias_ptr,
				   const char *key,
				   const char *default_value);

/* Similar to CFG_GET_STRING but the string is converted to an integer.
 */
extern int cfg_get_int (void *host_ptr, void *alias_ptr,
			const char *key, int default_value);

/* Similar to CFG_GET_STRING but the string is converted to a boolean.
 */
extern int cfg_get_bool (void *host_ptr, void *alias_ptr,
			 const char *key, int default_value);

#endif /* CFG_H */
