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

#ifndef __FS_REISERFS_H__
#define __FS_REISERFS_H__

struct s_dico;
struct s_strlist;

int reiserfs_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int reiserfs_getinfo(struct s_dico *d, char *devname);
int reiserfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int reiserfs_get_reqmntopt(char *partition, struct s_strlist *reqopt, struct s_strlist *badopt);
int reiserfs_umount(char *partition, char *mntbuf);
int reiserfs_test(char *devname);

#define REISERFS_SUPER_MAGIC_STRING    "ReIsErFs"
#define REISER2FS_SUPER_MAGIC_STRING    "ReIsEr2Fs"
#define REISER4FS_SUPER_MAGIC_STRING    "ReIsEr4"

#define REISERFS_DISK_OFFSET_IN_BYTES    (64 * 1024)

struct reiserfs_super_block_v1
{
  u32 s_block_count;            /* blocks count         */
  u32 s_free_blocks;                   /* free blocks count    */
  u32 s_root_block;                   /* root block number    */
  u32 s_journal_block;               /* journal block number    */
  u32 s_journal_dev;                   /* journal device number  */
  u32 s_orig_journal_size;         /* size of the journal on FS creation.  used to make sure they don't overflow it */
  u32 s_journal_trans_max ;               /* max number of blocks in a transaction.  */
  u32 s_journal_block_count ;         /* total size of the journal. can change over time  */
  u32 s_journal_max_batch ;           /* max number of blocks to batch into a trans */
  u32 s_journal_max_commit_age ;      /* in seconds, how old can an async commit be */
  u32 s_journal_max_trans_age ;       /* in seconds, how old can a transaction be */
  u16 s_blocksize;                       /* block size           */
  u16 s_oid_maxsize;            /* max size of object id array, see get_objectid() commentary  */
  u16 s_oid_cursize;            /* current size of object id array */
  u16 s_state;                           /* valid or error       */
  char s_magic[12];                     /* reiserfs magic string indicates that file system is reiserfs */
  u32 s_hash_function_code;        /* indicate, what hash fuction is being use to sort names in a directory*/
  u16 s_tree_height;                  /* height of disk tree */
  u16 s_bmap_nr;                      /* amount of bitmap blocks needed to address each block of file system */
  u16 s_reserved;
} __attribute__ ((__packed__));

struct reiserfs_super_block
{
    struct reiserfs_super_block_v1 s_v1;
    u32 s_inode_generation;
    u32 s_flags;        /* Right now used only by inode-attributes, if enabled */
    unsigned char s_uuid[16];    /* filesystem unique identifier */
    unsigned char s_label[16];    /* filesystem volume label */
    char s_unused[88];        /* padding and reserved */
};

#endif // __FS_REISERFS_H__
