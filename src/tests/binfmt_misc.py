#! /usr/bin/python
# binfmt_misc filesystem emulator using FUSE.

# Copyright (C) 2010 Colin Watson.
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

import os
import stat
import errno

import fuse

fuse.fuse_python_api = (0, 2)

# We only need to emulate a small number of operations here:
#
#   * echo 1 >status
#   * echo "$regstring" >register
#   * test -e "$entry"
#   * cat "$entry"
#   * printf %d -1 >"$entry"
#
# Anything else is gravy.

class BinfmtMisc(fuse.Fuse):
    def __init__(self, *args, **kwargs):
        fuse.Fuse.__init__(self, *args, **kwargs)
        self.enabled = False
        self.names = {}

    def create_entry(self, path, fh=None):
        def format_arg(s):
            return ''.join(['%02x' % ord(c) for c in s])

        if not path.startswith('/') or path[1:] not in self.names:
            raise ValueError

        fields = self.names[path[1:]]
        lines = ['enabled']
        lines.append('interpreter %s' % fields[4])
        lines.append('flags: ') # TODO
        if fields[0] == 'E':
            lines.append('extension .%s' % fields[2])
        else:
            lines.append('offset %d' % fields[1])
            lines.append('magic %s' % format_arg(fields[2]))
            if fields[3]:
                lines.append('mask %s' % format_arg(fields[3]))
        lines.append('')
        return '\n'.join(lines)

    def getattr(self, path):
        st = fuse.Stat()
        st.st_ino = 0
        st.st_dev = 0
        st.st_blksize = 0
        st.st_rdev = 0
        st.st_uid = 0
        st.st_gid = 0
        st.st_size = 0
        st.st_blocks = 0
        st.st_atime = 0
        st.st_mtime = 0
        st.st_ctime = 0

        if path == '/':
            st.st_mode = stat.S_IFDIR | 0o755
            st.st_nlink = 2
            return st
        else:
            st.st_mode = stat.S_IFREG | 0o644
            st.st_nlink = 1

        if path == '/register':
            st.st_mode = (st.st_mode & ~0o777) | 0o200
            return st
        elif path == '/status':
            return st
        else:
            try:
                st.st_size = len(self.create_entry(path))
                return st
            except ValueError:
                return -errno.ENOENT

    def readdir(self, path, offset):
        if path != '/':
            raise OSError(errno.ENOENT, "readdir only works on /", path)

        dirents = ['.', '..']
        dirents.extend(list(self.names))
        dirents.extend(('register', 'status'))
        for dirent in dirents:
            yield fuse.Direntry(dirent)

    def readlink(self, path):
        return -errno.EINVAL

    def truncate(self, path, offset):
        pass

    def open(self, path, flags):
        if not isinstance(self.getattr(path), fuse.Stat):
            return -errno.ENOENT
        accmode = os.O_RDONLY | os.O_WRONLY | os.O_RDWR
        if path == '/register' and (flags & accmode) != os.O_WRONLY:
            return -errno.EACCES

    def read(self, path, length, offset, fh=None):
        if path == '/status' or path == '/register':
            return -errno.EINVAL
        else:
            try:
                return self.create_entry(path)[offset:offset + length]
            except ValueError:
                return -errno.ENOENT

    def write(self, path, buf, offset, fh=None):
        def parse_arg(s):
            # left out of Python for some bizarre reason
            def isxdigit(c):
                return c.isdigit() or c.lower() in 'abcdef'

            out = ''
            end = len(s)
            i = 0
            while i < end:
                if i + 1 < end and s[i] == '\\' and s[i + 1] == 'x':
                    if (i + 3 < end and
                        isxdigit(s[i + 2]) and isxdigit(s[i + 3])):
                        out += chr(int(s[i + 2:i + 4], 16))
                        i += 4
                    else:
                        raise ValueError
                else:
                    out += s[i]
                    i += 1
            return out

        if path == '/status':
            if buf.rstrip('\n') == '1':
                self.enabled = True
            else:
                self.enabled = False
        elif path == '/register':
            if not buf or len(buf) < 11 or len(buf) > 256:
                return -errno.EINVAL
            fields = buf.rstrip('\n').split(buf[0])[1:]
            if len(fields) < 6:
                return -errno.EINVAL
            name = fields.pop(0)
            if name in self.names:
                return -errno.EEXIST
            if not name or name == '.' or name == '..' or '/' in name:
                return -errno.EINVAL
            if fields[0] != 'E' and fields[0] != 'M':
                return -errno.EINVAL
            if not fields[2]:
                return -errno.EINVAL
            if fields[0] == 'M':
                try:
                    fields[1] = int(fields[1])
                    fields[2] = parse_arg(fields[2])
                    fields[3] = parse_arg(fields[3])
                    if fields[3] and len(fields[2]) != len(fields[3]):
                        return -errno.EINVAL
                except ValueError:
                    return -errno.EINVAL
            else:
                if '/' in fields[2]:
                    return -errno.EINVAL
            if not fields[4]:
                return -errno.EINVAL
            # TODO do something with flags
            self.names[name] = fields
        elif path.startswith('/') and path[1:] in self.names:
            if buf.rstrip('\n') == '-1':
                del self.names[path[1:]]
            else:
                return -errno.EINVAL
        else:
            return -errno.ENOENT
        return len(buf)

    def flush(self, path):
        pass

def main():
    usage="Incomplete binfmt_misc emulator\n\n%s" % fuse.Fuse.fusage
    server = BinfmtMisc(version="%prog " + fuse.__version__,
                        usage=usage,
                        dash_s_do='setsingle')
    server.multithreaded = False
    server.parse(errex=2)
    server.main()

if __name__ == '__main__':
    main()
