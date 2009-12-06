/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2009 Francois Dupoux.  All rights reserved.
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

#include "fsarchiver.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "fs_ntfs.h"
#include "filesys.h"
#include "strlist.h"

int ntfs_mkfs(cdico *d, char *partition)
{
    char command[2048];
    char buffer[2048];
    char options[2048];
    u64 temp64;
    u32 temp32;
    u16 temp16;
    
    // there is no option that just displays the version and return 0 in mkfs.ntfs
    if (exec_command(command, sizeof(command), NULL, 0, NULL, 0, "which mkfs.ntfs")!=0)
    {   errprintf("mkfs.ntfs not found. please install ntfs-3g 2009.1.1AR.1 or more recent on your system or check the PATH.\n");
        return -1;
    }
    
    // ---- set the advanced filesystem settings from the dico
    memset(options, 0, sizeof(options));
    if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(options, sizeof(options), " --label '%s' ", buffer);
    
    if (dico_get_u16(d, 0, FSYSHEADKEY_NTFSSECTORSIZE, &temp16)==0)
        strlcatf(options, sizeof(options), " -s %ld ", (long)temp16);
    
    if (dico_get_u32(d, 0, FSYSHEADKEY_NTFSCLUSTERSIZE, &temp32)==0)
        strlcatf(options, sizeof(options), " -c %ld ", (long)temp32);
    
    if (exec_command(command, sizeof(command), NULL, 0, NULL, 0, "mkfs.ntfs -f %s %s", partition, options)!=0)
    {   errprintf("command [%s] failed\n", command);
        return -1;
    }
    
    // ---- preserve ntfs uuid (attribute saved only with fsarchiver>=0.5.8)
    if (dico_get_u64(d, 0, FSYSHEADKEY_NTFSUUID, &temp64)==0)
        ntfs_replace_uuid(partition, cpu_to_le64(temp64));
    
    return 0;
}

int ntfs_read_label(int fd, char *labelbuf, int bufsize, struct s_ntfsinfo *info)
{
    u8 cFileRecord[4096]; // 1024 in most cases
    u64 offset;
    u16 nOffsetSequenceAttribute;
    u8 *cData;
    u32 dwAttribType;
    u32 dwAttribLen;
    bool bAttribResident;    
    u8 *cDataResident;
    u32 nAttrSize;
    u64 i;
    
    // init
    memset(labelbuf, 0, bufsize);
    
    // 1. read $Volume record
    offset = ((u64) info->lcn_of_mft_data_attrib * info->bytes_per_cluster) + ((u64) (3 * info->file_record_size));
    
    if (lseek(fd, offset, SEEK_SET)!=offset)
    {   errprintf("lseek(fd, offset=%lld, SEEK_SET) failed\n", (long long)offset);
        return -1;
    }
    
    if ((read(fd, cFileRecord, info->file_record_size))!=info->file_record_size)
    {   errprintf("cannot read file record\n");
        return -2;
    }
    
    // -------- decode the MFT File record
    nOffsetSequenceAttribute = le16_to_cpu(*((u16*)(cFileRecord+0x14)));
    cData = cFileRecord + nOffsetSequenceAttribute;
    
    do
    {   // szData points to the beginning of an attribute
        dwAttribType = le32_to_cpu(*((u32*)(cData)));
        dwAttribLen = le32_to_cpu(*((u32*)(cData+4)));
        bAttribResident = le8_to_cpu(*((u8*)(cData+8)))==0;
        
        if(dwAttribType == 0x60) // "volume_name"
        {
            if (bAttribResident == false)
            {   errprintf("$volume_name attribute is not resident\n");
                return -3;
            }
            
            nAttrSize = le16_to_cpu(*((u16*)(cData+0x10)));
            cDataResident = cData+le16_to_cpu(*((u16*)(cData+0x14)));
            
            for (i=0; (i < nAttrSize/2) && (i<bufsize-1); i++)
                labelbuf[i] = cDataResident[2*i];
        }
        cData += dwAttribLen;
    } while (dwAttribType != (u32)-1); // attribute list ends with type -1
    
    return 0;
}

int ntfs_getinfo(cdico *d, char *devname)
{
    struct s_ntfsinfo info;
    char bootsect[16384];
    char label[256];
    int ret=0;
    int fd=-1;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
    {   ret=-1;
        errprintf("cannot open(%s, O_RDONLY)\n", devname);
        goto ntfs_getinfo_return;
    }
    
    if (read(fd, bootsect, sizeof(bootsect))!=sizeof(bootsect))
    {   ret=-1;
        errprintf("cannot read the boot sector on %s\n", devname);
        goto ntfs_getinfo_close;
    }
    
    if (memcmp(bootsect+3, "NTFS", 4) != 0)
    {   ret=-1;
        errprintf("cannot find the ntfs signature on %s\n", devname);
        goto ntfs_getinfo_close;
    }
    
    info.bytes_per_sector = le16_to_cpu(*((u16*)(bootsect+0xB)));
    info.sectors_per_clusters = le8_to_cpu(*((u8*)(bootsect+0xD)));
    info.total_sector_count = le64_to_cpu(*((u64*)(bootsect+0x28)));
    info.lcn_of_mft_data_attrib = le64_to_cpu(*((u64*)(bootsect+0x30)));
    info.bytes_per_cluster = info.bytes_per_sector * info.sectors_per_clusters;
    info.clusters_per_mft_record = *((s8*)(bootsect+0x40));
    info.uuid = le64_to_cpu(*((u64*)(bootsect+0x48)));
    
    if (info.clusters_per_mft_record > 0)
        info.file_record_size = info.clusters_per_mft_record * info.bytes_per_cluster;
    else
        info.file_record_size = 1 << (-info.clusters_per_mft_record);
    
    msgprintf(MSG_DEBUG1, "bytes_per_sector=[%lld]\n", (long long)info.bytes_per_sector);
    msgprintf(MSG_DEBUG1, "sectors_per_clusters=[%lld]\n", (long long)info.sectors_per_clusters);
    msgprintf(MSG_DEBUG1, "total_sector_count=[%lld]\n", (long long)info.total_sector_count);
    msgprintf(MSG_DEBUG1, "clusters_per_mft_record=[%lld]\n", (long long)info.clusters_per_mft_record);
    msgprintf(MSG_DEBUG1, "lcn_of_mft_data_attrib=[%lld]\n", (long long)info.lcn_of_mft_data_attrib);
    msgprintf(MSG_DEBUG1, "bytes_per_cluster=[%lld]\n", (long long)info.bytes_per_cluster);
    msgprintf(MSG_DEBUG1, "lcn_of_mft_data_attrib=[%lld]\n", (long long)info.lcn_of_mft_data_attrib);
    msgprintf(MSG_DEBUG1, "file_record_size=[%lld]\n", (long long)info.file_record_size);
    msgprintf(MSG_DEBUG1, "uuid=[%016llX]\n", (long long unsigned int)info.uuid);
    
    dico_add_u16(d, 0, FSYSHEADKEY_NTFSSECTORSIZE, info.bytes_per_sector);
    dico_add_u32(d, 0, FSYSHEADKEY_NTFSCLUSTERSIZE, info.bytes_per_cluster);
    dico_add_u64(d, 0, FSYSHEADKEY_NTFSUUID, info.uuid);
    
    if (ntfs_read_label(fd, label, sizeof(label), &info)!=0)
        snprintf(label, sizeof(label), "<unknonw>");
    
    msgprintf(MSG_DEBUG1, "ntfs_label=[%s]\n", label);
    dico_add_string(d, 0, FSYSHEADKEY_FSLABEL, label);
    
    // ---- minimum fsarchiver version required to restore
    dico_add_u64(d, 0, FSYSHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 5, 9, 0));
    
    // ---- save mount options used at savefs so that restfs can use consistent mount options
    dico_add_string(d, 0, FSYSHEADKEY_MOUNTINFO, "streams_interface=xattr"); // may change in the future
    
ntfs_getinfo_close:
    close(fd);
ntfs_getinfo_return:
    return ret;
}

int ntfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo)
{
    char minversion[1024];
    char streamif[1024];
    int year=0, month=0, day=0;
    char stderrbuf[2048];
    char command[2048];
    char options[1024];
    char delims[]="\n\r";
    char *saveptr;
    char *result;
    u64 instver=0;
    
    // init
    memset(options, 0, sizeof(options));
    snprintf(minversion, sizeof(minversion), "ntfs-3g %.4d.%.2d.%.2d (standard release)", 
        NTFS3G_MINVER_Y, NTFS3G_MINVER_M, NTFS3G_MINVER_D);
    
    // check that mount.ntfs-3g is available
    if (exec_command(command, sizeof(command), NULL, 0, NULL, 0, "which mount.ntfs-3g")!=0)
    {   errprintf("mount.ntfs-3g not found. please install %s\n"
            "or a newer version on your system or check the PATH.\n", minversion);
        return -1;
    }
    
    memset(stderrbuf, 0, sizeof(stderrbuf));
    exec_command(command, sizeof(command), NULL, 0, stderrbuf, sizeof(stderrbuf), "mount.ntfs-3g -h");
    
    // check if there is a recent ntfs-3g version installed
    result=strtok_r(stderrbuf, delims, &saveptr);
    while (result != NULL && instver==0)
    {   if (sscanf(result, "ntfs-3g %4d.%2d.%2d ", &year, &month, &day)==3)
        {   instver=NTFS3G_VERSION(year, month, day);
            msgprintf(MSG_VERB2, "ntfs-3g detected version: year=[%.4d], month=[%.2d], day=[%.2d]\n", year, month, day);
        }
        result = strtok_r(NULL, delims, &saveptr);
    }
    
    if (instver < NTFS3G_VERSION(NTFS3G_MINVER_Y, NTFS3G_MINVER_M, NTFS3G_MINVER_D))
    {
        errprintf("fsarchiver requires %s to operate. The version detected is too old\n", minversion);
        return -1;
    }
    else
    {
        msgprintf(MSG_VERB2, "ntfs-3g has been found: version is %d.%d.%d\n", year, month, day);
    }
    
    // if mntinfo is specified, check which mount option was used at savefs and use the same for consistency
    snprintf(streamif, sizeof(streamif), "xattr"); // set the default "streams_interface" (may change in the future)
    if ((mntinfo!=NULL) && (strlen(mntinfo)>0)) // if a mntinfo has been specified, respect its options
    {
        if (strstr(mntinfo, "streams_interface=xattr")!=NULL) // if "xattr" was used during savefs then use it for restfs
            snprintf(streamif, sizeof(streamif), "xattr");
        else if (strstr(mntinfo, "streams_interface=windows")!=NULL) // if "windows" was used during savefs then use it for restfs
            snprintf(streamif, sizeof(streamif), "windows");
    }
    
    // set mount options
    strlcatf(options, sizeof(options), " -o streams_interface=%s -o efs_raw ", streamif);
    
    if (flags & MS_RDONLY)
        strlcatf(options, sizeof(options), " -o ro ");
    
    // ---- set the advanced filesystem settings from the dico
    if (exec_command(command, sizeof(command), NULL, 0, NULL, 0, "mount.ntfs-3g %s %s %s", options, partition, mntbuf)!=0)
    {   errprintf("command [%s] failed, make sure a recent version of ntfs-3g is installed\n", command);
        return -1;
    }
    
    return 0;
}

int ntfs_umount(char *partition, char *mntbuf)
{
    char command[2048];

    if (exec_command(command, sizeof(command), NULL, 0, NULL, 0, "which fusermount")!=0)
    {   errprintf("fusermount not found. please install fuse on your system or check the PATH.\n");
        return -1;
    }
    
    if (exec_command(command, sizeof(command), NULL, 0, NULL, 0, "fusermount -u %s", mntbuf)!=0)
    {   errprintf("cannot unmount [%s]\n", mntbuf);
        return -1;
    }
    
    return 0;
}

int ntfs_test(char *devname)
{
    char bootsect[16384];
    int fd=-1;
    
    if ((fd=open64(devname, O_RDONLY|O_LARGEFILE))<0)
        return false;
    
    if (read(fd, bootsect, sizeof(bootsect))!=sizeof(bootsect))
    {   close(fd);
        return false;
    }
    
    if (memcmp(bootsect+3, "NTFS", 4) != 0)
    {   close(fd);
        return false;
    }
    
    close(fd);
    return true;
}

int ntfs_get_reqmntopt(char *partition, cstrlist *reqopt, cstrlist *badopt)
{
    if (!reqopt || !badopt)
        return -1;
    
    strlist_add(reqopt, "streams_interface=xattr"); // may change in the future
    return 0;
}

int ntfs_replace_uuid(char *devname, u64 uuid)
{
    u8 bootsect[512];
    int fd=-1;
    
    if ((fd=open64(devname, O_RDWR|O_LARGEFILE))<0)
    {
        errprintf("cannot open(%s, O_RDWR)\n", devname);
        return -1;
    }
    
    if (read(fd, bootsect, sizeof(bootsect))!=sizeof(bootsect))
    {
        errprintf("cannot read the boot sector on %s\n", devname);
        close(fd);
        return -1;
    }
    
    memcpy(bootsect+0x48, &uuid, sizeof(uuid));
    
    if (lseek(fd, 0, SEEK_SET)!=0)
    {
        errprintf("lseek(fd, 0, SEEK_SET) failed\n");
        close(fd);
        return -1;
    }
    
    if (write(fd, bootsect, sizeof(bootsect))!=sizeof(bootsect))
    {
        errprintf("cannot modify the boot sector on %s\n", devname);
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}
