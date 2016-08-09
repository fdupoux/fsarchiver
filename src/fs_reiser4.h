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

#ifndef __FS_REISER4_H__
#define __FS_REISER4_H__

struct s_dico;
struct s_strlist;

int reiser4_mkfs(struct s_dico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid);
int reiser4_getinfo(struct s_dico *d, char *devname);
int reiser4_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo);
int reiser4_get_reqmntopt(char *partition, struct s_strlist *reqopt, struct s_strlist *badopt);
int reiser4_umount(char *partition, char *mntbuf);
int reiser4_test(char *devname);

#define REISERFS4_SUPER_MAGIC        "ReIsEr4"
#define MAGIC_SIZE 16
#define REISER4_DISK_OFFSET_IN_BYTES    (64 * 1024)

typedef struct reiser4_master_sb reiser4_master_sb;

struct reiser4_master_sb
{
    char magic[16];        // "ReIsEr4"
    u16 disk_plugin_id;    // id of disk layout plugin
    u16 blocksize;
    char uuid[16];        // unique id
    char label[16];        // filesystem label
    u64 diskmap;        // location of the diskmap. 0 if not present
} __attribute__ ((__packed__));

/*struct format40_super
{
    u64 sb_block_count;
    u64 sb_free_blocks;
    u64 sb_root_block;
    u64 sb_oid[2];
    u64 sb_flushes;
    u32 sb_mkfs_id;
    char sb_magic[MAGIC_SIZE];
    u16 sb_tree_height;
    u16 sb_policy;
    u64 sb_flags;
    char sb_unused[432];
} __attribute__((packed));*/

#endif // __FS_REISER4_H__
