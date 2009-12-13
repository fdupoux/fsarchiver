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

#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include <limits.h>

#include "fsarchiver.h"
#include "dico.h"

typedef struct s_archive
{
    int    archfd; // file descriptor of the current volume (set to -1 when closed)
    u32    archid; // 32bit archive id for checking (random number generated at creation)
    u64    fscount; // how many filesystems in archive (valid only if archtype=filesystems)
    int    locked; // true if the file is locked by the open/create/close functions
    u32    archtype; // what has been saved in the archive: filesystems or directories
    u32    curvol; // current volume number, starts at 0, incremented when we change the volume
    u32    compalgo; // compression algorithm which has been used to create the archive
    u32    cryptalgo; // encryption algorithm which has been used to create the archive
    u32    complevel; // compression level which is specific to the compression algorithm
    u32    fsacomp; // fsa compression level given on the command line by the user
    u64    creattime; // archive create time (number of seconds since epoch)
    bool    createdbyfsa; // true during savefs if archive has been created/written by fsa
    char    filefmt[FSA_MAX_FILEFMTLEN]; // file format of that archive
    char    creatver[FSA_MAX_PROGVERLEN]; // fsa version used to create archive
    char    label[FSA_MAX_LABELLEN]; // archive label defined by the user
    char    basepath[PATH_MAX]; // path of the first volume of an archive
    char    volpath[PATH_MAX]; // path of the current volume of an archive
} carchive;

int archive_init(carchive *ai);
int archive_destroy(carchive *ai);
int archive_create(carchive *ai);
int archive_open(carchive *ai);
int archive_close(carchive *ai);
int archive_remove(carchive *ai);
int archive_generate_id(carchive *ai);
s64 archive_get_currentpos(carchive *ai);
int archive_is_path_to_curvol(carchive *ai, char *path);

int archive_read_data(carchive *ai, void *data, u64 size);
int archive_read_dico(carchive *ai, cdico *d);
int archive_read_header(carchive *ai, char *magic, cdico **d, bool allowseek, u16 *fsid);
int archive_write_data(carchive *ai, void *data, u64 size);
int archive_incvolume(carchive *ai, bool waitkeypress);
int archive_volpath(carchive *ai);

#endif // __ARCHIVE_H__
