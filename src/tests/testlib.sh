# Copyright (C) 2010, 2011, 2012 Colin Watson.
#
# This file is part of binfmt-support.
#
# binfmt-support is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# binfmt-support is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with binfmt-support; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

failures=0

: ${UPDATE_BINFMTS=update-binfmts}
: ${RUN_DETECTORS=run-detectors}

FAKE_PROC=false
cleanup () {
	if $FAKE_PROC && [ -e "$tmpdir/proc/register" ]; then
		fusermount -u "$tmpdir/proc"
	fi
	rm -rf "$tmpdir"
}

init () {
	tmpdir="tmp-${0##*/}"
	mkdir -p "$tmpdir" || exit $?
	trap cleanup HUP INT QUIT TERM
	mkdir -p "$tmpdir/usr/share/binfmts" "$tmpdir/var/lib/binfmts"
}

fake_proc () {
	mkdir -p "$tmpdir/proc"
	if [ "$TEST_FUSE_DEBUG" ]; then
		"$srcdir/binfmt_misc.py" -d "$tmpdir/proc" &
		sleep 1
	else
		if ! "$srcdir/binfmt_misc.py" "$tmpdir/proc" 2>/dev/null; then
			echo "SKIP: python/fuse unavailable"
			cleanup
			exit 77
		fi
	fi
	FAKE_PROC=:
}

update_binfmts () {
	$UPDATE_BINFMTS --admindir "$tmpdir/var/lib/binfmts" \
			--importdir "$tmpdir/usr/share/binfmts" "$@"
}

update_binfmts_proc () {
	update_binfmts --procdir "$tmpdir/proc" "$@"
}

run_detectors () {
	$RUN_DETECTORS --admindir "$tmpdir/var/lib/binfmts" \
		       --procdir "$tmpdir/proc" "$@"
}

expect_pass () {
	ret=0
	eval "$2" || ret=$?
	if [ "$ret" = 0 ]; then
		echo "  PASS: $1"
	else
		failures="$(($failures + 1))"
		echo "  FAIL: $1"
	fi
}

finish () {
	case $failures in
		0)
			cleanup
			exit 0
			;;
		*)
			if [ -z "$TEST_FAILURE_KEEP" ]; then
				cleanup
			fi
			exit 1
			;;
	esac
}
