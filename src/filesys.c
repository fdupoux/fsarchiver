/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2010 Francois Dupoux.  All rights reserved.
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
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/utsname.h>
#include <sys/mount.h>
#include <errno.h>

#include "fsarchiver.h"
#include "common.h"
#include "filesys.h"
#include "fs_ext2.h"
#include "fs_reiserfs.h"
#include "fs_reiser4.h"
#include "fs_btrfs.h"
#include "fs_xfs.h"
#include "fs_jfs.h"
#include "fs_ntfs.h"
#include "error.h"

cfilesys filesys[]=
{
    {"ext2",     extfs_mount,    extfs_umount,    extfs_getinfo,    ext2_mkfs,     ext2_test,     extfs_get_reqmntopt,    false, false},
    {"ext3",     extfs_mount,    extfs_umount,    extfs_getinfo,    ext3_mkfs,     ext3_test,     extfs_get_reqmntopt,    false, false},
    {"ext4",     extfs_mount,    extfs_umount,    extfs_getinfo,    ext4_mkfs,     ext4_test,     extfs_get_reqmntopt,    false, false},
    {"reiserfs", reiserfs_mount, reiserfs_umount, reiserfs_getinfo, reiserfs_mkfs, reiserfs_test, reiserfs_get_reqmntopt, false, false},
    {"reiser4",  reiser4_mount,  reiser4_umount,  reiser4_getinfo,  reiser4_mkfs,  reiser4_test,  reiser4_get_reqmntopt,  false, false},
    {"btrfs",    btrfs_mount,    btrfs_umount,    btrfs_getinfo,    btrfs_mkfs,    btrfs_test,    btrfs_get_reqmntopt,    false, false},
    {"xfs",      xfs_mount,      xfs_umount,      xfs_getinfo,      xfs_mkfs,      xfs_test,      xfs_get_reqmntopt,      false, false},
    {"jfs",      jfs_mount,      jfs_umount,      jfs_getinfo,      jfs_mkfs,      jfs_test,      jfs_get_reqmntopt,      false, false},
    {"ntfs",     ntfs_mount,     ntfs_umount,     ntfs_getinfo,     ntfs_mkfs,     ntfs_test,     ntfs_get_reqmntopt,     true,  true},
    {NULL,       NULL,           NULL,            NULL,             NULL,          NULL,          NULL,                   false, false},
};

// return the index of a filesystem in the filesystem table
int generic_get_fstype(char *fsname, int *fstype)
{
    int i;
    
    for (i=0; filesys[i].name; i++)
    {
        if (strcmp(filesys[i].name, fsname)==0)
        {   *fstype=i;
            return 0;
        }
    }
    *fstype=-1;
    return -1;
}

int generic_get_spacestats(char *dev, char *mnt, char *text, int textlen)
{
    struct statfs stf;
    struct stat64 st;
    
    // init
    memset(text, 0, textlen);
    
    if (stat64(dev, &st)!=0)
        return -1;
    
    if (statfs(dev, &stf)!=0)
        return -1;
    
    if (!S_ISBLK(st.st_mode))
        return -1;
    else
        return 0;
}

int generic_get_fsrwstatus(char *options)
{
    char temp[FSA_MAX_FSNAMELEN];
    char delims[]=",";
    char *saveptr;
    char *result;
    
    snprintf(temp, sizeof(temp), "%s", options);
    
    result=strtok_r(temp, delims, &saveptr);
    while(result != NULL)
    {   if (strcmp(result, "rw")==0)
            return 1; // true
        result = strtok_r(NULL, delims, &saveptr);
    }
    
    return 0; // false
}

int devcmp(char *dev1, char *dev2)
{
    struct stat64 devstat[2];
    char *devname[]={dev1, dev2};
    int i;

    for (i=0; i<2; i++)
    {
        if (strncmp(devname[i], "/dev/", 5)!=0)
            return -1;

        errno=0;
        if (stat64(devname[i], &devstat[i]) != 0)
        {
            if (errno == ENOENT)
                errprintf("Warning: node for device [%s] does not exist in /dev/\n", devname[i]);
            else
                errprintf("Warning: cannot get details for device [%s]\n", devname[i]);
            return -1;
        }
        
        if (!S_ISBLK(devstat[0].st_mode))
        {
            errprintf("Warning: [%s] is not a block device\n", devname[i]);
            return -1;
        }
    }

    return  (devstat[0].st_rdev==devstat[1].st_rdev) ? 0 : 1;
}

int generic_get_mntinfo(char *devname, int *readwrite, char *mntbuf, int maxmntbuf, char *optbuf, int maxoptbuf, char *fsbuf, int maxfsbuf)
{
    char col_fs[FSA_MAX_FSNAMELEN];
    int devisroot=false;
    struct stat64 devstat;
    struct stat64 rootstat;
    char delims[]=" \t\n";
    struct utsname suname;
    char col_dev[128];
    char col_mnt[128];
    char col_opt[128];
    char line[1024];
    char temp[2048];
    char *saveptr;
    char *result;
    int res;
    FILE *f;
    int i;

    // init
    res=uname(&suname);
    *readwrite=-1; // unknown
    memset(mntbuf, 0, sizeof(mntbuf));
    memset(optbuf, 0, sizeof(optbuf));

    // 1. workaround for systems not having the "/dev/root" node entry.

    // There are systems showing "/dev/root" in "/proc/mounts" instead
    // of the actual root partition such as "/dev/sda1".
    // The consequence is that fsarchiver won't be able to realize
    // that the device it is archiving (such as "/dev/sda1") is the
    // same as "/dev/root" and that it is actually mounted. This 
    // function would then say that the "/dev/sda1" device is not mounted
    // and fsarchiver would try to mount it and mount() fails with EBUSY
    if (stat64(devname, &devstat)==0 && stat64("/", &rootstat)==0 && (devstat.st_rdev==rootstat.st_dev))
    {
        devisroot=true;
        msgprintf(MSG_VERB1, "device [%s] is the root device\n", devname);
    }

    // 2. check device in "/proc/mounts" (typical case)
    if ((f=fopen("/proc/mounts","rb"))==NULL)
    {   sysprintf("Cannot open /proc/mounts\n");
        return 1;
    }

    while(!feof(f))
    {
        if (stream_readline(f, line, 1024)>1)
        {
            result=strtok_r(line, delims, &saveptr);
            col_dev[0]=col_mnt[0]=col_fs[0]=col_opt[0]=0;
            for(i=0; result != NULL && i<=3; i++)
            {
                switch (i) // only the second word is a mount-point
                {
                    case 0:
                        snprintf(col_dev, sizeof(col_dev), "%s", result);
                        break;
                    case 1:
                        snprintf(col_mnt, sizeof(col_mnt), "%s", result);
                        break;
                    case 2:
                        snprintf(col_fs, sizeof(col_fs), "%s", result);
                        break;
                    case 3:
                        snprintf(col_opt, sizeof(col_opt), "%s", result);
                        break;
                }
                result = strtok_r(NULL, delims, &saveptr);
            }

            if ((devisroot==true) && (strcmp(col_mnt, "/")==0) && (strcmp(col_fs, "rootfs")!=0))
                snprintf(col_dev, sizeof(col_dev), "%s", devname);

            msgprintf(MSG_DEBUG1, "mount entry: col_dev=[%s] col_mnt=[%s] col_fs=[%s] col_opt=[%s]\n", col_dev, col_mnt, col_fs, col_opt);

            if (devcmp(col_dev, devname)==0)
            {
                if (generic_get_spacestats(col_dev, col_mnt, temp, sizeof(temp))==0)
                {
                    msgprintf(MSG_DEBUG1, "found mount entry for device=[%s]: mnt=[%s] fs=[%s] opt=[%s]\n", devname, col_mnt, col_fs, col_opt);
                    *readwrite=generic_get_fsrwstatus(col_opt);
                    snprintf(mntbuf, maxmntbuf, "%s", col_mnt);
                    snprintf(optbuf, maxoptbuf, "%s", col_opt);
                    snprintf(fsbuf, maxfsbuf, "%s", col_fs);
                    fclose(f);
                    return 0;
                }
            }
        }
    }
    
    fclose(f);
    return -1;
}

int generic_mount(char *partition, char *mntbuf, char *fsbuf, char *mntopt, int flags)
{
    if (!partition || !mntbuf || !fsbuf || !fsbuf[0])
    {   errprintf("invalid parameters\n");
        return -1;
    }
    
    // optimization: don't change access times on the original partition
    flags|=MS_NOATIME|MS_NODIRATIME;
    
    // if fsbuf is not empty, use the filesystem which is specified
    msgprintf(MSG_DEBUG1, "trying to mount [%s] on [%s] as [%s] with options [%s]\n", partition, mntbuf, fsbuf, mntopt);
    if (mount(partition, mntbuf, fsbuf, flags, mntopt)!=0)
    {   errprintf("partition [%s] cannot be mounted on [%s] as [%s] with options [%s]\n", partition, mntbuf, fsbuf, mntopt);
        return -1;
    }
    msgprintf(MSG_VERB2, "partition [%s] was successfully mounted on [%s] as [%s] with options [%s]\n", partition, mntbuf, fsbuf, mntopt);
    return 0;
}

int generic_umount(char *mntbuf)
{
    if (!mntbuf)
    {   errprintf("invalid param: mntbuf is null\n");
        return -1;
    }
    msgprintf(MSG_DEBUG1, "unmount_partition(%s)\n", mntbuf);
    return umount2(mntbuf, 0);
}

char *format_prog_version(u64 version, char *bufdat, int buflen)
{
    snprintf(bufdat, buflen, "%ld.%ld.%ld", (long)(version>>16&0xFF), (long)(version>>8&0xFF), (long)(version>>0&0xFF));
    return bufdat;
}
