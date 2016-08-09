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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <uuid.h>

#include "fsarchiver.h"
#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "fs_reiserfs.h"
#include "filesys.h"
#include "strlist.h"
#include "error.h"

int reiserfs_mkfs(cdico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    char command[2048];
    char buffer[2048];
    char options[2048];
    int exitst;
    u64 temp64;
    
    // ---- check mkreiserfs is available
    if (exec_command(command, sizeof(command), NULL, NULL, 0, NULL, 0, "mkreiserfs -V")!=0)
    {   errprintf("mkreiserfs not found. please install reiserfsprogs-3.6 on your system or check the PATH.\n");
        return -1;
    }
    
    // ---- set the advanced filesystem settings from the dico
    memset(options, 0, sizeof(options));

    strlcatf(options, sizeof(options), " %s ", fsoptions);

    if (strlen(mkfslabel) > 0)
        strlcatf(options, sizeof(options), " -l '%.16s' ", mkfslabel);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(options, sizeof(options), " -l '%.16s' ", buffer);
    
    if (dico_get_u64(d, 0, FSYSHEADKEY_FSREISERBLOCKSIZE, &temp64)==0)
        strlcatf(options, sizeof(options), " -b %ld ", (long)temp64);
    
    if (strlen(mkfsuuid) > 0)
        strlcatf(options, sizeof(options), " -u %s ", mkfsuuid);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSUUID, buffer, sizeof(buffer))==0 && strlen(buffer)==36)
        strlcatf(options, sizeof(options), " -u %s ", buffer);
    
    if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "mkreiserfs -f %s %s", partition, options)!=0 || exitst!=0)
    {   errprintf("command [%s] failed\n", command);
        return -1;
    }
    
    return 0;
}

int reiserfs_getinfo(cdico *d, char *devname)
{
    struct reiserfs_super_block sb;
    char uuid[512];
    u16 temp16;
    int ret=0;
    int fd=-1;
    int res;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
    {   ret=-1;
        errprintf("cannot open(%s, O_RDONLY)\n", devname);
        goto reiserfs_read_sb_return;
    }
    
    if (lseek(fd, REISERFS_DISK_OFFSET_IN_BYTES, SEEK_SET)!=REISERFS_DISK_OFFSET_IN_BYTES)
    {   ret=-2;
        errprintf("cannot lseek(fd, REISERFS_DISK_OFFSET_IN_BYTES, SEEK_SET) on %s\n", devname);
        goto reiserfs_read_sb_close;
    }
    
    res=read(fd, &sb, sizeof(sb));
    if (res!=sizeof(sb))
    {   ret=-3;
        errprintf("cannot read the reiserfs superblock on device [%s]\n", devname);
        goto reiserfs_read_sb_close;
    }
    
    if (strncmp(sb.s_v1.s_magic, REISERFS_SUPER_MAGIC_STRING, strlen(REISERFS_SUPER_MAGIC_STRING)) == 0)
        dico_add_string(d, 0, FSYSHEADKEY_FSVERSION, "reiserfs-3.5");
    else if (strncmp(sb.s_v1.s_magic, REISER2FS_SUPER_MAGIC_STRING, strlen(REISER2FS_SUPER_MAGIC_STRING)) == 0)
        dico_add_string(d, 0, FSYSHEADKEY_FSVERSION, "reiserfs-3.6");
    else
    {   ret=-4;
        errprintf("magic different from expectations superblock on %s: magic=[%s]\n", devname, sb.s_v1.s_magic);
        goto reiserfs_read_sb_close;
    }
    
    msgprintf(MSG_DEBUG1, "reiserfs_magic=[%s]\n", sb.s_v1.s_magic);
    
    // ---- label
    msgprintf(MSG_DEBUG1, "reiserfs_label=[%s]\n", sb.s_label);
    dico_add_string(d, 0, FSYSHEADKEY_FSLABEL, (char*)sb.s_label);
    
    // ---- uuid
    /*if ((str=e2p_uuid2str(sb.s_uuid))!=NULL)
        dico_add_string(d, 0, FSYSHEADKEY_FSUUID, str);*/
    memset(uuid, 0, sizeof(uuid));
    uuid_unparse_lower((u8*)sb.s_uuid, uuid);
    dico_add_string(d, 0, FSYSHEADKEY_FSUUID, uuid);
    msgprintf(MSG_DEBUG1, "reiserfs_uuid=[%s]\n", uuid);
    
    // ---- block size
    temp16=le16_to_cpu(sb.s_v1.s_blocksize);
    dico_add_u64(d, 0, FSYSHEADKEY_FSREISERBLOCKSIZE, temp16);
    msgprintf(MSG_DEBUG1, "reiserfs_blksize=[%ld]\n", (long)temp16);
    
    // ---- minimum fsarchiver version required to restore
    dico_add_u64(d, 0, FSYSHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 6, 4, 0));
    
reiserfs_read_sb_close:
    close(fd);
reiserfs_read_sb_return:
    return ret;
}

int reiserfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo)
{
    return generic_mount(partition, mntbuf, fsbuf, "user_xattr,acl", flags);
}

int reiserfs_umount(char *partition, char *mntbuf)
{
    return generic_umount(mntbuf);
}

int reiserfs_test(char *devname)
{
    struct reiserfs_super_block sb;
    int fd=-1;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
        return false;
    
    if (lseek(fd, REISERFS_DISK_OFFSET_IN_BYTES, SEEK_SET)!=REISERFS_DISK_OFFSET_IN_BYTES)
    {   close(fd);
        return false;
    }
    
    if (read(fd, &sb, sizeof(sb))!=sizeof(sb))
    {   close(fd);
        return false;
    }
    
    if ((strncmp(sb.s_v1.s_magic, REISERFS_SUPER_MAGIC_STRING, strlen(REISERFS_SUPER_MAGIC_STRING)) != 0)
        && (strncmp(sb.s_v1.s_magic, REISER2FS_SUPER_MAGIC_STRING, strlen(REISER2FS_SUPER_MAGIC_STRING)) != 0))
    {   close(fd);
        return false;
    }
    
    close(fd);
    return true;
}

int reiserfs_get_reqmntopt(char *partition, cstrlist *reqopt, cstrlist *badopt)
{
    if (!reqopt || !badopt)
        return -1;
    
    strlist_add(badopt, "nouser_xattr");
    strlist_add(badopt, "noacl");
    
    return 0;
}
