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

#ifndef __FS_EXT2_H__
#define __FS_EXT2_H__

#include "dico.h"
#include "strlist.h"

enum {EXTFSTYPE_EXT2, EXTFSTYPE_EXT3, EXTFSTYPE_EXT4};

int ext2_mkfs(cdico *d, char *partition);
int ext3_mkfs(cdico *d, char *partition);
int ext4_mkfs(cdico *d, char *partition);
int extfs_getinfo(cdico *d, char *devname);
int extfs_get_fstype_from_compat_flags(u32 compat, u32 incompat, u32 ro_compat);
int extfs_get_reqmntopt(char *partition, cstrlist *reqopt, cstrlist *badopt);
int extfs_mkfs(cdico *d, char *partition, int extfstype);
int extfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int extfs_umount(char *partition, char *mntbuf);
int extfs_test(char *partition, int extfstype);
int ext2_test(char *partition);
int ext3_test(char *partition);
int ext4_test(char *partition);

/* for s_feature_compat */
#define FSA_EXT2_FEATURE_COMPAT_DIR_PREALLOC        0x0001
#define FSA_EXT2_FEATURE_COMPAT_IMAGIC_INODES        0x0002
#define FSA_EXT3_FEATURE_COMPAT_HAS_JOURNAL        0x0004
#define FSA_EXT2_FEATURE_COMPAT_EXT_ATTR        0x0008
#define FSA_EXT2_FEATURE_COMPAT_RESIZE_INODE        0x0010
#define FSA_EXT2_FEATURE_COMPAT_DIR_INDEX        0x0020
#define FSA_EXT2_FEATURE_COMPAT_LAZY_BG            0x0040

/* for s_feature_ro_compat */
#define FSA_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER        0x0001
#define FSA_EXT2_FEATURE_RO_COMPAT_LARGE_FILE        0x0002
#define FSA_EXT2_FEATURE_RO_COMPAT_BTREE_DIR        0x0004
#define FSA_EXT4_FEATURE_RO_COMPAT_HUGE_FILE        0x0008
#define FSA_EXT4_FEATURE_RO_COMPAT_GDT_CSUM        0x0010
#define FSA_EXT4_FEATURE_RO_COMPAT_DIR_NLINK        0x0020
#define FSA_EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE        0x0040

/* for s_feature_incompat */
#define FSA_EXT2_FEATURE_INCOMPAT_FILETYPE        0x0002
#define FSA_EXT3_FEATURE_INCOMPAT_RECOVER        0x0004
#define FSA_EXT3_FEATURE_INCOMPAT_JOURNAL_DEV        0x0008
#define FSA_EXT2_FEATURE_INCOMPAT_META_BG        0x0010
#define FSA_EXT4_FEATURE_INCOMPAT_EXTENTS        0x0040
#define FSA_EXT4_FEATURE_INCOMPAT_64BIT            0x0080
#define FSA_EXT4_FEATURE_INCOMPAT_MMP            0x0100
#define FSA_EXT4_FEATURE_INCOMPAT_FLEX_BG        0x0200

#define FSA_EXT2_FEATURE_RO_COMPAT_SUPP            (FSA_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
                            FSA_EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
                            FSA_EXT2_FEATURE_RO_COMPAT_BTREE_DIR)

#define FSA_EXT2_FEATURE_INCOMPAT_SUPP            (FSA_EXT2_FEATURE_INCOMPAT_FILETYPE| \
                            FSA_EXT2_FEATURE_INCOMPAT_META_BG)

#define FSA_EXT2_FEATURE_INCOMPAT_UNSUPPORTED        ~FSA_EXT2_FEATURE_INCOMPAT_SUPP
#define FSA_EXT2_FEATURE_RO_COMPAT_UNSUPPORTED        ~FSA_EXT2_FEATURE_RO_COMPAT_SUPP

#define FSA_EXT3_FEATURE_RO_COMPAT_SUPP            (FSA_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
                            FSA_EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
                            FSA_EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define FSA_EXT3_FEATURE_INCOMPAT_SUPP            (FSA_EXT2_FEATURE_INCOMPAT_FILETYPE| \
                            FSA_EXT3_FEATURE_INCOMPAT_RECOVER| \
                            FSA_EXT2_FEATURE_INCOMPAT_META_BG)
#define FSA_EXT3_FEATURE_INCOMPAT_UNSUPPORTED        ~FSA_EXT3_FEATURE_INCOMPAT_SUPP
#define FSA_EXT3_FEATURE_RO_COMPAT_UNSUPPORTED        ~FSA_EXT3_FEATURE_RO_COMPAT_SUPP

// -------------- features supported by the current fsarchiver version --------------------
#define FSA_FEATURE_COMPAT_SUPP                (u64)(FSA_EXT2_FEATURE_COMPAT_DIR_PREALLOC| \
                            FSA_EXT2_FEATURE_COMPAT_IMAGIC_INODES| \
                            FSA_EXT3_FEATURE_COMPAT_HAS_JOURNAL| \
                            FSA_EXT2_FEATURE_COMPAT_EXT_ATTR| \
                            FSA_EXT2_FEATURE_COMPAT_RESIZE_INODE| \
                            FSA_EXT2_FEATURE_COMPAT_DIR_INDEX| \
                            FSA_EXT2_FEATURE_COMPAT_LAZY_BG)
#define FSA_FEATURE_INCOMPAT_SUPP            (u64)(FSA_EXT2_FEATURE_INCOMPAT_FILETYPE|\
                            FSA_EXT3_FEATURE_INCOMPAT_JOURNAL_DEV|\
                            FSA_EXT2_FEATURE_INCOMPAT_META_BG|\
                            FSA_EXT3_FEATURE_INCOMPAT_RECOVER|\
                            FSA_EXT4_FEATURE_INCOMPAT_EXTENTS|\
                            FSA_EXT4_FEATURE_INCOMPAT_FLEX_BG)
#define FSA_FEATURE_RO_COMPAT_SUPP            (u64)(FSA_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER|\
                            FSA_EXT4_FEATURE_RO_COMPAT_HUGE_FILE|\
                            FSA_EXT2_FEATURE_RO_COMPAT_LARGE_FILE|\
                            FSA_EXT4_FEATURE_RO_COMPAT_DIR_NLINK|\
                            FSA_EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE|\
                            FSA_EXT4_FEATURE_RO_COMPAT_GDT_CSUM)

// EXT2_FLAG_SOFTSUPP_FEATURES not defined on old e2fsprogs versions
#ifndef EXT2_FLAG_SOFTSUPP_FEATURES
#define EXT2_FLAG_SOFTSUPP_FEATURES     0x8000
#endif 

#endif // __FS_EXT2_H__
