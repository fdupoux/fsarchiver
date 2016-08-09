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

#ifndef __FS_VFAT_H__
#define __FS_VFAT_H__

struct s_dico;
struct s_strlist;

int vfat_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int vfat_getinfo(struct s_dico *d, char *devname);
int vfat_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int vfat_get_reqmntopt(char *partition, struct s_strlist *reqopt, struct s_strlist *badopt);
int vfat_umount(char *partition, char *mntbuf);
int vfat_test(char *devname);
int vfat_check_compatibility(u64 compat, u64 ro_compat, u64 incompat, u64 log_incompat);

#define VFAT_SB_MAGIC 0x55AA
enum fat_type {FAT_TYPE_FAT32=0, FAT_TYPE_FAT16};

struct vfat_superblock
{
    u8  jump[3];
    u8  oem_id[8];
    u16 u8s_per_sector;
    u8  sectors_per_cluster;
    u16 num_boot_sectors;
    u8  num_fats;
    u16 num_root_dir_ents;
    u16 total_sectors;
    u8  media_id;
    u16 sectors_per_fat;
    u16 sectors_per_track;
    u16 heads;
    u32 hidden_sectors;
    u32 total_sectors_large;
    u8  boot_code[474];
    u16 magic;
} __attribute__((packed));

#endif // __FS_VFAT_H__
