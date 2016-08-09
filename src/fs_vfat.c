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
#include "filesys.h"
#include "strlist.h"
#include "fs_vfat.h"
#include "error.h"

int vfat_mkfs(cdico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    char stdoutbuf[2048];
    char command[2048];
    char buffer[2048];
    char mkfsopts[2048];
    int exitst;
    u32 temp32;
    u16 temp16;

    memset(mkfsopts, 0, sizeof(mkfsopts));

    // ---- check that mkfs.vfat is installed
    if (exec_command(command, sizeof(command), NULL, stdoutbuf, sizeof(stdoutbuf), NULL, 0, "mkfs.vfat --help")!=0)
    {   errprintf("mkfs.vfat not found. please install mkfs.vfat on your system or check the PATH.\n");
        return -1;
    }

    // ---- set the correct type of FAT filesystem
    if ((dico_get_u16(d, 0, FSYSHEADKEY_FSVFATTYPE, &temp16)==0) && (temp16==FAT_TYPE_FAT16))
        strlcatf(mkfsopts, sizeof(mkfsopts), " -F 16 ");
    else
        strlcatf(mkfsopts, sizeof(mkfsopts), " -F 32 ");

    // ---- filesystem label
    if (strlen(mkfslabel) > 0)
        strlcatf(mkfsopts, sizeof(mkfsopts), " -n '%.11s' ", mkfslabel);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(mkfsopts, sizeof(mkfsopts), " -n '%.11s' ", buffer);

    // ---- filesystem serial
    if (dico_get_u32(d, 0, FSYSHEADKEY_FSVFATSERIAL, &temp32)==0)
        strlcatf(mkfsopts, sizeof(mkfsopts), " -i '%08X' ", temp32);

    // ---- create the new filesystem using mkfs.vfat
    if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "mkfs.vfat %s %s", mkfsopts, partition)!=0 || exitst!=0)
    {   errprintf("command [%s] failed\n", command);
        return -1;
    }

    return 0;
}

int vfat_getinfo(cdico *d, char *devname)
{
    struct vfat_superblock sb;
    char label[512];
    u32 serial;
    u32 temp32;
    u16 type;
    int ret=0;
    int fd;
    int res;

    memset(label, 0, sizeof(label));
    memset(&sb, 0, sizeof(sb));

    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
    {   ret=-1;
        goto vfat_read_sb_return;
    }

    res=read(fd, &sb, sizeof(sb));
    if (res!=sizeof(sb))
    {   ret=-1;
        goto vfat_read_sb_close;
    }

    // make sure we can find the magic number
    if (be16_to_cpu(sb.magic)!=VFAT_SB_MAGIC)
    {   msgprintf(MSG_DEBUG1, "(be16_to_cpu(sb.magic)=%.2x) != (VFAT_SB_MAGIC=%.2x)\n", be16_to_cpu(sb.magic), VFAT_SB_MAGIC);
        ret=-1;
        goto vfat_read_sb_close;
    }

    // number of FATs must be 1 or 2
    if (sb.num_fats != 1 && sb.num_fats != 2)
    {   msgprintf(MSG_DEBUG1, "Invalid number of FAT tables: %d\n", (int)sb.num_fats);
        ret=-1;
        goto vfat_read_sb_close;
    }

    // num_root_dir_ents set to zero indicates a FAT32
    if (sb.num_root_dir_ents == 0)
    {   type=FAT_TYPE_FAT32;
        msgprintf(MSG_DEBUG1, "FAT_TYPE_FAT32\n");
        memcpy(label, ((char*)&sb)+0x047, 11);
        memcpy(&temp32, ((char*)&sb)+0x043, 4);
        serial=le32_to_cpu(temp32);
    }
    else
    {   type=FAT_TYPE_FAT16;
        msgprintf(MSG_DEBUG1, "FAT_TYPE_FAT16\n");
        memcpy(label, ((char*)&sb)+0x02B, 11);
        memcpy(&temp32, ((char*)&sb)+0x027, 4);
        serial=le32_to_cpu(temp32);
    }

    // ---- type
    dico_add_u16(d, 0, FSYSHEADKEY_FSVFATTYPE, type);

    // ---- label
    msgprintf(MSG_DEBUG1, "vfat_label=[%s]\n", label);
    dico_add_string(d, 0, FSYSHEADKEY_FSLABEL, label);

    // ---- serial
    dico_add_u32(d, 0, FSYSHEADKEY_FSVFATSERIAL, serial);
    msgprintf(MSG_DEBUG1, "vfat_serial=[%08X]\n", serial);

    // ---- minimum fsarchiver version required to restore
    dico_add_u64(d, 0, FSYSHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 8, 0, 0));

vfat_read_sb_close:
    close(fd);
vfat_read_sb_return:
    return ret;
}

int vfat_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo)
{
    return generic_mount(partition, mntbuf, fsbuf, "", flags);
}

int vfat_umount(char *partition, char *mntbuf)
{
    return generic_umount(mntbuf);
}

int vfat_test(char *devname)
{
    struct vfat_superblock sb;
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

    // ---- check it is a VFAT file system
    if (be16_to_cpu(sb.magic)!=VFAT_SB_MAGIC)
    {   close(fd);
        msgprintf(MSG_DEBUG1, "(be16_to_cpu(sb.magic)=%.2x) != (VFAT_SB_MAGIC=%.2x)\n", be16_to_cpu(sb.magic), VFAT_SB_MAGIC);
        return false;
    }

    close(fd);
    return true;
}

int vfat_get_reqmntopt(char *partition, cstrlist *reqopt, cstrlist *badopt)
{
    if (!reqopt || !badopt)
        return -1;

    return 0;
}
