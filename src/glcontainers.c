/*
 * glcontainers.c: common Gnulib container helpers
 *
 * Copyright (C) 2019 Colin Watson.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with man-db; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "gl_xmap.h"
#include "gl_xomap.h"
#include "hash-pjw-bare.h"

#include "glcontainers.h"

bool string_equals (const void *s1, const void *s2)
{
    return strcmp ((const char *) s1, (const char *) s2) == 0;
}

size_t string_hash (const void *s)
{
    return hash_pjw_bare (s, strlen ((const char *) s));
}

void plain_free (const void *s)
{
    /* The Gnulib container typedefs declare the argument as const, but
     * there doesn't seem to be a good reason for this.
     */
    free ((void *) s);
}

gl_map_t new_string_map (gl_map_implementation_t implementation,
			 gl_mapvalue_dispose_fn vdispose_fn)
{
    return gl_map_create_empty (implementation, string_equals, string_hash,
				plain_free, vdispose_fn);
}

gl_omap_t new_string_omap (gl_omap_implementation_t implementation,
			   gl_mapvalue_dispose_fn vdispose_fn)
{
    return gl_omap_create_empty (implementation, (gl_mapkey_compar_fn) strcmp,
				 plain_free, vdispose_fn);
}

/* Work around inconvenient APIs in Gnulib:
 *   https://lists.gnu.org/archive/html/bug-gnulib/2019-02/msg00013.html
 */

bool map_contains (gl_map_t map, const void *key)
{
    const void *value;
    return gl_map_search (map, key, &value);
}

bool omap_contains (gl_omap_t omap, const void *key)
{
    const void *value;
    return gl_omap_search (omap, key, &value);
}
