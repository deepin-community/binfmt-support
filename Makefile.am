## Process this file with automake to produce Makefile.in

## Copyright (C) 2010 Colin Watson.
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

SUBDIRS = gl/lib src doc man init

EXTRA_DIST = \
	.gitignore \
	ChangeLog-2013 \
	bootstrap \
	bootstrap.conf \
	release.sh

# These macro files are imported by gnulib-tool, but at present not used.
# We include them in the distribution anyway for maximum consistency.
EXTRA_DIST += \
	gl/m4/math_h.m4

ACLOCAL_AMFLAGS = -I gl/m4 -I m4

dist-hook: gen-ChangeLog

gen_start_date = 2013-12-28
.PHONY: gen-ChangeLog
gen-ChangeLog:
	$(AM_V_GEN)if test -d .git; then \
		$(top_srcdir)/build-aux/gitlog-to-changelog \
			--format='%s%n%n%b%n' \
			--since="$(gen_start_date)" > $(distdir)/cl-t; \
		rm -f $(distdir)/ChangeLog; \
		mv $(distdir)/cl-t $(distdir)/ChangeLog; \
	fi

DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemdsystemunitdir=\$${prefix}/$(systemdsystemunitdir)
