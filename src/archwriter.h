/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2016 Francois Dupoux.  All rights reserved.
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

#ifndef __ARCHWRITER_H__
#define __ARCHWRITER_H__

#include <limits.h>
#include "strlist.h"

struct s_writebuf;
struct s_blockinfo;
struct s_headinfo;
struct s_strlist;

struct s_archwriter;
typedef struct s_archwriter carchwriter;

struct s_archwriter
{   int    archfd; // file descriptor of the current volume (set to -1 when closed)
    u32    archid; // 32bit archive id for checking (random number generated at creation)
    u32    curvol; // current volume number, starts at 0, incremented when we change the volume
    bool   newarch; // true when the archive has been created by then current process
    char   filefmt[FSA_MAX_FILEFMTLEN]; // file format of that archive
    char   creatver[FSA_MAX_PROGVERLEN]; // fsa version used to create archive
    char   label[FSA_MAX_LABELLEN]; // archive label defined by the user
    char   basepath[PATH_MAX]; // path of the first volume of an archive
    char   volpath[PATH_MAX]; // path of the current volume of an archive
    cstrlist vollist; // paths to all volumes of an archive
};

int archwriter_init(carchwriter *ai);
int archwriter_destroy(carchwriter *ai);
int archwriter_create(carchwriter *ai);
int archwriter_close(carchwriter *ai);
int archwriter_remove(carchwriter *ai);
int archwriter_generate_id(carchwriter *ai);
s64 archwriter_get_currentpos(carchwriter *ai);
int archwriter_is_path_to_curvol(carchwriter *ai, char *path);
int archwriter_write_buffer(carchwriter *ai, struct s_writebuf *wb);
int archwriter_incvolume(carchwriter *ai, bool waitkeypress);
int archwriter_volpath(carchwriter *ai);
int archwriter_write_volheader(carchwriter *ai);
int archwriter_write_volfooter(carchwriter *ai, bool lastvol);
int archwriter_split_check(carchwriter *ai, struct s_writebuf *wb);
int archwriter_split_if_necessary(carchwriter *ai, struct s_writebuf *wb);
int archwriter_dowrite_block(carchwriter *ai, struct s_blockinfo *blkinfo);
int archwriter_dowrite_header(carchwriter *ai, struct s_headinfo *headinfo);

#endif // __ARCHWRITER_H__
