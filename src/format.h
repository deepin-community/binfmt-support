/* format.h - read/write binary format files
 *
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
 *               2010 Colin Watson <cjwatson@debian.org>.
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

#include "gl_map.h"

struct binfmt {
    char *name;
    char *package;
    char *type;
    char *offset;
    char *magic;
    size_t magic_size; /* only used in --find and run-detectors */
    char *mask;
    char *interpreter;
    char *detector;
    char *credentials;
    char *preserve;
    char *fix_binary;
};

struct binfmt *binfmt_load (const char *name, const char *filename, int quiet);
struct binfmt *binfmt_new (const char *name, gl_map_t args);
int binfmt_write (const struct binfmt *binfmt, const char *filename);
void binfmt_print (const struct binfmt *binfmt);
int binfmt_equals (const struct binfmt *left, const struct binfmt *right);
void binfmt_free (struct binfmt *binfmt);
