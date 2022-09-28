## Process this file with automake to produce Makefile.in

## Copyright (C) 2010, 2011 Colin Watson.
##
## This file is part of binfmt-support.
##
## binfmt-support is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 3 of the License, or (at your
## option) any later version.
##
## binfmt-support is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
## Public License for more details.
##
## You should have received a copy of the GNU General Public License along
## with binfmt-support; if not, write to the Free Software Foundation, Inc.,
## 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

SUBDIRS = tests

sbin_PROGRAMS = update-binfmts
pkglibexec_PROGRAMS = run-detectors

AM_CPPFLAGS = \
	-I$(top_builddir)/gl/lib \
	-I$(top_srcdir)/gl/lib \
	-DADMINDIR=\"$(admindir)\" \
	-DIMPORTDIR=\"$(importdir)\" \
	-DPROCDIR=\"$(procdir)\" \
	-DAUXDIR=\"$(pkglibexecdir)\"

AM_CFLAGS = \
	$(libpipeline_CFLAGS)

LIBGNU = $(top_builddir)/gl/lib/libgnu.a

update_binfmts_LDADD = $(libpipeline_LIBS) $(LIBGNU)
run_detectors_LDADD = $(libpipeline_LIBS) $(LIBGNU)

COMMON = \
	error.c \
	error.h \
	find.c \
	find.h \
	format.c \
	format.h \
	glcontainers.c \
	glcontainers.h \
	paths.c \
	paths.h

update_binfmts_SOURCES = \
	$(COMMON) \
	update-binfmts.c

run_detectors_SOURCES = \
	$(COMMON) \
	run-detectors.c

install-data-hook:
	$(MKDIR_P) $(DESTDIR)$(admindir)
	$(MKDIR_P) $(DESTDIR)$(importdir)
