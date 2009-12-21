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

#ifndef __SHOWPART_H__
#define __SHOWPART_H__

struct s_blkdev;
struct s_diskinfo;

int partlist_showlist(bool details);

enum {BLKDEV_INVALID=-1, BLKDEV_PHYSDISK=0, BLKDEV_FILESYSDEV=1};

struct s_blkdev
{
    int  devtype;
    char devname[FSA_MAX_DEVLEN];
    char longname[FSA_MAX_DEVLEN];
    char label[FSA_MAX_LABELLEN];
    char uuid[FSA_MAX_UUIDLEN];
    char fsname[FSA_MAX_FSNAMELEN];
    char name[512];
    char txtsize[64];
    u64 devsize;
    char minor[16];
    char major[16];
    u64 rdev;
};

struct s_diskinfo
{
    bool detailed;
    char format[256];
    char title[256];
};

#endif // __SHOWPART_H__
