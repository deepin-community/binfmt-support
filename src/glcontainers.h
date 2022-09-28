/*
 * glcontainers.h: interface to common Gnulib container helpers
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

#ifndef BINFMT_SUPPORT_GLCONTAINERS_H
#define BINFMT_SUPPORT_GLCONTAINERS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>

#include "gl_list.h"
#include "gl_map.h"
#include "gl_omap.h"

/* These types are compatible with those required by Gnulib container
 * initialisation.
 */

bool string_equals (const void *s1, const void *s2);
size_t string_hash (const void *s);
void plain_free (const void *s);

/* Convenience functions. */

gl_map_t new_string_map (gl_map_implementation_t implementation,
			 gl_mapvalue_dispose_fn vdispose_fn);
gl_omap_t new_string_omap (gl_omap_implementation_t implementation,
			   gl_mapvalue_dispose_fn vdispose_fn);

bool map_contains (gl_map_t map, const void *key);
bool omap_contains (gl_omap_t map, const void *key);

/* Iterator macros. */

#define GL_LIST_FOREACH_START(list, item) \
    do { \
	gl_list_iterator_t list##_iter = gl_list_iterator (list); \
	while (gl_list_iterator_next (&list##_iter, \
				      (const void **) &item, NULL))

#define GL_LIST_FOREACH_END(list) \
	gl_list_iterator_free (&list##_iter); \
    } while (0)

#define GL_OMAP_FOREACH_START(omap, key, value) \
    do { \
	gl_omap_iterator_t omap##_iter = gl_omap_iterator (omap); \
	while (gl_omap_iterator_next (&omap##_iter, \
				      (const void **) &key, \
				      (const void **) &value))

#define GL_OMAP_FOREACH_END(omap) \
	gl_omap_iterator_free (&omap##_iter); \
    } while (0)

#endif /* BINFMT_SUPPORT_GLCONTAINERS_H */
