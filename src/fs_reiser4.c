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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <uuid.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "fs_reiser4.h"
#include "filesys.h"
#include "strlist.h"
#include "error.h"

int reiser4_mkfs(cdico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    char command[2048];
    char buffer[2048];
    char options[2048];
    int exitst;
    u64 temp64;
    
    // ---- check mkfs.reiser4 is available
    if (exec_command(command, sizeof(command), NULL, NULL, 0, NULL, 0, "mkfs.reiser4 -V")!=0)
    {   errprintf("mkfs.reiser4 not found. please install reiser4progs on your system or check the PATH.\n");
        return -1;
    }
    
    // ---- set the advanced filesystem settings from the dico
    memset(options, 0, sizeof(options));

    strlcatf(options, sizeof(options), " %s ", fsoptions);

    if (strlen(mkfslabel) > 0)
        strlcatf(options, sizeof(options), " -L '%.16s' ", mkfslabel);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(options, sizeof(options), " -L '%.16s' ", buffer);
    
    if (dico_get_u64(d, 0, FSYSHEADKEY_FSREISER4BLOCKSIZE, &temp64)==0)
        strlcatf(options, sizeof(options), " -b %ld ", (long)temp64);
    
    if (strlen(mkfsuuid) > 0)
        strlcatf(options, sizeof(options), " -U %s ", mkfsuuid);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSUUID, buffer, sizeof(buffer))==0 && strlen(buffer)==36)
        strlcatf(options, sizeof(options), " -U %s ", buffer);
    
    if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "mkfs.reiser4 -y %s %s", partition, options)!=0 || exitst!=0)
    {   errprintf("command [%s] failed\n", command);
        return -1;
    }
    
    return 0;    
}

int reiser4_getinfo(cdico *d, char *devname)
{
    struct reiser4_master_sb sb;
    char uuid[512];
    u16 temp16;
    int ret=0;
    int fd;
    int res;

    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
    {   ret=-1;
        errprintf("cannot open(%s, O_RDONLY)\n", devname);
        goto reiser4_get_specific_return;
    }

    if (lseek(fd, REISER4_DISK_OFFSET_IN_BYTES, SEEK_SET)!=REISER4_DISK_OFFSET_IN_BYTES)
    {   ret=-2;
        errprintf("cannot lseek(fd, REISER4_DISK_OFFSET_IN_BYTES, SEEK_SET) on %s\n", devname);
        goto reiser4_get_specific_close;
    }

    res=read(fd, &sb, sizeof(sb));
    if (res!=sizeof(sb))
    {   ret=-3;
        errprintf("cannot read the reiser4 superblock on device [%s]\n", devname);
        goto reiser4_get_specific_close;
    }
    
    if (strncmp(sb.magic, REISERFS4_SUPER_MAGIC, strlen(REISERFS4_SUPER_MAGIC)) == 0)
    {
        dico_add_string(d, 0, FSYSHEADKEY_FSVERSION, "reiserfs-4.0");
    }
    else
    {   ret=-4;
        errprintf("magic different from expectations superblock on %s: magic=[%s], expected=[%s]\n", devname, sb.magic, REISERFS4_SUPER_MAGIC);
        goto reiser4_get_specific_close;
    }
    msgprintf(MSG_DEBUG1, "reiser4_magic=[%s]\n", sb.magic);
    
    // ---- label
    msgprintf(MSG_DEBUG1, "reiser4_label=[%s]\n", sb.label);
    dico_add_string(d, 0, FSYSHEADKEY_FSLABEL, (char*)sb.label);
    
    // ---- uuid
    /*if ((str=e2p_uuid2str(sb.uuid))!=NULL)
        dico_add_string(d, 0, FSYSHEADKEY_FSUUID, str);*/
    memset(uuid, 0, sizeof(uuid));
    uuid_unparse_lower((u8*)sb.uuid, uuid);
    dico_add_string(d, 0, FSYSHEADKEY_FSUUID, uuid);
    msgprintf(MSG_DEBUG1, "reiser4_uuid=[%s]\n", uuid);
    
    // ---- block size
    temp16=le16_to_cpu(sb.blocksize);
    if (temp16!=4096)
    {   ret=-5;
        errprintf("invalid reiser4 block-size: %ld, it should be 4096\n", (long)temp16);
        goto reiser4_get_specific_close;
    }
    else
    {   dico_add_u64(d, 0, FSYSHEADKEY_FSREISER4BLOCKSIZE, temp16);
        msgprintf(3, "reiser4_blksize=[%ld]\n", (long)temp16);
    }
    
    // ---- minimum fsarchiver version required to restore
    dico_add_u64(d, 0, FSYSHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 6, 4, 0));
    
reiser4_get_specific_close:
    close(fd);
reiser4_get_specific_return:
    return ret;
}

int reiser4_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo)
{
    return generic_mount(partition, mntbuf, fsbuf, NULL, flags);
}

int reiser4_umount(char *partition, char *mntbuf)
{
    return generic_umount(mntbuf);
}

int reiser4_test(char *devname)
{
    struct reiser4_master_sb sb;
    int fd;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
        return false;
    
    if (lseek(fd, REISER4_DISK_OFFSET_IN_BYTES, SEEK_SET)!=REISER4_DISK_OFFSET_IN_BYTES)
    {   close(fd);
        return false;
    }
    
    if (read(fd, &sb, sizeof(sb))!=sizeof(sb))
    {   close(fd);
        return false;
    }
    
    if (strncmp(sb.magic, REISERFS4_SUPER_MAGIC, strlen(REISERFS4_SUPER_MAGIC)) != 0)
    {   close(fd);
        return false;
    }
    
    close(fd);
    return true;
}

int reiser4_get_reqmntopt(char *partition, cstrlist *reqopt, cstrlist *badopt)
{
    if (!reqopt || !badopt)
        return -1;

    return 0;
}
