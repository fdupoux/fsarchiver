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

#ifndef __FS_NTFS_H__
#define __FS_NTFS_H__

struct s_dico;
struct s_strlist;

#define NTFS3G_VERSION(year, month, day)    ((u64)((((u64)year&0xFFFF)<<48)+(((u64)month&0xFFFF)<<32)+(((u64)day&0xFFFF)<<16)))
#define NTFS3G_MINVER_Y                     2009
#define NTFS3G_MINVER_M                     11
#define NTFS3G_MINVER_D                     14

struct s_ntfsinfo
{   u32 bytes_per_sector;
    u32 sectors_per_clusters;
    u32 bytes_per_cluster;
    u64 uuid;
};

int ntfs_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int ntfs_getinfo(struct s_dico *d, char *devname);
int ntfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int ntfs_get_reqmntopt(char *partition, struct s_strlist *reqopt, struct s_strlist *badopt);
int ntfs_replace_uuid(char *devname, u64 uuid);
int ntfs_umount(char *partition, char *mntbuf);
int ntfs_test(char *devname);

#endif // __FS_NTFS_H__
