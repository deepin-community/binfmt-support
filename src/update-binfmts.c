/* update-binfmts.c - maintain registry of executable binary formats
 *
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
 *               2010, 2011 Colin Watson <cjwatson@debian.org>.
 * See update-binfmts(8) for documentation.
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
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __linux__
# include <sys/vfs.h>

# ifndef FUSE_SUPER_MAGIC
#  define FUSE_SUPER_MAGIC 0x65735546
# endif /* FUSE_SUPER_MAGIC */
#endif /* __linux__ */

#include <pipeline.h>

#include "argp.h"
#include "gl_hash_map.h"
#include "gl_list.h"
#include "gl_rbtree_omap.h"
#include "gl_xmap.h"
#include "gl_xomap.h"
#include "xalloc.h"
#include "xstdopen.h"
#include "xvasprintf.h"

#include "error.h"
#include "find.h"
#include "format.h"
#include "glcontainers.h"
#include "paths.h"

char *program_name;

static int test = 0;

static char *path_register, *path_status;
static char *run_detectors;

gl_omap_t formats;

static inline bool exists (const char *name)
{
    struct stat st;

    return stat (name, &st) != -1;
}

static inline bool is_file (const char *name)
{
    struct stat st;

    return stat (name, &st) != -1 && S_ISREG (st.st_mode);
}

static inline bool is_directory (const char *name)
{
    struct stat st;

    return stat (name, &st) != -1 && S_ISDIR (st.st_mode);
}

static void check_supported_os (void)
{
    struct utsname uts;

    uname (&uts);
    if (!strcmp (uts.sysname, "Linux"))
	return;
    if (isatty (STDERR_FILENO)) {
	fprintf (stderr, "Sorry, update-binfmts currently only works on "
			 "Linux.\n");
	if (!strcmp (uts.sysname, "GNU"))
	    fprintf (stderr, "Patches for Hurd support are welcomed; they "
			     "should not be difficult.\n");
    }
    exit (0);
}

static int rename_mv (const char *source, const char *dest)
{
    pipeline *mv;

    if (!rename (source, dest))
	return 1;

    mv = pipeline_new_command_args ("mv", source, dest, (void *) 0);
    return pipeline_run (mv) == 0;
}

static gl_map_t get_import (const char *name)
{
    FILE *import_file;
    gl_map_t import;
    char *line = NULL;
    ssize_t len;
    size_t n;

    import_file = fopen (name, "r");
    if (!import_file) {
	warning_err ("unable to open %s", name);
	return NULL;
    }
    import = new_string_map (GL_HASH_MAP, plain_free);

    while ((len = getline (&line, &n, import_file)) != -1) {
	char *space, *key, *value, *p, *q;

	while (len && isspace ((int) line[len - 1]))
	    line[--len] = 0;

	space = strchr (line, ' ');
	if (space) {
	    *space = 0;
	    value = xstrdup (space + 1);
	} else
	    value = NULL;

	p = key = xmalloc (strlen (line) + 1);
	q = line;
	while (*q)
	    *p++ = tolower ((int) *q++);
	*p = 0;

	/* Steals memory for key and value. */
	gl_map_put (import, key, value);
	free (line);
	line = NULL;
    }

    fclose (import_file);
    return import;
}

/* Check whether binfmt_misc is enabled.
 *
 * This lets us avoids mounting /proc/sys/fs/binfmt_misc when update-binfmts
 * --import is run in a chroot, which in turn avoids possible busy-mount
 * problems when cleaning up the chroot.
 *
 * This isn't an entirely reliable test: in particular, if systemd is built
 * with ENABLE_BINFMT, it sets up /proc/sys/fs/binfmt_misc to be
 * automounted, so just checking for the existence of
 * /proc/sys/fs/binfmt_misc/register will cause it to be mounted.  However,
 * if systemd is running then you're almost certainly in an environment that
 * knows how to clean up the mount again, so we can probably afford to
 * ignore this.
 */
static int is_enabled (void)
{
    return is_file (path_register);
}

/* Loading and unloading logic, which should cope with the various ways this
 * has been implemented.
 */

static int load_binfmt_misc (void)
{
    if (test) {
	printf ("load binfmt_misc\n");
	return 1;
    }

    if (!is_enabled ()) {
	pipeline *mount = pipeline_new_command_args
	    ("/bin/mount", "-t", "binfmt_misc", "-o", "nodev,noexec,nosuid",
	     "binfmt_misc", procdir, (void *) 0);

	if (pipeline_run (mount)) {
	    warning ("Couldn't mount the binfmt_misc filesystem on %s.",
		     procdir);
	    return 0;
	}
    }

    if (is_enabled ()) {
	FILE *status_file = fopen (path_status, "w");
	if (status_file) {
	    fprintf (status_file, "1\n");
	    fclose (status_file);
	} else
	    warning_err ("unable to open %s for writing", path_status);
	return 1;
    } else {
	warning ("binfmt_misc initialised, but %s missing!  Giving up.",
		 path_register);
	return 0;
    }
}

static int unload_binfmt_misc (void)
{
#ifdef __linux__
    struct statfs stfs;
#endif /* __linux__ */
    pipeline *umount;

    if (test) {
	printf ("unload binfmt_misc\n");
	return 1;
    }

#ifdef __linux__
    if (statfs (procdir, &stfs) == 0 && stfs.f_type == FUSE_SUPER_MAGIC) {
        /* Probably running within test suite.  Try fusermount -u. */
        umount = pipeline_new_command_args
            ("fusermount", "-u", procdir, NULL);
        if (!pipeline_run (umount))
            return 1;
    }
#endif /* __linux__ */

    umount = pipeline_new_command_args ("/bin/umount", procdir, (void *) 0);
    if (pipeline_run (umount)) {
        warning ("Couldn't unmount the binfmt_misc filesystem from %s.",
                 procdir);
        return 0;
    }

    return 1;
}

/* Reading the administrative directory. */

static void load_format (const char *name, int quiet)
{
    char *admindir_name = xasprintf ("%s/%s", admindir, name);
    if (is_file (admindir_name)) {
	struct binfmt *format = binfmt_load (name, admindir_name, quiet);
	if (format && !omap_contains (formats, name))
	    gl_omap_put (formats, xstrdup (name), format);
    }
    free (admindir_name);
}

static void load_all_formats (int quiet)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir (admindir);
    if (!dir)
	quit_err ("unable to open %s", admindir);
    while ((entry = readdir (dir)) != NULL) {
	if (!strcmp (entry->d_name, ".") || !strcmp (entry->d_name, ".."))
	    continue;
	load_format (entry->d_name, quiet);
    }
    closedir (dir);
}

/* Actions. */

/* Enable a binary format in the kernel. */
static int act_enable (const char *name)
{
    char *procdir_name;
    const struct binfmt *binfmt;

    if (!load_binfmt_misc ())
	return 1;

    if (name) {
	char type;
	int need_detector;
	const char *interpreter;
	const char *credentials;
	const char *preserve;
	const char *fix_binary;
	char *regstring;

	procdir_name = xasprintf ("%s/%s", procdir, name);
	if (exists (procdir_name)) {
	    /* This can happen in chroots, since /proc/sys/fs/binfmt_misc is
	     * shared between all chroots.  The real fix for this is to give
	     * the binfmt_misc filesystem per-chroot (or per-mount) state,
	     * since at the moment there will be leakage of enabled binary
	     * formats between chroots.  However, for now, we just
	     * gracefully bail out if the format is already enabled.
	     */
	    warning ("%s already enabled in kernel.", name);
	    free (procdir_name);
	    return 1;
	}
	free (procdir_name);

	load_format (name, 0);
	binfmt = gl_omap_get (formats, name);
	if (!test && !binfmt) {
	    warning ("%s not in database of installed binary formats.", name);
	    return 0;
	}
	type = (!strcmp (binfmt->type, "magic")) ? 'M' : 'E';

	need_detector = binfmt->detector && *binfmt->detector;
	if (!need_detector) {
	    const char *other_name;
	    const struct binfmt *other_binfmt;

	    /* Scan the format database to see if anything else uses the
	     * same spec as us. If so, assume that we need a detector,
	     * effectively /bin/true. Don't actually set binfmt->detector
	     * though, since run-detectors optimizes the case of empty
	     * detectors and "runs" them last.
	     */
	    load_all_formats (1);

	    GL_OMAP_FOREACH_START (formats, other_name, other_binfmt) {
		if (!strcmp (name, other_name))
		    continue;
		if (binfmt_equals (binfmt, other_binfmt)) {
		    need_detector = 1;
		    break;
		}
	    } GL_OMAP_FOREACH_END (formats);
	}
	/* Fake the interpreter if we need a userspace detector program. */
	interpreter = need_detector ? run_detectors : binfmt->interpreter;

	credentials =
		(binfmt->credentials && !strcmp (binfmt->credentials, "yes"))
		? "C" : "";
	preserve = (binfmt->preserve && !strcmp (binfmt->preserve, "yes"))
		? "P" : "";
	fix_binary =
		(binfmt->fix_binary && !strcmp (binfmt->fix_binary, "yes"))
		? "F" : "";
	if (need_detector && *fix_binary) {
	    warning_err ("unable to enable binary format %s: another format "
			 "with the same magic already exists and this is a "
			 "fix-binary format", name);
	    return 0;
	}
	regstring = xasprintf (":%s:%c:%s:%s:%s:%s:%s%s%s\n",
			       name, type, binfmt->offset, binfmt->magic,
			       binfmt->mask, interpreter,
			       credentials, preserve, fix_binary);
	if (test)
	    printf ("enable %s with the following format string:\n %s",
		    name, regstring);
	else {
	    FILE *register_file;

	    register_file = fopen (path_register, "w");
	    if (!register_file) {
		warning_err ("unable to open %s for writing",
			     path_register);
		return 0;
	    }
	    fputs (regstring, register_file);
	    if (fclose (register_file)) {
		warning_err ("unable to close %s", path_register);
		return 0;
	    }
	}
	return 1;
    } else {
	int worked = 1;

	load_all_formats (0);
	GL_OMAP_FOREACH_START (formats, name, binfmt) {
	    procdir_name = xasprintf ("%s/%s", procdir, name);
	    if (!exists (procdir_name))
		worked &= act_enable (name);
	    free (procdir_name);
	} GL_OMAP_FOREACH_END (formats);
	return worked;
    }
}

/* Disable a binary format in the kernel. */
static int act_disable (const char *name)
{
    if (!is_directory (procdir))
	return 1; /* We're disabling anyway, so we don't mind */
    if (name) {
	char *procdir_name;

	procdir_name = xasprintf ("%s/%s", procdir, name);
	if (!exists (procdir_name)) {
	    /* Don't warn in this circumstance, as it could happen e.g. when
	     * binfmt-support and a package depending on it are upgraded at
	     * the same time, so we get called when stopped.  Just pretend
	     * that the disable operation succeeded.
	     */
	    free (procdir_name);
	    return 1;
	}

	/* We used to check the entry in procdir to make sure we were
	 * removing an entry with the same interpreter, but this is bad; it
	 * makes things really difficult for packages that want to change
	 * their interpreter, for instance.  Now we unconditionally remove
	 * and rely on the calling logic to check that the entry in admindir
	 * belongs to the same package.
	 *
	 * In other words, admindir becomes the canonical reference, not
	 * procdir.  This is in line with similar update-* tools in Debian.
	 */

	if (test)
	    printf ("disable %s\n", name);
	else {
	    FILE *procentry_file;

	    procentry_file = fopen (procdir_name, "w");
	    if (!procentry_file) {
		warning_err ("unable to open %s for writing", procdir_name);
		free (procdir_name);
		return 0;
	    }
	    fputs ("-1", procentry_file);
	    if (fclose (procentry_file)) {
		warning_err ("unable to close %s", procdir_name);
		free (procdir_name);
		return 0;
	    }
	    if (exists (procdir_name)) {
		warning ("removal of %s ignored by kernel!", procdir_name);
		return 0;
	    }
	}
	return 1;
    } else {
	int worked = 1;
	const struct binfmt *binfmt;

	load_all_formats (0);
	GL_OMAP_FOREACH_START (formats, name, binfmt) {
	    char *procdir_id = xasprintf ("%s/%s", procdir, name);
	    if (exists (procdir_id))
		worked &= act_disable (name);
	    free (procdir_id);
	} GL_OMAP_FOREACH_END (formats);
	unload_binfmt_misc (); /* ignore errors here */
	return worked;
    }
}

static int act_install (const char *name, const struct binfmt *binfmt,
			int always_update_kernel)
{
    const struct binfmt *old_binfmt;
    char *admindir_name;

    load_format (name, 1);
    old_binfmt = gl_omap_get (formats, name);
    if (old_binfmt) {
	/* For now we just silently zap any old versions with the same
	 * package name (has to be silent or upgrades are annoying).  Maybe
	 * we should be more careful in the future.
	 */
	const char *package, *old_package;

	package = binfmt->package;
	old_package = old_binfmt->package;
	if (strcmp (package, old_package)) {
	    if (!strcmp (package, ":"))
		package = "<local>";
	    if (!strcmp (old_package, ":"))
		old_package = "<local>";
	    warning ("current package is %s, but binary format already "
		     "installed by %s", package, old_package);
	    return 0;
	}
    }
    /* Separate test just in case the administrative file exists but is
     * corrupt.
     */
    admindir_name = xasprintf ("%s/%s", admindir, name);
    if (is_file (admindir_name)) {
	if (always_update_kernel || is_enabled ()) {
	    if (!act_disable (name)) {
		warning ("unable to disable binary format %s", name);
		free (admindir_name);
		return 0;
	    }
	}
    }

    if (test) {
	printf ("install the following binary format description:\n");
	binfmt_print (binfmt);
    } else {
	char *admindir_name_tmp = xasprintf ("%s.tmp", admindir_name);
	if (!binfmt_write (binfmt, admindir_name_tmp)) {
	    free (admindir_name_tmp);
	    free (admindir_name);
	    return 0;
	}
	if (!rename_mv (admindir_name_tmp, admindir_name)) {
	    warning_err ("unable to install %s as %s",
			 admindir_name_tmp, admindir_name);
	    free (admindir_name_tmp);
	    free (admindir_name);
	    return 0;
	}
	free (admindir_name_tmp);
    }
    free (admindir_name);
    gl_omap_put (formats, xstrdup (name), binfmt);
    if (always_update_kernel || is_enabled ()) {
	if (!act_enable (name)) {
	    warning ("unable to enable binary format %s", name);
	    return 0;
	}
    }
    return 1;
}

/* Remove a binary format from binfmt-support's database.  Attempt to
 * disable the format in the kernel first.
 */
static int act_remove (const char *name, const char *package,
		       int always_update_kernel)
{
    char *admindir_name;
    const struct binfmt *old_binfmt;

    admindir_name = xasprintf ("%s/%s", admindir, name);
    if (!is_file (admindir_name)) {
	/* There may be a --force option in the future to allow entries like
	 * this to be removed; either they were created manually or
	 * update-binfmts was broken.
	 */
	warning ("%s does not exist; nothing to do!", admindir_name);
	free (admindir_name);
	return 1;
    }
    load_format (name, 1);
    old_binfmt = gl_omap_get (formats, name);
    if (old_binfmt) {
	const char *old_package = old_binfmt->package;
	if (strcmp (package, old_package)) {
	    if (!strcmp (package, ":"))
		package = "<local>";
	    if (!strcmp (old_package, ":"))
		old_package = "<local>";
	    warning ("current package is %s, but binary format already "
		     "installed by %s; not removing.", package, old_package);
	    free (admindir_name);
	    /* I don't think this should be fatal. */
	    return 1;
	}
    }
    if (always_update_kernel || is_enabled ()) {
	if (!act_disable (name)) {
	    warning ("unable to disable binary format %s", name);
	    free (admindir_name);
	    return 0;
	}
    }
    if (test)
	printf ("remove %s", admindir_name);
    else {
	if (unlink (admindir_name) == -1) {
	    warning_err ("unable to remove %s", admindir_name);
	    free (admindir_name);
	    return 0;
	}
	gl_omap_remove (formats, name);
    }
    free (admindir_name);
    return 1;
}

/* Import a new format file into binfmt-support's database.  This is
 * intended for use by packaging systems.
 */
static int act_import (const char *name)
{
    if (name) {
	const char *slash, *id;
	char *path;
	gl_map_t import;
	const struct binfmt *binfmt;
	const char *interpreter;

	slash = strrchr (name, '/');
	if (slash) {
	    id = slash + 1;
	    path = xstrdup (name);
	} else {
	    id = name;
	    path = xasprintf ("%s/%s", importdir, name);
	}

	if (!strcmp (id, ".") || !strcmp (id, "..") ||
	    !strcmp (id, "register") || !strcmp (id, "status")) {
	    warning ("binary format name '%s' is reserved", id);
	    free (path);
	    return 0;
	}

	import = get_import (path);
	if (!import || !gl_map_size (import)) {
	    warning ("couldn't find information about '%s' to import", id);
	    free (path);
	    return 0;
	}

	load_format (id, 1);
	binfmt = gl_omap_get (formats, id);
	if (binfmt) {
	    if (!strcmp (binfmt->package, ":")) {
		/* Installed version was installed manually, so don't import
		 * over it.
		 */
		warning ("preserving local changes to %s", id);
		free (path);
		return 1;
	    } else {
		/* Installed version was installed by a package, so it
		 * should be OK to replace it.
		 */
	    }
	}

	/* TODO: This duplicates the verification code below slightly. */
	if (!map_contains (import, "package")) {
	    warning ("%s: required 'package' line missing", path);
	    free (path);
	    return 0;
	}

	interpreter = gl_map_get (import, "interpreter");
	if (!interpreter || access (interpreter, X_OK))
	    warning ("%s: no executable %s found, but continuing anyway as "
		     "you request", path, interpreter);

	act_install (id, binfmt_new (path, import), 0);
	gl_map_free (import);
	free (path);
	return 1;
    } else {
	DIR *dir;
	struct dirent *entry;
	int worked;

	dir = opendir (importdir);
	if (!dir) {
	    warning_err ("unable to open %s", importdir);
	    return 0;
	}
	worked = 1;
	while ((entry = readdir (dir)) != NULL) {
	    char *importdir_name;
	    if (!strcmp (entry->d_name, ".") || !strcmp (entry->d_name, ".."))
		continue;
	    importdir_name = xasprintf ("%s/%s", importdir, entry->d_name);
	    if (is_file (importdir_name))
		worked &= act_import (entry->d_name);
	    free (importdir_name);
	}
	closedir (dir);
	return worked;
    }
}

/* Unimport an old format file that was previously imported into
 * binfmt-support's database.  This is intended for use by packaging
 * systems.
 */
static int act_unimport (const char *name)
{
    if (name) {
	const char *slash, *id;
	char *path;
	gl_map_t import;
	const struct binfmt *binfmt;
	const char *package;

	slash = strrchr (name, '/');
	if (slash) {
	    id = slash + 1;
	    path = xstrdup (name);
	} else {
	    id = name;
	    path = xasprintf ("%s/%s", importdir, name);
	}

	if (!strcmp (id, ".") || !strcmp (id, "..") ||
	    !strcmp (id, "register") || !strcmp (id, "status")) {
	    warning ("binary format name '%s' is reserved", id);
	    free (path);
	    return 0;
	}

	import = get_import (path);
	if (!import || !gl_map_size (import)) {
	    warning ("couldn't find information about '%s' to unimport", id);
	    free (path);
	    return 0;
	}

	load_format (id, 1);
	binfmt = gl_omap_get (formats, id);
	if (binfmt) {
	    if (!strcmp (binfmt->package, ":")) {
		/* Installed version was installed manually, so don't
		 * unimport it.
		 */
		warning ("preserving local changes to %s", id);
		free (path);
		return 1;
	    }
	} else {
	    /* Nothing to do. */
	    free (path);
	    return 1;
	}

	package = gl_map_get (import, "package");
	if (!package) {
	    warning ("%s: required 'package' line missing", path);
	    free (path);
	    return 0;
	}

	act_remove (id, package, 0);
	gl_map_free (import);
	free (path);
	return 1;
    } else {
	DIR *dir;
	struct dirent *entry;
	int worked;

	dir = opendir (importdir);
	if (!dir) {
	    warning_err ("unable to open %s", importdir);
	    return 0;
	}
	worked = 1;
	while ((entry = readdir (dir)) != NULL) {
	    char *importdir_name;
	    if (!strcmp (entry->d_name, ".") || !strcmp (entry->d_name, ".."))
		continue;
	    importdir_name = xasprintf ("%s/%s", importdir, entry->d_name);
	    if (is_file (importdir_name))
		worked &= act_unimport (entry->d_name);
	    free (importdir_name);
	}
	closedir (dir);
	return worked;
    }
}

static int act_display (const char *name)
{
    const struct binfmt *binfmt;

    if (name) {
	char *procdir_name;
	const char *package;

	procdir_name = xasprintf ("%s/%s", procdir, name);
	load_format (name, 0);
	binfmt = gl_omap_get (formats, name);
	if (!binfmt) {
	    warning ("%s not in database of installed binary formats.", name);
	    return 0;
	}
	printf ("%s (%s):\n",
		name, exists (procdir_name) ? "enabled" : "disabled");
	package = (!strcmp (binfmt->package, ":"))
		  ? "<local>" : binfmt->package;
	printf ("\
     package = %s\n\
        type = %s\n\
      offset = %s\n\
       magic = %s\n\
        mask = %s\n\
 interpreter = %s\n\
    detector = %s\n",
	    package, binfmt->type, binfmt->offset, binfmt->magic, binfmt->mask,
	    binfmt->interpreter, binfmt->detector);
    } else {
	load_all_formats (0);
	GL_OMAP_FOREACH_START (formats, name, binfmt)
	    act_display (name);
	GL_OMAP_FOREACH_END (formats);
    }
    return 1;
}

static int act_find (const char *executable)
{
    gl_list_t interpreters;
    const struct binfmt *binfmt;

    interpreters = find_interpreters (executable);

    GL_LIST_FOREACH_START (interpreters, binfmt)
	printf ("%s\n", binfmt->interpreter);
    GL_LIST_FOREACH_END (interpreters);

    gl_list_free (interpreters);

    return 1;
}

const char *argp_program_version = "binfmt-support " PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

enum opts {
    OPT_INSTALL = 256,
    OPT_REMOVE,
    OPT_IMPORT,
    OPT_UNIMPORT,
    OPT_DISPLAY,
    OPT_ENABLE,
    OPT_DISABLE,
    OPT_FIND,
    OPT_MAGIC,
    OPT_MASK,
    OPT_OFFSET,
    OPT_EXTENSION,
    OPT_DETECTOR,
    OPT_CREDENTIALS,
    OPT_PRESERVE,
    OPT_FIX_BINARY,
    OPT_PACKAGE,
    OPT_ADMINDIR,
    OPT_IMPORTDIR,
    OPT_PROCDIR,
    OPT_TEST
};

static struct argp_option options[] = {
    { "install",	OPT_INSTALL,	0,		OPTION_HIDDEN,
	"install binary format" },
    { "remove",		OPT_REMOVE,	0,		OPTION_HIDDEN,
	"remove binary format" },
    { "import",		OPT_IMPORT,	0,
	OPTION_ARG_OPTIONAL | OPTION_HIDDEN,
	"import packaged format file" },
    { "unimport",	OPT_UNIMPORT,	0,
	OPTION_ARG_OPTIONAL | OPTION_HIDDEN,
	"unimport packaged format file" },
    { "display",	OPT_DISPLAY,	0,
	OPTION_ARG_OPTIONAL | OPTION_HIDDEN,
	"display information on binary format" },
    { "enable",		OPT_ENABLE,	0,
	OPTION_ARG_OPTIONAL | OPTION_HIDDEN,
	"enable binary format in kernel" },
    { "disable",	OPT_DISABLE,	0,
	OPTION_ARG_OPTIONAL | OPTION_HIDDEN,
	"disable binary format in kernel" },
    { "find",		OPT_FIND,	0,		OPTION_HIDDEN,
	"find list of interpreters for an executable" },
    { "magic",		OPT_MAGIC,	"BYTE-SEQUENCE",
	OPTION_HIDDEN,
	"match files starting with this byte sequence" },
    { "mask",		OPT_MASK,	"BYTE-SEQUENCE",
	OPTION_HIDDEN,
	"mask byte sequence with this string before comparing with magic" },
    { "offset",		OPT_OFFSET,	"OFFSET",   	OPTION_HIDDEN,
	"byte offset of magic/mask in file" },
    { "extension",	OPT_EXTENSION,	"EXTENSION",	OPTION_HIDDEN,
	"match files whose names end in .EXTENSION" },
    { "detector",	OPT_DETECTOR,	"PATH",		OPTION_HIDDEN,
	"use this userspace detector program" },
    { "credentials",	OPT_CREDENTIALS, "YES/NO",	OPTION_HIDDEN,
	"use credentials of original binary for interpreter (yes/no)" },
    { "preserve",	OPT_PRESERVE, "YES/NO",	OPTION_HIDDEN,
	"preserve argv[0] of original binary for interpreter (yes/no)" },
    { "fix-binary",	OPT_FIX_BINARY,	"YES/NO",	OPTION_HIDDEN,
	"open interpreter binary immediately and always use open image "
	"(yes/no)" },
    { "package",	OPT_PACKAGE,	"PACKAGE-NAME",	0,
	"for --install and --remove, specify the current package name", 1 },
    { "admindir",	OPT_ADMINDIR,	"DIRECTORY",	0,
	"administration directory (default: " ADMINDIR ")", 2 },
    { "importdir",	OPT_IMPORTDIR,	"DIRECTORY",	0,
	"import directory (default: " IMPORTDIR ")", 3 },
    { "procdir",	OPT_PROCDIR,	"DIRECTORY",	OPTION_HIDDEN,
	"proc directory, for test suite use only "
	"(default: " PROCDIR ")", 5 },
    { "test",		OPT_TEST,	0,		0,
	"don't do anything, just demonstrate", 6 },
    { 0 }
};

const char *package, *name, *executable;
static enum opts mode, type;

static struct {
    const char *magic;
    const char *mask;
    const char *offset;
    const char *extension;
    const char *interpreter;
    const char *detector;
    const char *credentials;
    const char *preserve;
    const char *fix_binary;
} spec;

static const char *mode_name (enum opts m)
{
    switch (m) {
	case OPT_INSTALL:	return "install";
	case OPT_REMOVE:	return "remove";
	case OPT_IMPORT:	return "import";
	case OPT_UNIMPORT:	return "unimport";
	case OPT_DISPLAY:	return "display";
	case OPT_ENABLE:	return "enable";
	case OPT_DISABLE:	return "disable";
	case OPT_FIND:		return "find";
	default:		return "";
    }
}

static const char *type_name (enum opts t)
{
    switch (t) {
	case OPT_MAGIC:		return "magic";
	case OPT_EXTENSION:	return "extension";
	default:		return "";
    }
}

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    /* Mode/type processing. */
    switch (key) {
	case OPT_INSTALL:
	case OPT_REMOVE:
	case OPT_IMPORT:
	case OPT_UNIMPORT:
	case OPT_DISPLAY:
	case OPT_ENABLE:
	case OPT_DISABLE:
	case OPT_FIND:
	    if (mode)
		argp_error (state, "two modes given: --%s and --%s",
			    mode_name (mode), mode_name (key));
	    mode = key;
	    break;

	case OPT_MAGIC:
	case OPT_EXTENSION:
	    if (type)
		argp_error (state,
			    "two binary format specifications given: "
			    "--%s and --%s",
			    type_name (type), type_name (key));
	    type = key;
	    break;

	default:
	    break;
    }

    /* Argument processing. */
    switch (key) {
	case OPT_INSTALL:
	case OPT_REMOVE:
	    if (state->next > state->argc - 2)
		argp_error (state, "--%s needs <name> <path>",
			    (key == OPT_INSTALL) ? "install" : "remove");
	    name = state->argv[state->next++];
	    spec.interpreter = state->argv[state->next++];
	    if (access (spec.interpreter, X_OK))
		warning ("no executable %s found, but continuing anyway as "
			 "you request", spec.interpreter);
	    return 0;

	case OPT_IMPORT:
	case OPT_UNIMPORT:
	case OPT_DISPLAY:
	case OPT_ENABLE:
	case OPT_DISABLE:
	    if (state->next < state->argc)
		name = state->argv[state->next++];
	    return 0;

	case OPT_FIND:
	    if (state->next >= state->argc)
		argp_error (state, "--find needs <path>");
	    executable = state->argv[state->next++];
	    return 0;

	case OPT_MAGIC:
	    spec.magic = arg;
	    return 0;

	case OPT_MASK:
	    if (spec.mask)
		argp_error (state, "more than one --mask option given");
	    spec.mask = arg;
	    return 0;

	case OPT_OFFSET:
	    if (spec.offset)
		argp_error (state, "more than one --offset option given");
	    spec.offset = arg;
	    {
		const char *p;
		for (p = spec.offset; *p; ++p)
		    if (!isdigit ((int) *p))
			argp_failure (state, argp_err_exit_status, 0,
				      "offset must be a whole number");
	    }
	    return 0;

	case OPT_EXTENSION:
	    spec.extension = arg;
	    return 0;

	case OPT_DETECTOR:
	    if (spec.detector)
		argp_error (state, "more than one --detector option given");
	    spec.detector = arg;
	    if (access (spec.detector, X_OK))
		warning ("no executable %s found, but continuing anyway as "
			 "you request", spec.detector);
	    return 0;

	case OPT_CREDENTIALS:
	    spec.credentials = arg;
	    return 0;

	case OPT_PRESERVE:
	    spec.preserve = arg;
	    return 0;

	case OPT_FIX_BINARY:
	    spec.fix_binary = arg;
	    return 0;

	case OPT_PACKAGE:
	    if (package)
		argp_error (state, "more than one --package option given");
	    package = arg;
	    return 0;

	case OPT_ADMINDIR:
	    admindir = arg;
	    return 0;

	case OPT_IMPORTDIR:
	    importdir = arg;
	    return 0;

	case OPT_PROCDIR:
	    procdir = arg;
	    return 0;

	case OPT_TEST:
	    test = 1;
	    return 0;

	case ARGP_KEY_SUCCESS:
	    if (!mode)
		argp_error (state,
			    "you must use one of --install, --remove, "
			    "--import, --unimport, --display, --enable, "
			    "--disable, --find");
	    else if (mode == OPT_INSTALL) {
		if (!type)
		    argp_error (state, "--install requires a <spec> option");
		if (!strcmp (name, ".") || !strcmp (name, "..") ||
		    !strcmp (name, "register") || !strcmp (name, "status"))
		    argp_failure (state, argp_err_exit_status, 0,
				  "binary format name '%s' is reserved", name);
	    }
	    return 0;
    }

    return ARGP_ERR_UNKNOWN;
}

static struct argp argp = {
    options, parse_opt,
    "--install <name> <path> <spec>\n"
    "--remove <name> <path>\n"
    "--import [<name>]\n"
    "--unimport [<name>]\n"
    "--display [<name>]\n"
    "--enable [<name>]\n"
    "--disable [<name>]\n"
    "--find <path>",
    "\n"
    "where <spec> is one of\n"
    "\n"
    "      --magic <byte-sequence> [--mask <byte-sequence>] "
	"[--offset <offset>]\n"
    "      --extension <extension>\n"
    "\n"
    "The following argument may be added to any <spec> to have a userspace "
    "process determine whether the file should be handled:\n"
    "\n"
    "      --detector <path>\n"
    "\n"
    "Options:"
    "\v"
    "Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,\n"
    "              2009, 2010 Colin Watson.\n"
    "This is free software; see the GNU General Public License version 3 or\n"
    "later for copying conditions."
};

int main (int argc, char **argv)
{
    int status = 0;

    program_name = xstrdup ("update-binfmts");

    xstdopen ();

    check_supported_os ();

    argp_err_exit_status = 2;
    if (argp_parse (&argp, argc, argv, 0, 0, 0))
	exit (argp_err_exit_status);

    path_register = xasprintf ("%s/register", procdir);
    path_status = xasprintf ("%s/status", procdir);
    run_detectors = xasprintf ("%s/run-detectors", auxdir);

    if (!package)
	package = ":";

    formats = new_string_omap (GL_RBTREE_OMAP,
			       (gl_mapvalue_dispose_fn) binfmt_free);

    if (mode == OPT_INSTALL) {
	struct binfmt *binfmt;
	gl_map_t format_args;

	format_args = gl_map_create_empty (GL_HASH_MAP, string_equals,
					   string_hash, NULL, NULL);
	gl_map_put (format_args, "package", package);
	gl_map_put (format_args, "type",
		    (type == OPT_MAGIC) ? "magic" : "extension");
#define ADD_SPEC(field) do { \
    if (spec.field) \
	gl_map_put (format_args, #field, spec.field); \
} while (0)
	ADD_SPEC (magic);
	ADD_SPEC (mask);
	ADD_SPEC (offset);
	ADD_SPEC (extension);
	ADD_SPEC (interpreter);
	ADD_SPEC (detector);
	ADD_SPEC (credentials);
	ADD_SPEC (preserve);
	ADD_SPEC (fix_binary);
#undef ADD_SPEC
	binfmt = binfmt_new (name, format_args);

	status = act_install (name, binfmt, 1);
    } else if (mode == OPT_REMOVE)
	status = act_remove (name, package, 1);
    else if (mode == OPT_IMPORT)
	status = act_import (name);
    else if (mode == OPT_UNIMPORT)
	status = act_unimport (name);
    else if (mode == OPT_DISPLAY)
	status = act_display (name);
    else if (mode == OPT_ENABLE)
	status = act_enable (name);
    else if (mode == OPT_DISABLE)
	status = act_disable (name);
    else if (mode == OPT_FIND)
	status = act_find (executable);

    if (status)
	return 0;
    else
	quit ("exiting due to previous errors");
}
