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

#ifndef __FS_JFS_H__
#define __FS_JFS_H__

struct s_dico;
struct s_strlist;

int jfs_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int jfs_getinfo(struct s_dico *d, char *devname);
int jfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int jfs_get_reqmntopt(char *partition, struct s_strlist *reqopt, struct s_strlist *badopt);
int jfs_umount(char *partition, char *mntbuf);
int jfs_test(char *devname);

// make the magic number something a human could read
#define JFS_MAGIC    "JFS1"        /* Magic word */
#define LV_NAME_SIZE    11        /* MUST BE 11 for OS/2 boot sector */
#define JFS_SUPER1_OFF    0x8000        /* primary superblock */

/*
 *    physical xd (pxd)
 */
typedef struct {
    unsigned len:24;
    unsigned addr1:8;
    u32 addr2;
} pxd_t;

/*
 * Almost identical to Linux's timespec, but not quite
 */
struct timestruc_t {
    u32 tv_sec;
    u32 tv_nsec;
};

/*
 *    aggregate superblock
 *
 * The name superblock is too close to super_block, so the name has been
 * changed to jfs_superblock.  The utilities are still using the old name.
 */
struct jfs_superblock {
    char s_magic[4];    /* 4: magic number */
    u32 s_version;    /* 4: version number */

    u64 s_size;        /* 8: aggregate size in hardware/LVM blocks;
                 * VFS: number of blocks
                 */
    u32 s_bsize;        /* 4: aggregate block size in bytes;
                 * VFS: fragment size
                 */
    u16 s_l2bsize;    /* 2: log2 of s_bsize */
    u16 s_l2bfactor;    /* 2: log2(s_bsize/hardware block size) */
    u32 s_pbsize;    /* 4: hardware/LVM block size in bytes */
    u16 s_l2pbsize;    /* 2: log2 of s_pbsize */
    u16 pad;        /* 2: padding necessary for alignment */

    u32 s_agsize;    /* 4: allocation group size in aggr. blocks */

    u32 s_flag;        /* 4: aggregate attributes:
                 *    see jfs_filsys.h
                 */
    u32 s_state;        /* 4: mount/unmount/recovery state:
                 *    see jfs_filsys.h
                 */
    u32 s_compress;        /* 4: > 0 if data compression */

    pxd_t s_ait2;        /* 8: first extent of secondary
                 *    aggregate inode table
                 */

    pxd_t s_aim2;        /* 8: first extent of secondary
                 *    aggregate inode map
                 */
    u32 s_logdev;        /* 4: device address of log */
    u32 s_logserial;    /* 4: log serial number at aggregate mount */
    pxd_t s_logpxd;        /* 8: inline log extent */

    pxd_t s_fsckpxd;    /* 8: inline fsck work space extent */

    struct timestruc_t s_time;    /* 8: time last updated */

    u32 s_fsckloglen;    /* 4: Number of filesystem blocks reserved for
                 *    the fsck service log.
                 *    N.B. These blocks are divided among the
                 *         versions kept.  This is not a per
                 *         version size.
                 *    N.B. These blocks are included in the
                 *         length field of s_fsckpxd.
                 */
    s8 s_fscklog;        /* 1: which fsck service log is most recent
                 *    0 => no service log data yet
                 *    1 => the first one
                 *    2 => the 2nd one
                 */
    char s_fpack[11];    /* 11: file system volume name
                 *     N.B. This must be 11 bytes to
                 *          conform with the OS/2 BootSector
                 *          requirements
                 *          Only used when s_version is 1
                 */

    /* extendfs() parameter under s_state & FM_EXTENDFS */
    u64 s_xsize;        /* 8: extendfs s_size */
    pxd_t s_xfsckpxd;    /* 8: extendfs fsckpxd */
    pxd_t s_xlogpxd;    /* 8: extendfs logpxd */
    /* - 128 byte boundary - */

    char s_uuid[16];    /* 16: 128-bit uuid for volume */
    char s_label[16];    /* 16: volume label */
    char s_loguuid[16];    /* 16: 128-bit uuid for log device */
};

#endif // __FS_JFS_H__
