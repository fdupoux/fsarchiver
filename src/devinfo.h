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

#ifndef __DEVINFO_H__
#define __DEVINFO_H__

enum {BLKDEV_INVALID=-1, BLKDEV_PHYSDISK=0, BLKDEV_FILESYSDEV=1};

struct s_devinfo
{
    int  devtype;
    char devname[FSA_MAX_DEVLEN];
    char longname[FSA_MAX_DEVLEN];
    char label[FSA_MAX_LABELLEN];
    char uuid[FSA_MAX_UUIDLEN];
    char fsname[FSA_MAX_FSNAMELEN];
    char name[512];
    char txtsize[64];
    u64  devsize;
    int  minor;
    int  major;
    u64  rdev;
};

int get_devinfo(struct s_devinfo *outdev, char *indevname, int min, int maj);

#endif // __DEVINFO_H__
