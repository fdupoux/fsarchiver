/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2009 Francois Dupoux.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Homepage: http://www.fsarchiver.org
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include "types.h"

struct timeval;

int exec_command(char *command, int cmdbufsize, char *stdoutbuf, int stdoutsize, char *stderrbuf, int stderrsize, char *format, ...);
int get_parent_dir_time_attrib(char *filepath, char *parentdirbuf, int bufsize, struct timeval *tv);
void concatenate_paths(char *buffer, int maxbufsize, char *p1, char *p2);
int path_force_extension(char *buf, int bufsize, char *origpath, char *ext);
char *format_size(u64 size, char *text, int max, char units);
int image_write_data(int fdarch, char *buffer, int buflen);
int extract_dirpath(char *filepath, char *dirbuf, int dirbufsize);
int generate_random_tmpdir(char *buffer, int bufsize, int n);
char *format_time(char *buffer, int bufsize, u64 t);
int stream_readline(FILE *f, char *buf, int buflen);
char *format_md5(char *buf, int maxbuf, u8 *md5bin);
int mkdir_recursive(char *path);
char *get_objtype_name(int objtype);
int is_dir_empty(char *path);
u32 generate_random_u32_id(void);
u32 fletcher32(u8 *data, u32 len);
int regfile_exists(char *filepath);
int is_magic_valid(char *magic);
char *strlcat(char *dest, const char *src, int n);
char *strlcatf(char *dest, int destbufsize, char *format, ...) __attribute__ ((format (printf, 3, 4)));
int format_stacktrace(char *buffer, int bufsize);
int stats_show(cstats stats, int fsid);
u64 stats_errcount(cstats stats);

#endif // __COMMON_H__
