dnl Process this file with autoconf to produce a configure script.

# Copyright (C) 2010 Colin Watson.
#
# This file is part of binfmt-support.
#
# binfmt-support is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# binfmt-support is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with binfmt-support; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
# USA.

# Initialise and check we're in the correct directory.
AC_INIT([binfmt-support], [2.2.1], [cjwatson@debian.org])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign parallel-tests])
AM_MAINTAINER_MODE
AM_SILENT_RULES([yes])
AC_PREREQ([2.63])
AC_CONFIG_SRCDIR([src/update-binfmts.c])
AC_GNU_SOURCE
BINFMT_TAR_SORT_NAME

AC_CONFIG_HEADER([config.h])
AC_CANONICAL_HOST

# Check $PATH for the following programs and append suitable options.
AC_PROG_CC
gl_EARLY
AC_PROG_CPP
CFLAGS="$CFLAGS -Wall"
if test "$GCC" = yes
then
	gl_WARN_ADD([-W])
	gl_WARN_ADD([-Wpointer-arith])
	gl_WARN_ADD([-Wwrite-strings])
	gl_WARN_ADD([-Wstrict-prototypes])
	gl_WARN_ADD([-Wshadow])
	gl_WARN_ADD([-Wformat-security])
	gl_WARN_ADD([-Wredundant-decls])
	gl_WARN_ADD([-Wno-missing-field-initializers])
fi
AC_PROG_MKDIR_P

# Check for various header files and associated libraries.
AC_ISC_POSIX
gl_INIT

# Checks for structures and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

PKG_CHECK_MODULES([libpipeline], [libpipeline])

AC_ARG_ENABLE([sysvinit],
	      AS_HELP_STRING([--enable-sysvinit], [Install sysvinit script]))
AM_CONDITIONAL([INSTALL_SYSVINIT], [test "x$enable_sysvinit" = xyes])

AC_ARG_ENABLE([openrc],
	      AS_HELP_STRING([--enable-openrc], [Install OpenRC script]))
AM_CONDITIONAL([INSTALL_OPENRC], [test "x$enable_openrc" = xyes])

if test "x$enable_sysvinit" = xyes && test "x$enable_openrc" = xyes; then
	AC_MSG_ERROR([--enable-sysvinit and --enable-openrc are mutually exclusive])
fi

AC_ARG_ENABLE([upstart],
	      AS_HELP_STRING([--disable-upstart], [Install Upstart job]))
AM_CONDITIONAL([INSTALL_UPSTART], [test "x$enable_upstart" != xno])

AC_ARG_ENABLE([systemd],
	      AS_HELP_STRING([--disable-systemd], [Install systemd unit]))
AM_CONDITIONAL([INSTALL_SYSTEMD], [test "x$enable_systemd" != xno])

AC_ARG_WITH([systemdsystemunitdir],
	    AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
	    [], [with_systemdsystemunitdir="$($PKG_CONFIG --variable=systemdsystemunitdir systemd)"])
AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemdsystemunitdir" != x])

# If we're cross-compiling, tests won't work.
AM_CONDITIONAL([CROSS_COMPILING], [test "x$cross_compiling" = xyes])

AC_SUBST([admindir], ['$(localstatedir)/lib/binfmts'])
AC_SUBST([importdir], ['$(datadir)/binfmts'])
AC_SUBST([procdir], ['/proc/sys/fs/binfmt_misc'])

AC_CONFIG_FILES([Makefile
	gl/lib/Makefile
	doc/Makefile
	init/Makefile
	init/openrc/Makefile
	init/systemd/Makefile
	init/sysvinit/Makefile
	init/upstart/Makefile
	man/Makefile
	src/Makefile
	src/tests/Makefile])
AC_OUTPUT
