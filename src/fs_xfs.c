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

int xfs_mkfs(cdico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    char stdoutbuf[2048];
    char command[2048];
    char buffer[2048];
    char mkfsopts[2048];
    char xadmopts[2048];
    char uuid[64];
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

    memset(mkfsopts, 0, sizeof(mkfsopts));
    memset(xadmopts, 0, sizeof(xadmopts));
    memset(uuid, 0, sizeof(uuid));

    strlcatf(mkfsopts, sizeof(mkfsopts), " %s ", fsoptions);

    if (strlen(mkfslabel) > 0)
        strlcatf(mkfsopts, sizeof(mkfsopts), " -L '%.12s' ", mkfslabel);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(mkfsopts, sizeof(mkfsopts), " -L '%.12s' ", buffer);

    if ((dico_get_u64(d, 0, FSYSHEADKEY_FSXFSBLOCKSIZE, &temp64)==0) && (temp64%512==0) && (temp64>=512) && (temp64<=65536))
        strlcatf(mkfsopts, sizeof(mkfsopts), " -b size=%ld ", (long)temp64);

    // ---- get xfs features attributes from the archive
    dico_get_u64(d, 0, FSYSHEADKEY_FSXFSFEATURECOMPAT, &sb_features_compat);
    dico_get_u64(d, 0, FSYSHEADKEY_FSXFSFEATUREROCOMPAT, &sb_features_ro_compat);
    dico_get_u64(d, 0, FSYSHEADKEY_FSXFSFEATUREINCOMPAT, &sb_features_incompat);
    dico_get_u64(d, 0, FSYSHEADKEY_FSXFSFEATURELOGINCOMPAT, &sb_features_log_incompat);

    // ---- check fsarchiver is aware of all the filesystem features used on that filesystem
    if (xfs_check_compatibility(sb_features_compat, sb_features_ro_compat, sb_features_incompat, sb_features_log_incompat)!=0)
        return -1;

    // Preserve version 4 of XFS if the original filesystem was an XFSv4 or if
    // it was saved with fsarchiver <= 0.6.19 which does not store the XFS
    // version in the metadata: make an XFSv4 if unsure for better compatibility,
    // as upgrading later is easier than downgrading
    if ((dico_get_u64(d, 0, FSYSHEADKEY_FSXFSVERSION, &temp64)!=0) || (temp64==XFS_SB_VERSION_4))
        xfsver = XFS_SB_VERSION_4;
    else
        xfsver = XFS_SB_VERSION_5;

    // Unfortunately it is impossible to preserve the UUID (stamped on every
    // metadata block) of an XFSv5 filesystem with mkfs.xfs < 4.3.0.
    // Restoring with a new random UUID would work but could prevent the system
    // from booting if this is a boot/root filesystem because grub/fstab often
    // use the UUID to identify it. Hence it is much safer to restore it as an
    // XFSv4 and it also provides a better compatibility with older kernels.
    // Hence this is the safest option.
    // More details: https://github.com/fdupoux/fsarchiver/issues/4
    if ((xfsver==XFS_SB_VERSION_5) && (xfstoolsver < PROGVER(4,3,0)))
    {
        xfsver = XFS_SB_VERSION_4; // Do not preserve the XFS version
        msgprintf(MSG_FORCE,
            "It is impossible to restore this filesystem as an XFSv5 and preserve its UUID\n"
            "with mkfs.xfs < 4.3.0. This filesystem will be restored as an XFSv4 instead\n"
            "as this is a much safer option (preserving the UUID may be required on\n"
            "boot/root filesystems for the operating system to be able to start). If you\n"
            "really want to have an XFSv5 filesystem, please upgrade xfsprogs to version\n"
            "4.3.0 or more recent and rerun this operation to get an XFSv5 with the same\n"
            "UUID as original filesystem\n");
    }

    // Determine if the "crc" mkfs option should be enabled (checksum)
    // - checksum must be disabled if we want to recreate an XFSv4 filesystem
    // - checksum must be enabled if we want to recreate an XFSv5 filesystem
    if (xfstoolsver >= PROGVER(3,2,0)) // only use "crc" option when it is supported by mkfs
    {
        optval = (xfsver==XFS_SB_VERSION_5);
        strlcatf(mkfsopts, sizeof(mkfsopts), " -m crc=%d ", (int)optval);
    }

    // Determine if the "finobt" mkfs option should be enabled (free inode btree)
    // - starting with linux-3.16 XFS has added a btree that tracks free inodes
    // - this feature relies on the new v5 on-disk format but it is optional
    // - this feature is enabled by default when using xfsprogs 3.2.3 or later
    // - this feature will be enabled if the original filesystem was XFSv5 and had it
    if (xfstoolsver >= PROGVER(3,2,1)) // only use "finobt" option when it is supported by mkfs
    {
        optval = ((xfsver==XFS_SB_VERSION_5) && (sb_features_ro_compat & XFS_SB_FEAT_RO_COMPAT_FINOBT));
        strlcatf(mkfsopts, sizeof(mkfsopts), " -m finobt=%d ", (int)optval);
    }

    // Attempt to preserve UUID of the filesystem
    // - the "-m uuid=<UUID>" option in mkfs.xfs was added in mkfs.xfs 4.3.0 and is the best way to set UUIDs
    // - the UUID of XFSv4 can be successfully set using either xfs_admin or mkfs.xfs >= 4.3.0
    // - it is impossible to set both types of UUIDs of an XFSv5 filesystem using xfsprogs < 4.3.0
    //   for this reason the XFS version is forced to v4 if xfsprogs version < 4.3.0
    if (strlen(mkfsuuid) > 0)
        snprintf(uuid, sizeof(uuid), "%s", mkfsuuid);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSUUID, buffer, sizeof(buffer))==0)
        snprintf(uuid, sizeof(uuid), "%s", buffer);
    if (strlen(uuid)==36)
    {
        if (xfstoolsver >= PROGVER(4,3,0))
            strlcatf(mkfsopts, sizeof(mkfsopts), " -m uuid=%s ", uuid);
        else
            strlcatf(xadmopts, sizeof(xadmopts), " -U %s ", uuid);
    }

    // Determine if the "ftype" mkfs option should be enabled (filetype in dirent)
    // - this feature allows the inode type to be stored in the directory structure
    // - mkfs.xfs 4.2.0 enabled ftype by default (supported since mkfs.xfs 3.2.0) for XFSv4 volumes
    // - when CRCs are enabled via -m crc=1, the ftype functionality is always enabled
    // - ftype is madatory in XFSv5 volumes but it is optional for XFSv4 volumes
    // - the "ftype" option must be specified after the "crc" option in mkfs.xfs < 4.2.0:
    //   http://oss.sgi.com/cgi-bin/gitweb.cgi?p=xfs/cmds/xfsprogs.git;a=commit;h=b990de8ba4e2df2bc76a140799d3ddb4a0eac4ce
    // - do not set ftype=1 with crc=1 as mkfs.xfs may fail when both options are enabled (at least with xfsprogs-3.2.2)
    // - XFSv4 with ftype=1 is supported since linux-3.13. We purposely always
    //   disable ftype for V4 volumes to keep them compatible with older kernels
    if (xfstoolsver >= PROGVER(3,2,0)) // only use "ftype" option when it is supported by mkfs
    {
        // ftype is already set to 1 when it is XFSv5
        if (xfsver==XFS_SB_VERSION_4)
            strlcatf(mkfsopts, sizeof(mkfsopts), " -n ftype=0 ");
    }

    // Determine if the "sparse" mkfs option should be enabled (sparse inode allocation)
    // - starting with linux-4.2 XFS can allocate discontinuous inode chunks
    // - this feature relies on the new v5 on-disk format but it is optional
    // - this feature will be enabled if the original filesystem was XFSv5 and had it
    if (xfstoolsver >= PROGVER(4,2,0)) // only use "sparse" option when it is supported by mkfs
    {
        optval = ((xfsver==XFS_SB_VERSION_5) && (sb_features_incompat & XFS_SB_FEAT_INCOMPAT_SPINODES));
        strlcatf(mkfsopts, sizeof(mkfsopts), " -i sparse=%d ", (int)optval);
    }

    // ---- create the new filesystem using mkfs.xfs
    if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "mkfs.xfs -f %s %s", partition, mkfsopts)!=0 || exitst!=0)
    {   errprintf("command [%s] failed\n", command);
        return -1;
    }
    
    // ---- use xfs_admin to set the UUID if not already done with mkfs.xfs
    if (xadmopts[0])
    {
        if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "xfs_admin %s %s", xadmopts, partition)!=0 || exitst!=-0)
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
