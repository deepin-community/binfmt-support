/* run-detectors.c - run userspace format detectors
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
#include <unistd.h>

#include "argp.h"
#include "gl_xlist.h"
#include "xalloc.h"

#include "error.h"
#include "find.h"
#include "format.h"
#include "glcontainers.h"
#include "paths.h"

char *program_name;

const char *argp_program_version = "binfmt-support " PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

enum opts {
    OPT_ADMINDIR = 256,
    OPT_PROCDIR
};

static struct argp_option options[] = {
    { "admindir",	OPT_ADMINDIR,	"DIRECTORY",	0,
	"administration directory (default: " ADMINDIR ")" },
    { "procdir",	OPT_PROCDIR,	"DIRECTORY",	OPTION_HIDDEN,
	"proc directory, for test suite use only "
	"(default: " PROCDIR ")", 5 },
    { 0 }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    switch (key) {
	case OPT_ADMINDIR:
	    admindir = arg;
	    return 0;

	case OPT_PROCDIR:
	    procdir = arg;
	    return 0;
    }

    return ARGP_ERR_UNKNOWN;
}

static struct argp argp = {
    options, parse_opt,
    "<target>",
    "\v"
    "Copyright (C) 2002, 2010 Colin Watson.\n"
    "This is free software; see the GNU General Public License version 3 or\n"
    "later for copying conditions."
};

int main (int argc, char **argv)
{
    int arg_index;
    char **real_argv;
    int i;
    gl_list_t interpreters;
    const struct binfmt *binfmt;

    program_name = xstrdup ("run-detectors");

    argp_err_exit_status = 2;
    if (argp_parse (&argp, argc, argv, ARGP_IN_ORDER, &arg_index, 0))
	exit (argp_err_exit_status);
    if (arg_index >= argc)
	quit ("argument required");

    real_argv = xcalloc (argc - arg_index + 2, sizeof *real_argv);
    for (i = arg_index; i < argc; ++i)
	real_argv[i - arg_index + 1] = argv[i];
    real_argv[argc - arg_index + 1] = NULL;

    interpreters = find_interpreters (argv[arg_index]);

    /* Try to exec() each interpreter in turn. */
    GL_LIST_FOREACH_START (interpreters, binfmt) {
	real_argv[0] = (char *) binfmt->interpreter;
	fflush (NULL);
	execvp (binfmt->interpreter, real_argv);
	warning_err ("unable to exec %s", binfmt->interpreter);
    } GL_LIST_FOREACH_END (interpreters);

    gl_list_free (interpreters);

    quit ("unable to find an interpreter for %s", argv[arg_index]);
}
