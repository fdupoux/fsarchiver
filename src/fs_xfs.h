/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2012 Francois Dupoux.  All rights reserved.
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

#define XFS_SUPER_MAGIC 0x58465342

int xfs_mkfs(struct s_dico *d, char *partition, char *fsoptions);
int xfs_getinfo(struct s_dico *d, char *devname);
int xfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int xfs_get_reqmntopt(char *partition, struct s_strlist *reqopt, struct s_strlist *badopt);
int xfs_umount(char *partition, char *mntbuf);
int xfs_test(char *devname);

typedef uint8_t __u8;
typedef int8_t __s8;
typedef uint16_t __u16;
typedef int16_t __s16;
typedef uint32_t __u32;
typedef int32_t __s32;
typedef uint64_t __u64;
typedef int64_t __s64;

typedef __uint32_t      xfs_agblock_t;  /* blockno in alloc. group */
typedef __uint32_t      xfs_extlen_t;   /* extent length in blocks */
typedef __uint32_t      xfs_agnumber_t; /* allocation group number */
typedef __int32_t       xfs_extnum_t;   /* # of extents in a file */
typedef __int16_t       xfs_aextnum_t;  /* # extents in an attribute fork */
typedef __int64_t       xfs_fsize_t;    /* bytes in a file */
typedef __uint64_t      xfs_ufsize_t;   /* unsigned bytes in a file */

typedef __int32_t       xfs_suminfo_t;  /* type of bitmap summary info */
typedef __int32_t       xfs_rtword_t;   /* word type for bitmap manipulations */
 
typedef __int64_t       xfs_lsn_t;      /* log sequence number */
typedef __int32_t       xfs_tid_t;      /* transaction identifier */

typedef __uint32_t      xfs_dablk_t;    /* dir/attr block number (in file) */
typedef __uint32_t      xfs_dahash_t;   /* dir/attr hash value */

typedef __uint16_t      xfs_prid_t;     /* prid_t truncated to 16bits in XFS */

/*
 * These types are 64 bits on disk but are either 32 or 64 bits in memory.
 * Disk based types:
 */
typedef __uint64_t      xfs_dfsbno_t;   /* blockno in filesystem (agno|agbno) */
typedef __uint64_t      xfs_drfsbno_t;  /* blockno in filesystem (raw) */
typedef __uint64_t      xfs_drtbno_t;   /* extent (block) in realtime area */
typedef __uint64_t      xfs_dfiloff_t;  /* block number in a file */
typedef __uint64_t      xfs_dfilblks_t; /* number of blocks in a file */

typedef __s64           xfs_off_t;      /* <file offset> type */
typedef __u64           xfs_ino_t;      /* <inode> type */
typedef __s64           xfs_daddr_t;    /* <disk address> type */
typedef char *          xfs_caddr_t;    /* <core address> type */
typedef __u32           xfs_dev_t;
typedef __u32           xfs_nlink_t;

typedef struct { unsigned char   __u_bits[16]; } xfs_uuid_t;


/*
 * Superblock - in core version.  Must match the ondisk version below.
 * Must be padded to 64 bit alignment.
 */
struct xfs_sb 
{
        __uint32_t      sb_magicnum;    /* magic number == XFS_SB_MAGIC */
        __uint32_t      sb_blocksize;   /* logical block size, bytes */
        xfs_drfsbno_t   sb_dblocks;     /* number of data blocks */
        xfs_drfsbno_t   sb_rblocks;     /* number of realtime blocks */
        xfs_drtbno_t    sb_rextents;    /* number of realtime extents */
        xfs_uuid_t      sb_uuid;        /* file system unique id */
        xfs_dfsbno_t    sb_logstart;    /* starting block of log if internal */
        xfs_ino_t       sb_rootino;     /* root inode number */
        xfs_ino_t       sb_rbmino;      /* bitmap inode for realtime extents */
        xfs_ino_t       sb_rsumino;     /* summary inode for rt bitmap */
        xfs_agblock_t   sb_rextsize;    /* realtime extent size, blocks */
        xfs_agblock_t   sb_agblocks;    /* size of an allocation group */
        xfs_agnumber_t  sb_agcount;     /* number of allocation groups */
        xfs_extlen_t    sb_rbmblocks;   /* number of rt bitmap blocks */
        xfs_extlen_t    sb_logblocks;   /* number of log blocks */
        __uint16_t      sb_versionnum;  /* header version == XFS_SB_VERSION */
        __uint16_t      sb_sectsize;    /* volume sector size, bytes */
        __uint16_t      sb_inodesize;   /* inode size, bytes */
        __uint16_t      sb_inopblock;   /* inodes per block */
        char            sb_fname[12];   /* file system name */
        __uint8_t       sb_blocklog;    /* log2 of sb_blocksize */
        __uint8_t       sb_sectlog;     /* log2 of sb_sectsize */
        __uint8_t       sb_inodelog;    /* log2 of sb_inodesize */
        __uint8_t       sb_inopblog;    /* log2 of sb_inopblock */
        __uint8_t       sb_agblklog;    /* log2 of sb_agblocks (rounded up) */
        __uint8_t       sb_rextslog;    /* log2 of sb_rextents */
        __uint8_t       sb_inprogress;  /* mkfs is in progress, don't mount */
        __uint8_t       sb_imax_pct;    /* max % of fs for inode space */
        /*
         * These fields must remain contiguous.  If you really
         * want to change their layout, make sure you fix the
         * code in xfs_trans_apply_sb_deltas().
         */
        __uint64_t      sb_icount;      /* allocated inodes */
        __uint64_t      sb_ifree;       /* free inodes */
        __uint64_t      sb_fdblocks;    /* free data blocks */
        __uint64_t      sb_frextents;   /* free realtime extents */
        /*
         * End contiguous fields.
         */
        xfs_ino_t       sb_uquotino;    /* user quota inode */
        xfs_ino_t       sb_gquotino;    /* group quota inode */
        __uint16_t      sb_qflags;      /* quota flags */
        __uint8_t       sb_flags;       /* misc. flags */
        __uint8_t       sb_shared_vn;   /* shared version number */
        xfs_extlen_t    sb_inoalignmt;  /* inode chunk alignment, fsblocks */
        __uint32_t      sb_unit;        /* stripe or raid unit */
        __uint32_t      sb_width;       /* stripe or raid width */
        __uint8_t       sb_dirblklog;   /* log2 of dir block size (fsbs) */
        __uint8_t       sb_logsectlog;  /* log2 of the log sector size */
        __uint16_t      sb_logsectsize; /* sector size for the log, bytes */
        __uint32_t      sb_logsunit;    /* stripe unit size for the log */
        __uint32_t      sb_features2;   /* additional feature bits */
        /*
         * bad features2 field as a result of failing to pad the sb
         * structure to 64 bits. Some machines will be using this field
         * for features2 bits. Easiest just to mark it bad and not use
         * it for anything else.
         */
        __uint32_t      sb_bad_features2;

        /* must be padded to 64 bit alignment */
};

#endif // __FS_XFS_H__
