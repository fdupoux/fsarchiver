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
#include "dico.h"
#include "common.h"
#include "fs_xfs.h"
#include "filesys.h"
#include "strlist.h"
#include "error.h"

int xfs_mkfs(cdico *d, char *partition)
{
    char command[2048];
    char buffer[2048];
    char options[2048];
    int exitst;
    u64 temp64;
    
    if (exec_command(command, sizeof(command), NULL, NULL, 0, NULL, 0, "mkfs.xfs -V")!=0)
    {   errprintf("mkfs.xfs not found. please install xfsprogs on your system or check the PATH.\n");
        return -1;
    }
    
    memset(options, 0, sizeof(options));
    if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(options, sizeof(options), " -L '%.12s' ", buffer);
    
    if ((dico_get_u64(d, 0, FSYSHEADKEY_FSXFSBLOCKSIZE, &temp64)==0) && (temp64%512==0) && (temp64>=512) && (temp64<=65536))
        strlcatf(options, sizeof(options), " -b size=%ld ", (long)temp64);
    
    if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "mkfs.xfs -f %s %s", partition, options)!=0 || exitst!=0)
    {   errprintf("command [%s] failed\n", command);
        return -1;
    }
    
    // ---- use xfs_admin to set the other advanced options
    memset(options, 0, sizeof(options));
    if (dico_get_string(d, 0, FSYSHEADKEY_FSUUID, buffer, sizeof(buffer))==0 && strlen(buffer)==36)
        strlcatf(options, sizeof(options), " -U %s ", buffer);
    
    if (options[0])
    {
        if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "xfs_admin %s %s", options, partition)!=0 || exitst!=-0)
        {   errprintf("command [%s] failed\n", command);
            return -1;
        }
    }
    
    return 0;    
}

int xfs_getinfo(cdico *d, char *devname)
{
    struct xfs_sb sb;
    char uuid[512];
    u32 temp32;
    int ret=0;
    int fd;
    int res;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
    {   ret=-1;
        goto xfs_read_sb_return;
    }
    
    res=read(fd, &sb, sizeof(sb));
    if (res!=sizeof(sb))
    {   ret=-1;
        goto xfs_read_sb_close;
    }
    
    // ---- check it's an XFS file system
    if (be32_to_cpu(sb.sb_magicnum) != XFS_SUPER_MAGIC)
    {   ret=-1;
        msgprintf(3, "sb.sb_magicnum!=XFS_SUPER_MAGIC\n");
        goto xfs_read_sb_close;
    }
    
    // ---- label
    msgprintf(MSG_DEBUG1, "xfs_label=[%s]\n", sb.sb_fname);
    dico_add_string(d, 0, FSYSHEADKEY_FSLABEL, (char*)sb.sb_fname);
    
    // ---- uuid
    /*if ((str=e2p_uuid2str(&sb.sb_uuid))!=NULL)
        dico_add_string(d, 0, FSYSHEADKEY_FSUUID, str);*/
    memset(uuid, 0, sizeof(uuid));
    uuid_unparse_lower((u8*)&sb.sb_uuid, uuid);
    dico_add_string(d, 0, FSYSHEADKEY_FSUUID, uuid);
    msgprintf(MSG_DEBUG1, "xfs_uuid=[%s]\n", uuid);
    
    // ---- block size
    temp32=be32_to_cpu(sb.sb_blocksize);
    if ((temp32%512!=0) || (temp32<512) || (temp32>65536))
    {   ret=-1;
        msgprintf(3, "xfs_blksize=[%ld] is an invalid xfs block size\n", (long)temp32);
        goto xfs_read_sb_close;
    }
    dico_add_u64(d, 0, FSYSHEADKEY_FSXFSBLOCKSIZE, temp32);
    msgprintf(MSG_DEBUG1, "xfs_blksize=[%ld]\n", (long)temp32);
    
    // ---- minimum fsarchiver version required to restore
    dico_add_u64(d, 0, FSYSHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 6, 4, 0));
    
xfs_read_sb_close:
    close(fd);
xfs_read_sb_return:
    return ret;
}

int xfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo)
{
    return generic_mount(partition, mntbuf, fsbuf, "nouuid", flags);
}

int xfs_umount(char *partition, char *mntbuf)
{
    return generic_umount(mntbuf);
}

int xfs_test(char *devname)
{
    struct xfs_sb sb;
    int fd;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
    {
        msgprintf(MSG_DEBUG1, "open64(%s) failed\n", devname);
        return false;
    }
    
    memset(&sb, 0, sizeof(sb));
    if (read(fd, &sb, sizeof(sb))!=sizeof(sb))
    {   close(fd);
        msgprintf(MSG_DEBUG1, "read failed\n");
        return false;
    }
    
    // ---- check it's an XFS file system
    if (be32_to_cpu(sb.sb_magicnum) != XFS_SUPER_MAGIC)
    {   close(fd);
        msgprintf(MSG_DEBUG1, "(be32_to_cpu(sb.sb_magicnum)=%.8x) != (XFS_SUPER_MAGIC=%.8x)\n", be32_to_cpu(sb.sb_magicnum), XFS_SUPER_MAGIC);
        return false;
    }
    
    close(fd);
    return true;
}

int xfs_get_reqmntopt(char *partition, cstrlist *reqopt, cstrlist *badopt)
{
    if (!reqopt || !badopt)
        return -1;

    return 0;
}
