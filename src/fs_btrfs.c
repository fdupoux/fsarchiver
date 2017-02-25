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
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <uuid.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "fs_btrfs.h"
#include "filesys.h"
#include "strlist.h"
#include "error.h"

int btrfs_check_compatibility(u64 compat, u64 incompat, u64 ro_compat)
{
    // to preserve the filesystem attributes, fsa must know all the features including the COMPAT ones
    if (compat & ~FSA_BTRFS_FEATURE_COMPAT_SUPP)
        return -1;
    
    if (incompat & ~FSA_BTRFS_FEATURE_INCOMPAT_SUPP)
        return -1;
    
    if (ro_compat & ~FSA_BTRFS_FEATURE_COMPAT_RO_SUPP)
        return -1;
    
    return 0;
}

int btrfs_mkfs(cdico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    char command[2048];
    char buffer[2048];
    char options[2048];
    u64 compat_flags;
    u64 incompat_flags;
    u64 compat_ro_flags;
    int exitst;
    u64 temp64;
    
    // ---- get original filesystem features (if the original filesystem was a btrfs)
    if (dico_get_u64(d, 0, FSYSHEADKEY_BTRFSFEATURECOMPAT, &compat_flags)!=0 ||
        dico_get_u64(d, 0, FSYSHEADKEY_BTRFSFEATUREINCOMPAT, &incompat_flags)!=0 ||
        dico_get_u64(d, 0, FSYSHEADKEY_BTRFSFEATUREROCOMPAT, &compat_ro_flags)!=0)
    {   // dont fail the original filesystem may not be a btrfs. in that case set defaults features
        compat_flags=0;
        incompat_flags=0;
        compat_ro_flags=0;
    }
    
    // ---- check there is no unsuported feature in that filesystem
    if (btrfs_check_compatibility(compat_flags, incompat_flags, compat_ro_flags)!=0)
    {   errprintf("this filesystem has features which are not supported by this fsarchiver version.\n");
        return -1;
    }
    
    // ---- there is no option that just displays the version and return 0 in mkfs.btrfs-0.16
    if (exec_command(command, sizeof(command), NULL, NULL, 0, NULL, 0, "mkfs.btrfs")!=0)
    {   errprintf("mkfs.btrfs not found. please install btrfs-progs on your system or check the PATH.\n");
        return -1;
    }
    
    // ---- set the advanced filesystem settings from the dico
    memset(options, 0, sizeof(options));

    strlcatf(options, sizeof(options), " %s ", fsoptions);

    if (strlen(mkfslabel) > 0)
        strlcatf(options, sizeof(options), " -L '%s' ", mkfslabel);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(options, sizeof(options), " -L '%s' ", buffer);

    if (strlen(mkfsuuid) > 0)
        strlcatf(options, sizeof(options), " -U '%s' ", mkfsuuid);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSUUID, buffer, sizeof(buffer))==0)
        strlcatf(options, sizeof(options), " -U '%s' ", buffer);

    if (dico_get_u64(d, 0, FSYSHEADKEY_FSBTRFSSECTORSIZE, &temp64)==0)
        strlcatf(options, sizeof(options), " -s %ld ", (long)temp64);
    
    if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "mkfs.btrfs -f %s %s", partition, options)!=0 || exitst!=0)
    {   errprintf("command [%s] failed\n", command);
        return -1;
    }
    
    return 0;
}

int btrfs_getinfo(cdico *d, char *devname)
{
    struct btrfs_super_block sb;
    char uuid[512];
    u16 temp32;
    int ret=0;
    int fd;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
    {   ret=-1;
        errprintf("cannot open(%s, O_RDONLY)\n", devname);
        goto btrfs_read_sb_return;
    }
    
    if (lseek(fd, BTRFS_SUPER_INFO_OFFSET, SEEK_SET)!=BTRFS_SUPER_INFO_OFFSET)
    {   ret=-2;
        errprintf("cannot lseek(fd, BTRFS_SUPER_INFO_OFFSET, SEEK_SET) on %s\n", devname);
        goto btrfs_read_sb_close;
    }
    
    if (read(fd, &sb, sizeof(sb))!=sizeof(sb))
    {   ret=-3;
        errprintf("cannot read the btrfs superblock on device [%s]\n", devname);
        goto btrfs_read_sb_close;
    }
    
    if (strncmp((char*)&sb.magic, BTRFS_MAGIC, sizeof(sb.magic))!=0)
    {   ret=-4;
        errprintf("magic different from expectations superblock on [%s]: magic=[%.8s], expected=[%.8s]\n", devname, (char*)&sb.magic, BTRFS_MAGIC);
        goto btrfs_read_sb_close;
    }
    
    // ---- label
    msgprintf(MSG_DEBUG1, "btrfs_label=[%s]\n", sb.label);
    dico_add_string(d, 0, FSYSHEADKEY_FSLABEL, (char*)sb.label);
    
    // ---- uuid
    /*if ((str=e2p_uuid2str(sb.dev_item.fsid))!=NULL)
        dico_add_string(d, 0, FSYSHEADKEY_FSUUID, str);*/
    memset(uuid, 0, sizeof(uuid));
    uuid_unparse_lower((u8*)sb.dev_item.fsid, uuid);
    dico_add_string(d, 0, FSYSHEADKEY_FSUUID, uuid);
    msgprintf(MSG_DEBUG1, "btrfs_uuid=[%s]\n", uuid);
    
    // ---- sector size
    temp32=le32_to_cpu(sb.sectorsize);
    dico_add_u64(d, 0, FSYSHEADKEY_FSBTRFSSECTORSIZE, temp32);
    msgprintf(MSG_DEBUG1, "btrfs_sectorsize=[%ld]\n", (long)temp32);
    
    // ---- filesystem features
    dico_add_u64(d, 0, FSYSHEADKEY_BTRFSFEATURECOMPAT, le64_to_cpu(sb.compat_flags));
    dico_add_u64(d, 0, FSYSHEADKEY_BTRFSFEATUREINCOMPAT, le64_to_cpu(sb.incompat_flags));
    dico_add_u64(d, 0, FSYSHEADKEY_BTRFSFEATUREROCOMPAT, le64_to_cpu(sb.compat_ro_flags));
    if (btrfs_check_compatibility(le64_to_cpu(sb.compat_flags), le64_to_cpu(sb.incompat_flags), le64_to_cpu(sb.compat_ro_flags))!=0)
    {   errprintf("this filesystem has features which are not supported by this fsarchiver version.\n");
        return -1;
    }
    
    // ---- minimum fsarchiver version required to restore
    dico_add_u64(d, 0, FSYSHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 6, 4, 0));
    
btrfs_read_sb_close:
    close(fd);
btrfs_read_sb_return:
    return ret;
}

int btrfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo)
{
    return generic_mount(partition, mntbuf, fsbuf, NULL, flags);
}

int btrfs_umount(char *partition, char *mntbuf)
{
    return generic_umount(mntbuf);
}

int btrfs_test(char *devname)
{
    struct btrfs_super_block sb;
    int fd;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
        return false;
    
    if (lseek(fd, BTRFS_SUPER_INFO_OFFSET, SEEK_SET)!=BTRFS_SUPER_INFO_OFFSET)
    {   close(fd);
        return false;
    }
    
    if (read(fd, &sb, sizeof(sb))!=sizeof(sb))
    {   close(fd);
        return false;
    }
    
    if (strncmp((char*)&sb.magic, BTRFS_MAGIC, sizeof(sb.magic))!=0)
    {   close(fd);
        return false;
    }
    
    close(fd);
    return true;
}

int btrfs_get_reqmntopt(char *partition, cstrlist *reqopt, cstrlist *badopt)
{
    if (!reqopt || !badopt)
        return -1;
    
    strlist_add(badopt, "noacl");
    return 0;
}
