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

#ifndef __FS_XFS_H__
#define __FS_XFS_H__

struct s_dico;
struct s_strlist;

/*
 * Super block
 * Fits into a sector-sized buffer at address 0 of each allocation group.
 * Only the first of these is ever updated except during growfs.
 */
#define XFS_SB_MAGIC        0x58465342  /* 'XFSB' */
#define XFS_SB_VERSION_1    1       /* 5.3, 6.0.1, 6.1 */
#define XFS_SB_VERSION_2    2       /* 6.2 - attributes */
#define XFS_SB_VERSION_3    3       /* 6.2 - new inode version */
#define XFS_SB_VERSION_4    4       /* 6.2+ - bitmask version */
#define XFS_SB_VERSION_5    5       /* CRC enabled filesystem */
#define XFS_SB_VERSION_NUMBITS      0x000f
#define XFS_SB_VERSION_ALLFBITS     0xfff0
#define XFS_SB_VERSION_ATTRBIT      0x0010
#define XFS_SB_VERSION_NLINKBIT     0x0020
#define XFS_SB_VERSION_QUOTABIT     0x0040
#define XFS_SB_VERSION_ALIGNBIT     0x0080
#define XFS_SB_VERSION_DALIGNBIT    0x0100
#define XFS_SB_VERSION_SHAREDBIT    0x0200
#define XFS_SB_VERSION_LOGV2BIT     0x0400
#define XFS_SB_VERSION_SECTORBIT    0x0800
#define XFS_SB_VERSION_EXTFLGBIT    0x1000
#define XFS_SB_VERSION_DIRV2BIT     0x2000
#define XFS_SB_VERSION_BORGBIT      0x4000  /* ASCII only case-insens. */
#define XFS_SB_VERSION_MOREBITSBIT  0x8000

int xfs_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int xfs_getinfo(struct s_dico *d, char *devname);
int xfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int xfs_get_reqmntopt(char *partition, struct s_strlist *reqopt, struct s_strlist *badopt);
int xfs_umount(char *partition, char *mntbuf);
int xfs_test(char *devname);
int xfs_check_compatibility(u64 compat, u64 ro_compat, u64 incompat, u64 log_incompat);

typedef uint32_t      xfs_agblock_t;  /* blockno in alloc. group */
typedef uint32_t      xfs_extlen_t;   /* extent length in blocks */
typedef uint32_t      xfs_agnumber_t; /* allocation group number */
typedef int32_t       xfs_extnum_t;   /* # of extents in a file */
typedef int16_t       xfs_aextnum_t;  /* # extents in an attribute fork */
typedef int64_t       xfs_fsize_t;    /* bytes in a file */
typedef uint64_t      xfs_ufsize_t;   /* unsigned bytes in a file */

typedef int32_t       xfs_suminfo_t;  /* type of bitmap summary info */
typedef int32_t       xfs_rtword_t;   /* word type for bitmap manipulations */
 
typedef int64_t       xfs_lsn_t;      /* log sequence number */
typedef int32_t       xfs_tid_t;      /* transaction identifier */

typedef uint32_t      xfs_dablk_t;    /* dir/attr block number (in file) */
typedef uint32_t      xfs_dahash_t;   /* dir/attr hash value */

typedef uint16_t      xfs_prid_t;     /* prid_t truncated to 16bits in XFS */

/*
 * These types are 64 bits on disk but are either 32 or 64 bits in memory.
 * Disk based types:
 */
typedef uint64_t      xfs_fsblock_t;  /* blockno in filesystem (agno|agbno) */
typedef uint64_t      xfs_rfsblock_t; /* blockno in filesystem (raw) */
typedef uint64_t      xfs_rtblock_t;  /* extent (block) in realtime area */
typedef uint64_t      xfs_dfiloff_t;  /* block number in a file */
typedef uint64_t      xfs_dfilblks_t; /* number of blocks in a file */

typedef __s64           xfs_off_t;      /* <file offset> type */
typedef __u64           xfs_ino_t;      /* <inode> type */
typedef __s64           xfs_daddr_t;    /* <disk address> type */
typedef char *          xfs_caddr_t;    /* <core address> type */
typedef __u32           xfs_dev_t;
typedef __u32           xfs_nlink_t;

/*
 * Superblock - in core version.  Must match the ondisk version below.
 * Must be padded to 64 bit alignment.
 */
struct xfs_sb
{
        uint32_t      sb_magicnum;    /* magic number == XFS_SB_MAGIC */
        uint32_t      sb_blocksize;   /* logical block size, bytes */
        xfs_rfsblock_t  sb_dblocks;     /* number of data blocks */
        xfs_rfsblock_t  sb_rblocks;     /* number of realtime blocks */
        xfs_rtblock_t   sb_rextents;    /* number of realtime extents */
        uuid_t          sb_uuid;        /* user-visible file system unique id */
        xfs_fsblock_t   sb_logstart;    /* starting block of log if internal */
        xfs_ino_t       sb_rootino;     /* root inode number */
        xfs_ino_t       sb_rbmino;      /* bitmap inode for realtime extents */
        xfs_ino_t       sb_rsumino;     /* summary inode for rt bitmap */
        xfs_agblock_t   sb_rextsize;    /* realtime extent size, blocks */
        xfs_agblock_t   sb_agblocks;    /* size of an allocation group */
        xfs_agnumber_t  sb_agcount;     /* number of allocation groups */
        xfs_extlen_t    sb_rbmblocks;   /* number of rt bitmap blocks */
        xfs_extlen_t    sb_logblocks;   /* number of log blocks */
        uint16_t      sb_versionnum;  /* header version == XFS_SB_VERSION */
        uint16_t      sb_sectsize;    /* volume sector size, bytes */
        uint16_t      sb_inodesize;   /* inode size, bytes */
        uint16_t      sb_inopblock;   /* inodes per block */
        char            sb_fname[12];   /* file system name */
        uint8_t       sb_blocklog;    /* log2 of sb_blocksize */
        uint8_t       sb_sectlog;     /* log2 of sb_sectsize */
        uint8_t       sb_inodelog;    /* log2 of sb_inodesize */
        uint8_t       sb_inopblog;    /* log2 of sb_inopblock */
        uint8_t       sb_agblklog;    /* log2 of sb_agblocks (rounded up) */
        uint8_t       sb_rextslog;    /* log2 of sb_rextents */
        uint8_t       sb_inprogress;  /* mkfs is in progress, don't mount */
        uint8_t       sb_imax_pct;    /* max % of fs for inode space */
                                        /* statistics */
        /*
         * These fields must remain contiguous.  If you really
         * want to change their layout, make sure you fix the
         * code in xfs_trans_apply_sb_deltas().
         */
        uint64_t      sb_icount;      /* allocated inodes */
        uint64_t      sb_ifree;       /* free inodes */
        uint64_t      sb_fdblocks;    /* free data blocks */
        uint64_t      sb_frextents;   /* free realtime extents */
        /*
         * End contiguous fields.
         */
        xfs_ino_t       sb_uquotino;    /* user quota inode */
        xfs_ino_t       sb_gquotino;    /* group quota inode */
        uint16_t      sb_qflags;      /* quota flags */
        uint8_t       sb_flags;       /* misc. flags */
        uint8_t       sb_shared_vn;   /* shared version number */
        xfs_extlen_t    sb_inoalignmt;  /* inode chunk alignment, fsblocks */
        uint32_t      sb_unit;        /* stripe or raid unit */
        uint32_t      sb_width;       /* stripe or raid width */
        uint8_t       sb_dirblklog;   /* log2 of dir block size (fsbs) */
        uint8_t       sb_logsectlog;  /* log2 of the log sector size */
        uint16_t      sb_logsectsize; /* sector size for the log, bytes */
        uint32_t      sb_logsunit;    /* stripe unit size for the log */
        uint32_t      sb_features2;   /* additional feature bits */

        /*
         * bad features2 field as a result of failing to pad the sb structure to
         * 64 bits. Some machines will be using this field for features2 bits.
         * Easiest just to mark it bad and not use it for anything else.
         *
         * This is not kept up to date in memory; it is always overwritten by
         * the value in sb_features2 when formatting the incore superblock to
         * the disk buffer.
         */
        uint32_t      sb_bad_features2;

        /* version 5 superblock fields start here */

        /* feature masks */
        uint32_t      sb_features_compat;
        uint32_t      sb_features_ro_compat;
        uint32_t      sb_features_incompat;
        uint32_t      sb_features_log_incompat;

        uint32_t      sb_crc;         /* superblock crc */
        xfs_extlen_t    sb_spino_align; /* sparse inode chunk alignment */

        xfs_ino_t       sb_pquotino;    /* project quota inode */
        xfs_lsn_t       sb_lsn;         /* last write sequence */
        uuid_t          sb_meta_uuid;   /* metadata file system unique id */

        /* must be padded to 64 bit alignment */
};

// XFS features used in XFS version 5 only
#define XFS_SB_FEAT_RO_COMPAT_FINOBT      (1 << 0)  /* free inode btree */
#define XFS_SB_FEAT_RO_COMPAT_RMAPBT      (1 << 1)  /* reverse map btree */
#define XFS_SB_FEAT_RO_COMPAT_REFLINK     (1 << 2)  /* reflinked files */
#define XFS_SB_FEAT_RO_COMPAT_INOBTCNT    (1 << 3)  /* inobt block counts */
#define XFS_SB_FEAT_INCOMPAT_FTYPE        (1 << 0)  /* filetype in dirent */
#define XFS_SB_FEAT_INCOMPAT_SPINODES     (1 << 1)  /* sparse inode chunks */
#define XFS_SB_FEAT_INCOMPAT_META_UUID    (1 << 2)  /* metadata UUID */
#define XFS_SB_FEAT_INCOMPAT_BIGTIME      (1 << 3)  /* large timestamps */
#define XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR  (1 << 4)  /* needs xfs_repair */
#define XFS_SB_FEAT_INCOMPAT_NREXT64      (1 << 5)  /* large extent counters */

// features supported by the current fsarchiver version
#define FSA_XFS_FEATURE_COMPAT_SUPP       (u64)(0)
#define FSA_XFS_FEATURE_RO_COMPAT_SUPP    (u64)(XFS_SB_FEAT_RO_COMPAT_FINOBT|\
                                                XFS_SB_FEAT_RO_COMPAT_RMAPBT|\
                                                XFS_SB_FEAT_RO_COMPAT_REFLINK|\
                                                XFS_SB_FEAT_RO_COMPAT_INOBTCNT)
#define FSA_XFS_FEATURE_INCOMPAT_SUPP     (u64)(XFS_SB_FEAT_INCOMPAT_FTYPE|\
                                                XFS_SB_FEAT_INCOMPAT_SPINODES|\
                                                XFS_SB_FEAT_INCOMPAT_META_UUID|\
                                                XFS_SB_FEAT_INCOMPAT_BIGTIME|\
                                                XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR|\
                                                XFS_SB_FEAT_INCOMPAT_NREXT64)
#define FSA_XFS_FEATURE_LOG_INCOMPAT_SUPP (u64)(0)

#endif // __FS_XFS_H__
