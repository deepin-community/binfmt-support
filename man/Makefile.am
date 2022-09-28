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

man8_MANS = update-binfmts.8
CLEANFILES = $(man8_MANS)

.man8.8:
	sed -e 's,%admindir%,$(admindir),g; s,%importdir%,$(importdir),g' \
		$< > $@

dist-hook:
	set -e; \
	dist_files=`for file in $(man8_MANS); do \
			echo $$file | sed 's/\.\([0-9]\)/\.man\1/'; \
		    done`; \
	for file in $$dist_files; do \
		test -f $(distdir)/$$file \
		|| cp -p $(srcdir)/$$file $(distdir)/$$file; \
	done
