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

int xfs_check_compatibility(u64 compat, u64 ro_compat, u64 incompat, u64 log_incompat)
{
    int errors=0;

    msgprintf(MSG_DEBUG1, "xfs features: compat=[%ld]\n", (long)compat);
    msgprintf(MSG_DEBUG1, "xfs features: ro_compat=[%ld]\n", (long)ro_compat);
    msgprintf(MSG_DEBUG1, "xfs features: incompat=[%ld]\n", (long)incompat);
    msgprintf(MSG_DEBUG1, "xfs features: log_incompat=[%ld]\n", (long)log_incompat);

    // to preserve the filesystem attributes, fsa must know all the features including the COMPAT ones
    if (compat & ~FSA_XFS_FEATURE_COMPAT_SUPP)
        errors++;

    if (ro_compat & ~FSA_XFS_FEATURE_RO_COMPAT_SUPP)
        errors++;

    if (incompat & ~FSA_XFS_FEATURE_INCOMPAT_SUPP)
        errors++;

    if (log_incompat & ~FSA_XFS_FEATURE_LOG_INCOMPAT_SUPP)
        errors++;

    if (errors > 0)
    {   errprintf("this filesystem has XFS features which are not supported by this fsarchiver version.\n");
        return -1;
    }
    else
    {   return 0;
    }
}

int xfs_mkfs(cdico *d, char *partition, char *fsoptions)
{
    char stdoutbuf[2048];
    char command[2048];
    char buffer[2048];
    char options[2048];
    u64 xfstoolsver;
    int exitst;
    u64 temp64;
    u64 xfsver;
    int x, y, z;
    int optval;

    u64 sb_features_compat=0;
    u64 sb_features_ro_compat=0;
    u64 sb_features_incompat=0;
    u64 sb_features_log_incompat=0;

    // ---- check that mkfs is installed and get its version
    if (exec_command(command, sizeof(command), NULL, stdoutbuf, sizeof(stdoutbuf), NULL, 0, "mkfs.xfs -V")!=0)
    {   errprintf("mkfs.xfs not found. please install xfsprogs on your system or check the PATH.\n");
        return -1;
    }
    x=y=z=0;
    sscanf(stdoutbuf, "mkfs.xfs version %d.%d.%d", &x, &y, &z);
    if (x==0 && y==0)
    {   errprintf("Can't parse mkfs.xfs version number: x=y=0\n");
        return -1;
    }
    xfstoolsver=PROGVER(x,y,z);
    msgprintf(MSG_VERB2, "Detected mkfs.xfs version %d.%d.%d\n", x, y, z);

    memset(options, 0, sizeof(options));

    strlcatf(options, sizeof(options), " %s ", fsoptions);

    if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(options, sizeof(options), " -L '%.12s' ", buffer);

    if ((dico_get_u64(d, 0, FSYSHEADKEY_FSXFSBLOCKSIZE, &temp64)==0) && (temp64%512==0) && (temp64>=512) && (temp64<=65536))
        strlcatf(options, sizeof(options), " -b size=%ld ", (long)temp64);

    // ---- get xfs features attributes from the archive
    dico_get_u64(d, 0, FSYSHEADKEY_FSXFSFEATURECOMPAT, &sb_features_compat);
    dico_get_u64(d, 0, FSYSHEADKEY_FSXFSFEATUREROCOMPAT, &sb_features_ro_compat);
    dico_get_u64(d, 0, FSYSHEADKEY_FSXFSFEATUREINCOMPAT, &sb_features_incompat);
    dico_get_u64(d, 0, FSYSHEADKEY_FSXFSFEATURELOGINCOMPAT, &sb_features_log_incompat);

    // ---- check fsarchiver is aware of all the filesystem features used on that filesystem
    if (xfs_check_compatibility(sb_features_compat, sb_features_ro_compat, sb_features_incompat, sb_features_log_incompat)!=0)
        return -1;

    // Preserve version 4 of XFS if the original filesystem was an XFS v4 or if
    // the original filesystem was saved with fsarchiver <= 0.6.19 which does
    // not store the XFS version in the metadata: make an XFSv4 if unsure for
    // better compatibility and as upgrading later is easier than downgrading
    // if the user gets an XFSv4 and wanted an XFSv5
    if ((dico_get_u64(d, 0, FSYSHEADKEY_FSXFSVERSION, &temp64)!=0) || (temp64==XFS_SB_VERSION_4))
        xfsver = XFS_SB_VERSION_4;
    else
        xfsver = XFS_SB_VERSION_5;

    // Determine if the "crc" mkfs option should be enabled (checksum)
    // - checksum must be disabled if we want to recreate an XFSv4 filesystem
    // - checksum must be enabled if we want to recreate an XFSv5 filesystem
    if (xfstoolsver >= PROGVER(3,2,0)) // only use "crc" option when it is supported by mkfs
    {
        optval = (xfsver==XFS_SB_VERSION_5);
        strlcatf(options, sizeof(options), " -m crc=%d ", (int)optval);
    }

    // Determine if the "finobt" mkfs option should be enabled (free inode btree)
    // - starting Linux 3.16 XFS has added a btree that tracks free inodes
    // - this feature relies on the new v5 on-disk format but it is optional
    // - this feature enabled by default when using xfsprogs 3.2.3 or later
    // - this feature will be enabled if the original filesystem was XFSv5 and had it
    if (xfstoolsver >= PROGVER(3,2,1)) // only use "finobt" option when it is supported by mkfs
    {
        optval = ((xfsver==XFS_SB_VERSION_5) && (sb_features_ro_compat & XFS_SB_FEAT_RO_COMPAT_FINOBT));
        strlcatf(options, sizeof(options), " -m finobt=%d ", (int)optval);
    }

    // Determine if the "ftype" mkfs option should be enabled (filetype in dirent)
    // - this feature allows the inode type to be stored in the directory structure
    // - mkfs.xfs 4.2.0 enabled ftype by default (supported since mkfs.xfs 3.2.0) for XFSv4 volumes
    // - when CRCs are enabled via -m crc=1, the ftype functionality is always enabled
    // - ftype is madatory in XFSv5 volumes but it is optional for XFSv4 volumes
    if (xfstoolsver >= PROGVER(3,2,0)) // only use "ftype" option when it is supported by mkfs
    {
        optval = (xfsver==XFS_SB_VERSION_5);
        strlcatf(options, sizeof(options), " -n ftype=%d ", (int)optval);
    }

    // ---- create the new filesystem using mkfs.xfs
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
    u64 xfsver;
    u32 temp32;
    int ret=0;
    int fd;
    int res;

    u64 sb_features_compat=0;
    u64 sb_features_ro_compat=0;
    u64 sb_features_incompat=0;
    u64 sb_features_log_incompat=0;

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
    if (be32_to_cpu(sb.sb_magicnum) != XFS_SB_MAGIC)
    {   ret=-1;
        msgprintf(3, "sb.sb_magicnum!=XFS_SB_MAGIC\n");
        goto xfs_read_sb_close;
    }

    // ---- check XFS filesystem version
    xfsver=be16_to_cpu(sb.sb_versionnum) & XFS_SB_VERSION_NUMBITS;
    switch (xfsver)
    {
        case XFS_SB_VERSION_4:
        case XFS_SB_VERSION_5:
            msgprintf(MSG_DEBUG1, "Detected XFS filesystem version %d\n", (int)xfsver);
            dico_add_u64(d, 0, FSYSHEADKEY_FSXFSVERSION, xfsver);
            break;
        default:
            ret=-1;
            msgprintf(MSG_STACK, "Invalid XFS filesystem version: version=[%d]\n", (int)xfsver);
            goto xfs_read_sb_close;
            break;
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

    // ---- get filesystem features (will all be set to 0 if this is an XFSv4)
    if (xfsver == XFS_SB_VERSION_5)
    {
        sb_features_compat=be32_to_cpu(sb.sb_features_compat);
        sb_features_ro_compat=be32_to_cpu(sb.sb_features_ro_compat);
        sb_features_incompat=be32_to_cpu(sb.sb_features_incompat);
        sb_features_log_incompat=be32_to_cpu(sb.sb_features_log_incompat);
    }

    // ---- check fsarchiver is aware of all the filesystem features used on that filesystem
    if (xfs_check_compatibility(sb_features_compat, sb_features_ro_compat, sb_features_incompat, sb_features_log_incompat)!=0)
        return -1;

    // ---- store features in the archive metadata
    dico_add_u64(d, 0, FSYSHEADKEY_FSXFSFEATURECOMPAT, (u64)sb_features_compat);
    dico_add_u64(d, 0, FSYSHEADKEY_FSXFSFEATUREROCOMPAT, (u64)sb_features_ro_compat);
    dico_add_u64(d, 0, FSYSHEADKEY_FSXFSFEATUREINCOMPAT, (u64)sb_features_incompat);
    dico_add_u64(d, 0, FSYSHEADKEY_FSXFSFEATURELOGINCOMPAT, (u64)sb_features_log_incompat);

    // ---- minimum fsarchiver version required to restore
    dico_add_u64(d, 0, FSYSHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 6, 20, 0));
    
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
    if (be32_to_cpu(sb.sb_magicnum) != XFS_SB_MAGIC)
    {   close(fd);
        msgprintf(MSG_DEBUG1, "(be32_to_cpu(sb.sb_magicnum)=%.8x) != (XFS_SB_MAGIC=%.8x)\n", be32_to_cpu(sb.sb_magicnum), XFS_SB_MAGIC);
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
