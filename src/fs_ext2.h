/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2018 Francois Dupoux.  All rights reserved.
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

#ifndef __FS_EXT2_H__
#define __FS_EXT2_H__

struct s_dico;
struct s_strlist;

enum {EXTFSTYPE_EXT2, EXTFSTYPE_EXT3, EXTFSTYPE_EXT4};

int ext2_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int ext3_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int ext4_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int extfs_getinfo(struct s_dico *d, char *devname);
int extfs_get_fstype_from_compat_flags(u32 compat, u32 incompat, u32 ro_compat);
int extfs_get_reqmntopt(char *partition, struct s_strlist *reqopt, struct s_strlist *badopt);
int extfs_mkfs(struct s_dico *d, char *partition, int extfstype, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int extfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int extfs_umount(char *partition, char *mntbuf);
int extfs_test(char *partition, int extfstype);
int ext2_test(char *partition);
int ext3_test(char *partition);
int ext4_test(char *partition);
u64 check_prog_version(char *prog);

/* for s_feature_compat */
#define FSA_EXT2_FEATURE_COMPAT_DIR_PREALLOC       0x0001
#define FSA_EXT2_FEATURE_COMPAT_IMAGIC_INODES      0x0002
#define FSA_EXT3_FEATURE_COMPAT_HAS_JOURNAL        0x0004
#define FSA_EXT2_FEATURE_COMPAT_EXT_ATTR           0x0008
#define FSA_EXT2_FEATURE_COMPAT_RESIZE_INODE       0x0010
#define FSA_EXT2_FEATURE_COMPAT_DIR_INDEX          0x0020
#define FSA_EXT2_FEATURE_COMPAT_LAZY_BG            0x0040
#define FSA_EXT4_FEATURE_COMPAT_SPARSE_SUPER2      0x0200
#define FSA_EXT4_FEATURE_COMPAT_FAST_COMMIT        0x0400
#define FSA_EXT4_FEATURE_COMPAT_STABLE_INODES      0x0800
#define FSA_EXT4_FEATURE_COMPAT_ORPHAN_FILE        0x1000

/* for s_feature_ro_compat */
#define FSA_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER    0x0001
#define FSA_EXT2_FEATURE_RO_COMPAT_LARGE_FILE      0x0002
#define FSA_EXT2_FEATURE_RO_COMPAT_BTREE_DIR       0x0004
#define FSA_EXT4_FEATURE_RO_COMPAT_HUGE_FILE       0x0008
#define FSA_EXT4_FEATURE_RO_COMPAT_GDT_CSUM        0x0010
#define FSA_EXT4_FEATURE_RO_COMPAT_DIR_NLINK       0x0020
#define FSA_EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE     0x0040
#define FSA_EXT4_FEATURE_RO_COMPAT_QUOTA           0x0100
#define FSA_EXT4_FEATURE_RO_COMPAT_BIGALLOC        0x0200
#define FSA_EXT4_FEATURE_RO_COMPAT_METADATA_CSUM   0x0400
#define FSA_EXT4_FEATURE_RO_COMPAT_READONLY        0x1000
#define FSA_EXT4_FEATURE_RO_COMPAT_PROJECT         0x2000
#define FSA_EXT4_FEATURE_RO_COMPAT_ORPHAN_PRESENT  0x10000

/* for s_feature_incompat */
#define FSA_EXT2_FEATURE_INCOMPAT_FILETYPE         0x0002
#define FSA_EXT3_FEATURE_INCOMPAT_RECOVER          0x0004
#define FSA_EXT3_FEATURE_INCOMPAT_JOURNAL_DEV      0x0008
#define FSA_EXT2_FEATURE_INCOMPAT_META_BG          0x0010
#define FSA_EXT4_FEATURE_INCOMPAT_EXTENTS          0x0040
#define FSA_EXT4_FEATURE_INCOMPAT_64BIT            0x0080
#define FSA_EXT4_FEATURE_INCOMPAT_MMP              0x0100
#define FSA_EXT4_FEATURE_INCOMPAT_FLEX_BG          0x0200
#define FSA_EXT4_FEATURE_INCOMPAT_EA_INODE         0x0400
#define FSA_EXT4_FEATURE_INCOMPAT_DIRDATA          0x1000
#define FSA_EXT4_FEATURE_INCOMPAT_CSUM_SEED        0x2000
#define FSA_EXT4_FEATURE_INCOMPAT_LARGEDIR         0x4000
#define FSA_EXT4_FEATURE_INCOMPAT_INLINEDATA       0x8000
#define FSA_EXT4_FEATURE_INCOMPAT_ENCRYPT          0x10000
#define FSA_EXT4_FEATURE_INCOMPAT_CASEFOLD         0x20000

#define FSA_EXT2_FEATURE_COMPAT_SUPP               FSA_EXT2_FEATURE_COMPAT_EXT_ATTR
#define FSA_EXT2_FEATURE_INCOMPAT_SUPP             (FSA_EXT2_FEATURE_INCOMPAT_FILETYPE| \
                                                    FSA_EXT2_FEATURE_INCOMPAT_META_BG)
#define FSA_EXT2_FEATURE_RO_COMPAT_SUPP            (FSA_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
                                                    FSA_EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
                                                    FSA_EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define FSA_EXT2_FEATURE_INCOMPAT_UNSUPPORTED      ~FSA_EXT2_FEATURE_INCOMPAT_SUPP
#define FSA_EXT2_FEATURE_RO_COMPAT_UNSUPPORTED     ~FSA_EXT2_FEATURE_RO_COMPAT_SUPP

#define FSA_EXT3_FEATURE_COMPAT_SUPP               FSA_EXT2_FEATURE_COMPAT_EXT_ATTR
#define FSA_EXT3_FEATURE_INCOMPAT_SUPP             (FSA_EXT2_FEATURE_INCOMPAT_FILETYPE| \
                                                    FSA_EXT3_FEATURE_INCOMPAT_RECOVER| \
                                                    FSA_EXT2_FEATURE_INCOMPAT_META_BG)
#define FSA_EXT3_FEATURE_RO_COMPAT_SUPP            (FSA_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
                                                    FSA_EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
                                                    FSA_EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define FSA_EXT3_FEATURE_INCOMPAT_UNSUPPORTED      ~FSA_EXT3_FEATURE_INCOMPAT_SUPP
#define FSA_EXT3_FEATURE_RO_COMPAT_UNSUPPORTED     ~FSA_EXT3_FEATURE_RO_COMPAT_SUPP

// -------------- features supported by the current fsarchiver version --------------------
#define FSA_FEATURE_COMPAT_SUPP                    (u64)(FSA_EXT2_FEATURE_COMPAT_DIR_PREALLOC|\
                                                         FSA_EXT2_FEATURE_COMPAT_IMAGIC_INODES|\
                                                         FSA_EXT3_FEATURE_COMPAT_HAS_JOURNAL|\
                                                         FSA_EXT2_FEATURE_COMPAT_EXT_ATTR|\
                                                         FSA_EXT2_FEATURE_COMPAT_RESIZE_INODE|\
                                                         FSA_EXT2_FEATURE_COMPAT_DIR_INDEX|\
                                                         FSA_EXT2_FEATURE_COMPAT_LAZY_BG|\
                                                         FSA_EXT4_FEATURE_COMPAT_SPARSE_SUPER2|\
                                                         FSA_EXT4_FEATURE_COMPAT_FAST_COMMIT|\
                                                         FSA_EXT4_FEATURE_COMPAT_ORPHAN_FILE)
#define FSA_FEATURE_RO_COMPAT_SUPP                 (u64)(FSA_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER|\
                                                         FSA_EXT2_FEATURE_RO_COMPAT_LARGE_FILE|\
                                                         FSA_EXT2_FEATURE_RO_COMPAT_BTREE_DIR|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_HUGE_FILE|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_GDT_CSUM|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_DIR_NLINK|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_QUOTA|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_BIGALLOC|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_METADATA_CSUM|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_READONLY|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_PROJECT|\
                                                         FSA_EXT4_FEATURE_RO_COMPAT_ORPHAN_PRESENT)
#define FSA_FEATURE_INCOMPAT_SUPP                  (u64)(FSA_EXT2_FEATURE_INCOMPAT_FILETYPE|\
                                                         FSA_EXT3_FEATURE_INCOMPAT_RECOVER|\
                                                         FSA_EXT3_FEATURE_INCOMPAT_JOURNAL_DEV|\
                                                         FSA_EXT2_FEATURE_INCOMPAT_META_BG|\
                                                         FSA_EXT4_FEATURE_INCOMPAT_EXTENTS|\
                                                         FSA_EXT4_FEATURE_INCOMPAT_64BIT|\
                                                         FSA_EXT4_FEATURE_INCOMPAT_MMP|\
                                                         FSA_EXT4_FEATURE_INCOMPAT_FLEX_BG|\
                                                         FSA_EXT4_FEATURE_INCOMPAT_EA_INODE|\
                                                         FSA_EXT4_FEATURE_INCOMPAT_DIRDATA|\
                                                         FSA_EXT4_FEATURE_INCOMPAT_CSUM_SEED|\
                                                         FSA_EXT4_FEATURE_INCOMPAT_LARGEDIR|\
                                                         FSA_EXT4_FEATURE_INCOMPAT_INLINEDATA)

// FSA_FEATURE_INCOMPAT_SUPP does not include:
// - FSA_EXT4_FEATURE_INCOMPAT_ENCRYPT as it would require special handling to mount an encrypted filesystem
// - FSA_EXT4_FEATURE_INCOMPAT_CASEFOLD as it would require special handling to recreate the filesystem with
//   proper "encoding" and "encoding_flags" mke2fs options
//
// FSA_FEATURE_COMPAT_SUPP does not include:
// - FSA_EXT4_FEATURE_COMPAT_STABLE_INODES because it is used mainly with encryption

// old e2fsprogs compatibility glue
#ifndef EXT2_FLAG_SOFTSUPP_FEATURES
#define EXT2_FLAG_SOFTSUPP_FEATURES                0x8000
#endif
#ifndef EXT4_FEATURE_INCOMPAT_FLEX_BG
#define EXT4_FEATURE_INCOMPAT_FLEX_BG              0x0200
#endif
#ifndef EXT4_FEATURE_RO_COMPAT_HUGE_FILE
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE           0x0008
#endif
#ifndef EXT4_FEATURE_RO_COMPAT_GDT_CSUM
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM            0x0010
#endif
#ifndef EXT4_FEATURE_RO_COMPAT_DIR_NLINK
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK           0x0020
#endif
#ifndef EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE         0x0040
#endif

// this struct is incomplete in e2fsprogs-1.39.x/ext2fs.h
struct fsa_ext2_sb
{
    u32   s_inodes_count;         /* Inodes count */
    u32   s_blocks_count;         /* Blocks count */
    u32   s_r_blocks_count;       /* Reserved blocks count */
    u32   s_free_blocks_count;    /* Free blocks count */
    u32   s_free_inodes_count;    /* Free inodes count */
    u32   s_first_data_block;     /* First Data Block */
    u32   s_log_block_size;       /* Block size */
    s32   s_log_frag_size;        /* Fragment size */
    u32   s_blocks_per_group;     /* # Blocks per group */
    u32   s_frags_per_group;      /* # Fragments per group */
    u32   s_inodes_per_group;     /* # Inodes per group */
    u32   s_mtime;                /* Mount time */
    u32   s_wtime;                /* Write time */
    u16   s_mnt_count;            /* Mount count */
    s16   s_max_mnt_count;        /* Maximal mount count */
    u16   s_magic;                /* Magic signature */
    u16   s_state;                /* File system state */
    u16   s_errors;               /* Behaviour when detecting errors */
    u16   s_minor_rev_level;      /* minor revision level */
    u32   s_lastcheck;            /* time of last check */
    u32   s_checkinterval;        /* max. time between checks */
    u32   s_creator_os;           /* OS */
    u32   s_rev_level;            /* Revision level */
    u16   s_def_resuid;           /* Default uid for reserved blocks */
    u16   s_def_resgid;           /* Default gid for reserved blocks */
    /*
     * These fields are for EXT2_DYNAMIC_REV superblocks only.
     *
     * Note: the difference between the compatible feature set and
     * the incompatible feature set is that if there is a bit set
     * in the incompatible feature set that the kernel doesn't
     * know about, it should refuse to mount the filesystem.
     *
     * e2fsck's requirements are more strict; if it doesn't know
     * about a feature in either the compatible or incompatible
     * feature set, it must abort and not try to meddle with
     * things it doesn't understand...
     */
    u32   s_first_ino;            /* First non-reserved inode */
    u16   s_inode_size;           /* size of inode structure */
    u16   s_block_group_nr;       /* block group # of this superblock */
    u32   s_feature_compat;       /* compatible feature set */
    u32   s_feature_incompat;     /* incompatible feature set */
    u32   s_feature_ro_compat;    /* readonly-compatible feature set */
    u8    s_uuid[16];             /* 128-bit uuid for volume */
    char  s_volume_name[16];      /* volume name */
    char  s_last_mounted[64];     /* directory where last mounted */
    u32   s_algorithm_usage_bitmap; /* For compression */
    /*
     * Performance hints.  Directory preallocation should only
     * happen if the EXT2_FEATURE_COMPAT_DIR_PREALLOC flag is on.
     */
    u8    s_prealloc_blocks;      /* Nr of blocks to try to preallocate*/
    u8    s_prealloc_dir_blocks;  /* Nr to preallocate for dirs */
    u16   s_reserved_gdt_blocks;  /* Per group table for online growth */
    /*
     * Journaling support valid if EXT2_FEATURE_COMPAT_HAS_JOURNAL set.
     */
    u8    s_journal_uuid[16];     /* uuid of journal superblock */
    u32   s_journal_inum;         /* inode number of journal file */
    u32   s_journal_dev;          /* device number of journal file */
    u32   s_last_orphan;          /* start of list of inodes to delete */
    u32   s_hash_seed[4];         /* HTREE hash seed */
    u8    s_def_hash_version;     /* Default hash version to use */
    u8    s_jnl_backup_type;      /* Default type of journal backup */
    u16   s_desc_size;            /* Group desc. size: INCOMPAT_64BIT */
    u32   s_default_mount_opts;
    u32   s_first_meta_bg;        /* First metablock group */
    u32   s_mkfs_time;            /* When the filesystem was created */
    u32   s_jnl_blocks[17];       /* Backup of the journal inode */
    u32   s_blocks_count_hi;      /* Blocks count high 32bits */
    u32   s_r_blocks_count_hi;    /* Reserved blocks count high 32 bits*/
    u32   s_free_blocks_hi;       /* Free blocks count */
    u16   s_min_extra_isize;      /* All inodes have at least # bytes */
    u16   s_want_extra_isize;     /* New inodes should reserve # bytes */
    u32   s_flags;                /* Miscellaneous flags */
    u16   s_raid_stride;          /* RAID stride */
    u16   s_mmp_interval;         /* # seconds to wait in MMP checking */
    u64   s_mmp_block;            /* Block for multi-mount protection */
    u32   s_raid_stripe_width;    /* blocks on all data disks (N*stride)*/
    u8    s_log_groups_per_flex;  /* FLEX_BG group size */
    u8    s_reserved_char_pad;
    u16   s_reserved_pad;         /* Padding to next 32bits */
    u32   s_reserved[162];        /* Padding to the end of the block */
};

#endif // __FS_EXT2_H__
