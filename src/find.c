/* query.c - find interpreter for a given executable
 *
 * Copyright (c) 2002, 2010, 2011 Colin Watson <cjwatson@debian.org>.
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
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pipeline.h>

#include "gl_xlist.h"
#include "gl_array_list.h"
#include "xalloc.h"
#include "xvasprintf.h"

#include "error.h"
#include "format.h"
#include "glcontainers.h"
#include "paths.h"

static size_t expand_hex (char **str)
{
    size_t len;
    char *new, *p, *s;

    /* The returned string is always no longer than the original. */
    len = strlen (*str);
    p = new = xmalloc (len + 1);
    s = *str;
    while (*s) {
	if (s <= *str + len - 4 && s[0] == '\\' && s[1] == 'x') {
	    char in[3];
	    char *end;
	    long hex;

	    in[0] = s[2];
	    in[1] = s[3];
	    in[2] = '\0';
	    hex = strtol (in, &end, 16);
	    if (end == in + 2) {
		*p++ = (char) hex;
		s += 4;
		continue;
	    }
	}
	*p++ = *s++;
    }
    *p = 0;

    free (*str);
    *str = new;
    return p - new;
}

static void load_all_formats (gl_list_t *formats)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir (admindir);
    if (!dir)
	quit_err ("unable to open %s", admindir);
    *formats = gl_list_create_empty (GL_ARRAY_LIST, NULL, NULL, NULL, true);
    while ((entry = readdir (dir)) != NULL) {
	char *admindir_name, *procdir_name;
	struct stat st;
	struct binfmt *binfmt;
	size_t mask_size;

	if (!strcmp (entry->d_name, ".") || !strcmp (entry->d_name, ".."))
	    continue;
	procdir_name = xasprintf ("%s/%s", procdir, entry->d_name);
	if (stat (procdir_name, &st) == -1) {
	    free (procdir_name);
	    continue;
	}
	free (procdir_name);
	admindir_name = xasprintf ("%s/%s", admindir, entry->d_name);
	binfmt = binfmt_load (entry->d_name, admindir_name, 0);
	free (admindir_name);

	/* binfmt_load should always make these at least empty strings,
	 * never null pointers.
	 */
	assert (binfmt->type);
	assert (binfmt->magic);
	assert (binfmt->mask);
	assert (binfmt->offset);
	assert (binfmt->interpreter);
	assert (binfmt->detector);

	binfmt->magic_size = expand_hex (&binfmt->magic);
	mask_size = expand_hex (&binfmt->mask);
	if (mask_size && mask_size != binfmt->magic_size) {
	    /* A warning here would be inappropriate, as it would often be
	     * emitted for unrelated programs.
	     */
	    binfmt_free (binfmt);
	    continue;
	}
	if (!*binfmt->offset)
	    binfmt->offset = xstrdup ("0");
	gl_list_add_last (*formats, binfmt);
    }
    closedir (dir);
}

gl_list_t find_interpreters (const char *path)
{
    gl_list_t formats, ok_formats, interpreters;
    const struct binfmt *binfmt;
    size_t toread;
    char *buf;
    FILE *target_file;
    const char *dot, *extension = NULL;

    load_all_formats (&formats);

    /* Find out how much of the file we need to read.  The kernel doesn't
     * currently let this be more than 128, so we shouldn't need to worry
     * about huge memory consumption.
     */
    toread = 0;
    GL_LIST_FOREACH_START (formats, binfmt) {
	if (!strcmp (binfmt->type, "magic")) {
	    size_t size;

	    size = atoi (binfmt->offset) + binfmt->magic_size;
	    if (size > toread)
		toread = size;
	}
    } GL_LIST_FOREACH_END (formats);

    buf = xzalloc (toread);
    target_file = fopen (path, "r");
    if (!target_file)
	quit_err ("unable to open %s", path);
    if (fread (buf, 1, toread, target_file) == 0)
	/* Ignore errors; the buffer is zero-filled so attempts to match
	 * beyond the data read here will fail anyway.
	 */
	;
    fclose (target_file);

    /* Now the horrible bit.  Since there isn't a real way to plug userspace
     * detectors into the kernel (which is why this program exists in the
     * first place), we have to redo the kernel's work.  Luckily it's a
     * fairly simple job ... see linux/fs/binfmt_misc.c:check_file().
     *
     * There is a small race between the kernel performing this check and us
     * performing it.  I don't believe that this is a big deal; certainly
     * there can be no privilege elevation involved unless somebody
     * deliberately makes a set-id binary a binfmt handler, in which case
     * "don't do that, then".
     */
    dot = strrchr (path, '.');
    if (dot)
	extension = dot + 1;

    ok_formats = gl_list_create_empty (GL_ARRAY_LIST, NULL, NULL, NULL, true);
    GL_LIST_FOREACH_START (formats, binfmt) {
	if (!strcmp (binfmt->type, "magic")) {
	    char *segment;

	    segment = xmalloc (binfmt->magic_size);
	    memcpy (segment, buf + atoi (binfmt->offset), binfmt->magic_size);
	    if (*binfmt->mask)
		for (size_t i = 0; i < toread && i < binfmt->magic_size; ++i)
		    segment[i] &= binfmt->mask[i];
	    if (!memcmp (segment, binfmt->magic, binfmt->magic_size))
		gl_list_add_last (ok_formats, binfmt);
	    free (segment);
	} else {
	    if (extension && !strcmp (extension, binfmt->magic))
		gl_list_add_last (ok_formats, binfmt);
	}
    } GL_LIST_FOREACH_END (formats);

    /* Everything in ok_formats is now a candidate.  Loop through twice,
     * once to try everything with a detector and once to try everything
     * without.
     */
    interpreters = gl_list_create_empty (GL_ARRAY_LIST, NULL, NULL, NULL,
					 true);
    GL_LIST_FOREACH_START (ok_formats, binfmt) {
	if (*binfmt->detector) {
	    pipeline *detector;

	    detector = pipeline_new_command_args (binfmt->detector,
						  path, (void *) 0);
	    if (pipeline_run (detector) == 0)
		gl_list_add_last (interpreters, binfmt);
	}
    } GL_LIST_FOREACH_END (ok_formats);

    GL_LIST_FOREACH_START (ok_formats, binfmt) {
	if (!*binfmt->detector)
	    gl_list_add_last (interpreters, binfmt);
    } GL_LIST_FOREACH_END (ok_formats);

    return interpreters;
}
