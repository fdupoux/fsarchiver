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
#include "fs_jfs.h"
#include "filesys.h"
#include "strlist.h"
#include "error.h"

int jfs_mkfs(cdico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    char command[2048];
    char buffer[2048];
    char options[2048];
    int exitst;
    
    // ---- check mkfs.reiser4 is available
    if (exec_command(command, sizeof(command), NULL, NULL, 0, NULL, 0, "mkfs.jfs -V")!=0)
    {   errprintf("mkfs.jfs not found. please install jfsutils on your system or check the PATH.\n");
        return -1;
    }
    
    // ---- set the advanced filesystem settings from the dico
    memset(options, 0, sizeof(options));

    strlcatf(options, sizeof(options), " %s ", fsoptions);

    if (strlen(mkfslabel) > 0)
        strlcatf(options, sizeof(options), " -L '%s' ", mkfslabel);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(options, sizeof(options), " -L '%s' ", buffer);
    
    if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "mkfs.jfs -q %s %s", options, partition)!=0 || exitst!=0)
    {   errprintf("command [%s] failed\n", command);
        return -1;
    }
    
    // ---- use jfs_tune to set the other advanced options
    memset(options, 0, sizeof(options));
    if (strlen(mkfsuuid) > 0)
        strlcatf(options, sizeof(options), " -U %s ", mkfsuuid);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSUUID, buffer, sizeof(buffer))==0 && strlen(buffer)==36)
        strlcatf(options, sizeof(options), " -U %s ", buffer);
    
    if (options[0])
    {
        if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "jfs_tune %s %s", partition, options)!=0 || exitst!=0)
        {   errprintf("command [%s] failed\n", command);
            return -1;
        }
    }
    
    return 0;
}

int jfs_getinfo(cdico *d, char *devname)
{
    struct jfs_superblock sb;
    char uuid[512];
    int ret=0;
    int fd;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
    {   ret=-1;
        errprintf("cannot open(%s, O_RDONLY)\n", devname);
        goto jfs_getinfo_return;
    }
    
    if (lseek(fd, JFS_SUPER1_OFF, SEEK_SET)!=JFS_SUPER1_OFF)
    {   ret=-2;
        errprintf("cannot lseek(fd, JFS_SUPER1_OFF, SEEK_SET) on %s\n", devname);
        goto jfs_getinfo_close;
    }
    
    if (read(fd, &sb, sizeof(sb))!=sizeof(sb))
    {   ret=-3;
        errprintf("cannot read the jfs superblock on [%s]\n", devname);
        goto jfs_getinfo_close;
    }
    
    if (strncmp(sb.s_magic, JFS_MAGIC, strlen(JFS_MAGIC)) != 0)
    {   ret=-4;
        errprintf("magic different from expectations superblock on %s: magic=[%s], expected=[%s]\n", devname, sb.s_magic, JFS_MAGIC);
        goto jfs_getinfo_close;
    }
    msgprintf(MSG_DEBUG1, "jfs_magic=[%s]\n", sb.s_magic);
    
    // ---- label
    msgprintf(MSG_DEBUG1, "jfs_label=[%s]\n", sb.s_label);
    dico_add_string(d, 0, FSYSHEADKEY_FSLABEL, (char*)sb.s_label);
    
    // ---- uuid
    /*if ((str=e2p_uuid2str(sb.s_uuid))!=NULL)
        dico_add_string(d, 0, FSYSHEADKEY_FSUUID, str);*/
    memset(uuid, 0, sizeof(uuid));
    uuid_unparse_lower((u8*)sb.s_uuid, uuid);
    dico_add_string(d, 0, FSYSHEADKEY_FSUUID, uuid);
    msgprintf(MSG_DEBUG1, "jfs_uuid=[%s]\n", uuid);
    
    // ---- minimum fsarchiver version required to restore
    dico_add_u64(d, 0, FSYSHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 6, 4, 0));
    
jfs_getinfo_close:
    close(fd);
jfs_getinfo_return:
    return ret;
}

int jfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo)
{
    return generic_mount(partition, mntbuf, fsbuf, NULL, flags);
}

int jfs_umount(char *partition, char *mntbuf)
{
    return generic_umount(mntbuf);
}

int jfs_test(char *devname)
{
    struct jfs_superblock sb;
    int fd;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
        return false;
    
    if (lseek(fd, JFS_SUPER1_OFF, SEEK_SET)!=JFS_SUPER1_OFF)
    {   close(fd);
        return false;
    }
    
    if (read(fd, &sb, sizeof(sb))!=sizeof(sb))
    {   close(fd);
        return false;
    }
    
    if (strncmp(sb.s_magic, JFS_MAGIC, strlen(JFS_MAGIC)) != 0)
    {   close(fd);
        return false;
    }
    
    close(fd);
    return true;
}

int jfs_get_reqmntopt(char *partition, cstrlist *reqopt, cstrlist *badopt)
{
    if (!reqopt || !badopt)
        return -1;
    
    return 0;
}
