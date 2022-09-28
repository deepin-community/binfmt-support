## Process this file with automake to produce Makefile.in

## Copyright (C) 2014 Colin Watson.
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

EXTRA_DIST = binfmt-support.in

CLEANFILES = binfmt-support

if INSTALL_OPENRC
openrcdir = $(sysconfdir)/init.d
openrc_SCRIPTS = binfmt-support

binfmt-support: binfmt-support.in
	sed -e "s,[@]sbindir[@],$(sbindir),g" $< > $@
endif
