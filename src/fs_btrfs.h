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

#ifndef __FS_BTRFS_H__
#define __FS_BTRFS_H__

struct s_dico;
struct s_strlist;

int btrfs_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int btrfs_getinfo(struct s_dico *d, char *devname);
int btrfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int btrfs_umount(char *partition, char *mntbuf);
int btrfs_check_support_for_features(u64 compat, u64 incompat, u64 ro_compat);
int btrfs_get_reqmntopt(char *partition, struct s_strlist *reqopt, struct s_strlist *badopt);
int btrfs_test(char *devname);

// compat flags: official definition from linux-3.14/fs/btrfs/ctree.h
#define BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF    (1ULL << 0)
#define BTRFS_FEATURE_INCOMPAT_DEFAULT_SUBVOL   (1ULL << 1)
#define BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS     (1ULL << 2)
#define BTRFS_FEATURE_INCOMPAT_COMPRESS_LZO     (1ULL << 3)
#define BTRFS_FEATURE_INCOMPAT_COMPRESS_LZOv2   (1ULL << 4)
#define BTRFS_FEATURE_INCOMPAT_BIG_METADATA     (1ULL << 5)
#define BTRFS_FEATURE_INCOMPAT_EXTENDED_IREF    (1ULL << 6)
#define BTRFS_FEATURE_INCOMPAT_RAID56           (1ULL << 7)
#define BTRFS_FEATURE_INCOMPAT_SKINNY_METADATA  (1ULL << 8)
#define BTRFS_FEATURE_INCOMPAT_NO_HOLES         (1ULL << 9)

// compat flags: btrfs features that this fsarchiver version supports
#define FSA_BTRFS_FEATURE_COMPAT_SUPP           0ULL
#define FSA_BTRFS_FEATURE_COMPAT_RO_SUPP        0ULL
#define FSA_BTRFS_FEATURE_INCOMPAT_SUPP                 \
        (BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF |         \
         BTRFS_FEATURE_INCOMPAT_DEFAULT_SUBVOL |        \
         BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS |          \
         BTRFS_FEATURE_INCOMPAT_BIG_METADATA |          \
         BTRFS_FEATURE_INCOMPAT_COMPRESS_LZO |          \
         BTRFS_FEATURE_INCOMPAT_COMPRESS_LZOv2 |        \
         BTRFS_FEATURE_INCOMPAT_RAID56 |                \
         BTRFS_FEATURE_INCOMPAT_EXTENDED_IREF |         \
         BTRFS_FEATURE_INCOMPAT_SKINNY_METADATA |       \
         BTRFS_FEATURE_INCOMPAT_NO_HOLES)


// disk layout definitions
#define BTRFS_SUPER_MAGIC 0x9123683E
#define BTRFS_MAGIC "_BHRfS_M"

#define BTRFS_SUPER_INFO_OFFSET (64 * 1024)
#define BTRFS_SUPER_INFO_SIZE 4096

#define BTRFS_SUPER_MIRROR_MAX     3
#define BTRFS_SUPER_MIRROR_SHIFT 12

#define BTRFS_NAME_LEN 255
#define BTRFS_CSUM_SIZE 32
#define BTRFS_CSUM_TYPE_CRC32    0
#define BTRFS_FSID_SIZE 16
#define BTRFS_HEADER_FLAG_WRITTEN (1 << 0)
#define BTRFS_SYSTEM_CHUNK_ARRAY_SIZE 2048
#define BTRFS_LABEL_SIZE 256
#define BTRFS_UUID_SIZE 16

static inline u64 btrfs_sb_offset(int mirror)
{
    u64 start = 16 * 1024;
    if (mirror)
        return start << (BTRFS_SUPER_MIRROR_SHIFT * mirror);
    return BTRFS_SUPER_INFO_OFFSET;
}

struct btrfs_dev_item 
{
    /* the internal btrfs device id */
    __le64 devid;

    /* size of the device */
    __le64 total_bytes;

    /* bytes used */
    __le64 bytes_used;

    /* optimal io alignment for this device */
    __le32 io_align;

    /* optimal io width for this device */
    __le32 io_width;

    /* minimal io size for this device */
    __le32 sector_size;

    /* type and info about this device */
    __le64 type;

    /* expected generation for this device */
    __le64 generation;

    /*
     * starting byte of this partition on the device,
     * to allowr for stripe alignment in the future
     */
    __le64 start_offset;

    /* grouping information for allocation decisions */
    __le32 dev_group;

    /* seek speed 0-100 where 100 is fastest */
    u8 seek_speed;

    /* bandwidth 0-100 where 100 is fastest */
    u8 bandwidth;

    /* btrfs generated uuid for this device */
    u8 uuid[BTRFS_UUID_SIZE];

    /* uuid of FS who owns this device */
    u8 fsid[BTRFS_UUID_SIZE];
} __attribute__ ((__packed__));

/*
 * the super block basically lists the main trees of the FS
 * it currently lacks any block count etc etc
 */
struct btrfs_super_block 
{
    u8 csum[BTRFS_CSUM_SIZE];
    /* the first 4 fields must match struct btrfs_header */
    u8 fsid[BTRFS_FSID_SIZE];    /* FS specific uuid */
    __le64 bytenr; /* this block number */
    __le64 flags;

    /* allowed to be different from the btrfs_header from here own down */
    __le64 magic;
    __le64 generation;
    __le64 root;
    __le64 chunk_root;
    __le64 log_root;

    /* this will help find the new super based on the log root */
    __le64 log_root_transid;
    __le64 total_bytes;
    __le64 bytes_used;
    __le64 root_dir_objectid;
    __le64 num_devices;
    __le32 sectorsize;
    __le32 nodesize;
    __le32 leafsize;
    __le32 stripesize;
    __le32 sys_chunk_array_size;
    __le64 chunk_root_generation;
    __le64 compat_flags;
    __le64 compat_ro_flags;
    __le64 incompat_flags;
    __le16 csum_type;
    u8 root_level;
    u8 chunk_root_level;
    u8 log_root_level;
    struct btrfs_dev_item dev_item;

    char label[BTRFS_LABEL_SIZE];

    /* future expansion */
    __le64 reserved[32];
    u8 sys_chunk_array[BTRFS_SYSTEM_CHUNK_ARRAY_SIZE];
} __attribute__ ((__packed__));

#endif // __FS_BTRFS_H__
