/* format.c - read/write binary format files
 *
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
 *               2010 Colin Watson <cjwatson@debian.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "gl_map.h"
#include "xalloc.h"

#include "error.h"
#include "format.h"
#include "glcontainers.h"

struct binfmt *binfmt_load (const char *name, const char *filename, int quiet)
{
    FILE *binfmt_file;
    struct binfmt *binfmt;

    binfmt_file = fopen (filename, "r");
    if (!binfmt_file)
	quit_err ("unable to open %s", filename);

    binfmt = xzalloc (sizeof *binfmt);
    binfmt->name = xstrdup (name);

#define READ_LINE(field, optional) do { \
    ssize_t len; \
    size_t n; \
    binfmt->field = NULL; \
    len = getline (&binfmt->field, &n, binfmt_file); \
    if (len == -1) { \
	if (optional) \
	    binfmt->field = xstrdup (""); \
	else if (quiet) { \
	    binfmt_free (binfmt); \
	    return NULL; \
	} else \
	    quit ("%s corrupt: out of binfmt data reading %s", \
		  filename, #field); \
    } else { \
	while (len && isspace ((int) binfmt->field[len - 1])) \
	    binfmt->field[--len] = 0; \
    } \
} while (0)

    READ_LINE (package, 0);
    READ_LINE (type, 0);
    READ_LINE (offset, 0);
    READ_LINE (magic, 0);
    READ_LINE (mask, 0);
    READ_LINE (interpreter, 0);
    READ_LINE (detector, 1);
    READ_LINE (credentials, 1);
    READ_LINE (preserve, 1);
    READ_LINE (fix_binary, 1);

#undef READ_LINE

    fclose (binfmt_file);
    return binfmt;
}

struct binfmt *binfmt_new (const char *name, gl_map_t args)
{
    struct binfmt *binfmt;

    binfmt = xzalloc (sizeof *binfmt);
    binfmt->name = xstrdup (name);

#define SET_FIELD(field) do { \
    const char *value = gl_map_get (args, #field); \
    /* value may be NULL, as update-binfmts' parser is simpler that way. */ \
    if (value && strchr (value, '\n')) \
	quit ("newlines prohibited in binfmt files (%s)", value); \
    binfmt->field = value ? xstrdup (value) : NULL; \
} while (0)

    SET_FIELD (package);
    SET_FIELD (type);
    SET_FIELD (offset);
    SET_FIELD (magic);
    SET_FIELD (mask);
    SET_FIELD (interpreter);
    SET_FIELD (detector);
    SET_FIELD (credentials);
    SET_FIELD (preserve);
    SET_FIELD (fix_binary);

#undef SET_FIELD

    if (!binfmt->type) {
	if (binfmt->magic) {
	    if (map_contains (args, "extension")) {
		warning ("%s: can't use both --magic and --extension", name);
		return NULL;
	    } else
		binfmt->type = xstrdup ("magic");
	} else {
	    if (map_contains (args, "extension"))
		binfmt->type = xstrdup ("extension");
	    else {
		warning ("%s: either --magic or --extension is required",
			 name);
		return NULL;
	    }
	}
    }

    if (!strcmp (binfmt->type, "extension")) {
	const char *extension;

	free (binfmt->magic);
	extension = gl_map_get (args, "extension");
	if (extension && strchr (extension, '\n'))
	    quit ("newlines prohibited in binfmt files (%s)", extension);
	binfmt->magic = extension ? xstrdup (extension) : NULL;
	if (binfmt->mask) {
	    warning ("%s: can't use --mask with --extension", name);
	    return NULL;
	}
	if (binfmt->offset) {
	    warning ("%s: can't use --offset with --extension", name);
	    return NULL;
	}
    }

    if (!binfmt->mask)
	binfmt->mask = xstrdup ("");
    if (!binfmt->offset || !*binfmt->offset)
	binfmt->offset = xstrdup ("0");

    return binfmt;
}

int binfmt_write (const struct binfmt *binfmt, const char *filename)
{
    FILE *binfmt_file;

    if (unlink (filename) == -1 && errno != ENOENT) {
	warning_err ("unable to ensure %s nonexistent", filename);
	return 0;
    }

    binfmt_file = fopen (filename, "w");
    if (!binfmt_file) {
	warning_err ("unable to open %s for writing", filename);
	return 0;
    }

#define WRITE_FIELD(field) \
    fprintf (binfmt_file, "%s\n", binfmt->field ? binfmt->field : "")

    WRITE_FIELD (package);
    WRITE_FIELD (type);
    WRITE_FIELD (offset);
    WRITE_FIELD (magic);
    WRITE_FIELD (mask);
    WRITE_FIELD (interpreter);
    WRITE_FIELD (detector);
    WRITE_FIELD (credentials);
    WRITE_FIELD (preserve);
    WRITE_FIELD (fix_binary);

#undef WRITE_FIELD

    if (fclose (binfmt_file)) {
	warning_err ("unable to close %s", filename);
	return 0;
    }

    return 1;
}

void binfmt_print (const struct binfmt *binfmt)
{
#define PRINT_FIELD(field) \
    printf ("%12s = %s\n", #field, binfmt->field ? binfmt->field : "")

    PRINT_FIELD (package);
    PRINT_FIELD (type);
    PRINT_FIELD (offset);
    PRINT_FIELD (magic);
    PRINT_FIELD (mask);
    PRINT_FIELD (interpreter);
    PRINT_FIELD (detector);
    PRINT_FIELD (credentials);
    PRINT_FIELD (preserve);
    PRINT_FIELD (fix_binary);

#undef PRINT_FIELD
}

int binfmt_equals (const struct binfmt *left, const struct binfmt *right)
{
    return (!strcmp (left->type, right->type) &&
	    !strcmp (left->offset, right->offset) &&
	    !strcmp (left->magic, right->magic) &&
	    !strcmp (left->mask, right->mask));
}

void binfmt_free (struct binfmt *binfmt)
{
    free (binfmt->name);
    free (binfmt->package);
    free (binfmt->type);
    free (binfmt->offset);
    free (binfmt->magic);
    free (binfmt->mask);
    free (binfmt->interpreter);
    free (binfmt->detector);
    free (binfmt->credentials);
    free (binfmt->preserve);
    free (binfmt->fix_binary);
    free (binfmt);
}
