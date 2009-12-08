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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/statvfs.h>

#include "dico.h"
#include "common.h"
#include "archive.h"
#include "queue.h"
#include "comp_gzip.h"
#include "comp_bzip2.h"

int archive_init(carchive *ai)
{
    assert(ai);
    memset(ai, 0, sizeof(struct s_archive));
    ai->cryptalgo=ENCRYPT_NULL;
    ai->compalgo=COMPRESS_NULL;
    ai->createdbyfsa=false;
    ai->fsacomp=-1;
    ai->complevel=-1;
    ai->archfd=-1;
    ai->archid=0;
    ai->locked=false;
    ai->curvol=0;
    return 0;
}

int archive_destroy(carchive *ai)
{
    assert(ai);
    return 0;
}

int archive_generate_id(carchive *ai)
{
    assert(ai);
    ai->archid=generate_random_u32_id();
    return 0;
}

int archive_create(carchive *ai)
{
    struct stat64 st;
    int res;
    
    assert(ai);
    
    memset(&st, 0, sizeof(st));
    res=stat64(ai->volpath, &st);
    
    // if the archive already exists and is a not regular file
    if (res==0 && !S_ISREG(st.st_mode))
    {   errprintf("%s already exists, and is not a regular file.\n", ai->basepath);
        return -1;
    }
    else if ((g_options.overwrite==0) && (res==0) && S_ISREG(st.st_mode)) // archive exists and is a regular file
    {   errprintf("%s already exists, please remove it first.\n", ai->basepath);
        return -1;
    }
    
    ai->archfd=open64(ai->volpath, O_RDWR|O_CREAT|O_TRUNC|O_LARGEFILE, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (ai->archfd<0)
    {      sysprintf ("cannot create archive %s\n", ai->volpath);
        return -1;
    }
    
    ai->createdbyfsa=true;
    
    if (lockf(ai->archfd, F_LOCK, 0)!=0)
    {   sysprintf("Cannot lock archive file: %s\n", ai->volpath);
        close(ai->archfd);
        return -1;
    }
    ai->locked=true;
    
    return 0;
}

int archive_open(carchive *ai)
{   
    struct stat64 st;
    
    assert(ai);

    ai->archfd=open64(ai->volpath, O_RDONLY|O_LARGEFILE);
    if (ai->archfd<0)
    {      sysprintf ("cannot open archive %s\n", ai->volpath);
        return -1;
    }
    
    if (fstat64(ai->archfd, &st)!=0)
    {   sysprintf("fstat64(%s) failed\n", ai->volpath);
        return -1;
    }
    
    if (!S_ISREG(st.st_mode))
    {   errprintf("%s is not a regular file, cannot continue\n", ai->volpath);
        close(ai->archfd);
        return -1;
    }
    
    return 0;
}

int archive_close(carchive *ai)
{
    int res;
    
    assert(ai);
    
    if (ai->archfd<0)
    {   //errprintf("invalid file descriptor: %d\n", (int)ai->archfd);
        return -1;
    }
    
    if (ai->locked)
        res=lockf(ai->archfd, F_ULOCK, 0);
    fsync(ai->archfd); // just in case the user reboots after it exits
    close(ai->archfd);
    ai->archfd=-1;

    return 0;
}

int archive_remove(carchive *ai)
{
    assert(ai);
    
    if (ai->archfd!=-1)
        archive_close(ai);
    if (ai->createdbyfsa)
    {   
        unlink(ai->basepath);
        msgprintf(MSG_FORCE, "removing %s\n", ai->basepath);
    }
    
    return 0;
}

s64 archive_get_currentpos(carchive *ai)
{
    assert(ai);
    return (s64)lseek64(ai->archfd, 0, SEEK_CUR);
}

int archive_read_data(carchive *ai, void *data, u64 size)
{
    long lres;
    
    assert(ai);

    if ((lres=read(ai->archfd, (char*)data, (long)size))!=(long)size)
    {   sysprintf("read failed: read(size=%ld)=%ld\n", (long)size, lres);
        return -1;
    }
    
    return 0;
}

int archive_read_dico(carchive *ai, cdico *d)
{
    u16 size;
    u16 headerlen;
    u32 origsum;
    u32 newsum;
    u8 *buffer;
    u8 *bufpos;
    u16 temp16;
    u32 temp32;
    u8 section;
    u16 count;
    u8 type;
    u16 key;
    int i;
    
    assert(ai);
    assert(d);
    
    // header-len, header-data, header-checksum
    if (archive_read_data(ai, &temp16, sizeof(temp16))!=0)
    {   errprintf("imgdisk_read_data() failed\n");
        return ERR_FATAL;
    }
    headerlen=le16_to_cpu(temp16);
    
    bufpos=buffer=malloc(headerlen);
    if (!buffer)
    {   errprintf("cannot allocate memory for header\n");
        return ERR_FATAL;
    }
    
    if (archive_read_data(ai, buffer, headerlen)!=0)
    {   errprintf("cannot read header data\n");
        free(buffer);
        return ERR_FATAL;
    }
    
    if (archive_read_data(ai, &temp32, sizeof(temp32))!=0)
    {   errprintf("cannot read header checksum\n");
        free(buffer);
        return ERR_FATAL;
    }
    origsum=le32_to_cpu(temp32);
    
    // check header-data integrity using checksum    
    newsum=fletcher32(buffer, headerlen);
    
    if (newsum!=origsum)
    {   errprintf("bad checksum for header\n");
        free(buffer);
        return ERR_MINOR; // header corrupt --> skip file
    }
    
    // read count from buffer
    memcpy(&temp16, bufpos, sizeof(temp16));
    bufpos+=sizeof(temp16);
    count=le16_to_cpu(temp16);
    
    // read items
    for (i=0; i < count; i++)
    {
        // a. read type from buffer
        memcpy(&type, bufpos, sizeof(type));
        bufpos+=sizeof(section);
        
        // b. read section from buffer
        memcpy(&section, bufpos, sizeof(section));
        bufpos+=sizeof(section);
        
        // c. read key from buffer
        memcpy(&temp16, bufpos, sizeof(temp16));
        bufpos+=sizeof(temp16);
        key=le16_to_cpu(temp16);
        
        // d. read sizeof(data)
        memcpy(&temp16, bufpos, sizeof(temp16));
        bufpos+=sizeof(temp16);
        size=le16_to_cpu(temp16);
        
        // e. add item to dico
        if (dico_add_generic(d, section, key, bufpos, size, type)!=0)
            return ERR_FATAL;
        bufpos+=size;
    }
    
    free(buffer);
    return ERR_SUCCESS;
}

int archive_read_header(carchive *ai, char *magic, cdico **d, bool allowseek, u16 *fsid)
{
    s64 curpos;
    u16 temp16;
    u32 temp32;
    u32 archid;
    int res;
    
    assert(ai);
    assert(d);
    assert(fsid);
    
    // init
    memset(magic, 0, FSA_SIZEOF_MAGIC);
    *fsid=FSA_FILESYSID_NULL;
    *d=NULL;
    
    if ((*d=dico_alloc())==NULL)
    {   errprintf("dico_alloc() failed\n");
        return ERR_FATAL;
    }
    
    // search for next read header marker and magic (it may be further if corruption in archive)
    if ((curpos=lseek64(ai->archfd, 0, SEEK_CUR))<0)
    {   sysprintf("lseek64() failed to get the current position in archive\n");
        return ERR_FATAL;
    }
    
    if ((res=archive_read_data(ai, magic, FSA_SIZEOF_MAGIC))!=ERR_SUCCESS)
    {   msgprintf(MSG_STACK, "cannot read header magic: res=%d\n", res);
        return ERR_FATAL;
    }
    
    // we don't want to search for the magic if it's a volume header
    if (is_magic_valid(magic)!=true && allowseek!=true)
    {   errprintf("cannot read header magic: this is not a valid fsarchiver file, or it has been created with a different version.\n");
        return ERR_FATAL;
    }
    
    while (is_magic_valid(magic)!=true)
    {
        if (lseek64(ai->archfd, curpos++, SEEK_SET)<0)
        {   sysprintf("lseek64(pos=%lld, SEEK_SET) failed\n", (long long)curpos);
            return ERR_FATAL;
        }
        if ((res=archive_read_data(ai, magic, FSA_SIZEOF_MAGIC))!=ERR_SUCCESS)
        {   msgprintf(MSG_STACK, "cannot read header magic: res=%d\n", res);
            return ERR_FATAL;
        }
    }
    
    // read the archive id
    if ((res=archive_read_data(ai, &temp32, sizeof(temp32)))!=ERR_SUCCESS)
    {   msgprintf(MSG_STACK, "cannot read archive-id in header: res=%d\n", res);
        return ERR_FATAL;
    }
    archid=le32_to_cpu(temp32);
    if (ai->archid) // only check archive-id if it's known (when main header has been read)
    {
        if (archid!=ai->archid)
        {   errprintf("archive-id in header does not match: archid=[%.8x], expected=[%.8x]\n", archid, ai->archid);
            return ERR_MINOR;
        }
    }
    
    // read the filesystem id
    if ((res=archive_read_data(ai, &temp16, sizeof(temp16)))!=ERR_SUCCESS)
    {   msgprintf(MSG_STACK, "cannot read filesystem-id in header: res=%d\n", res);
        return ERR_FATAL;
    }
    *fsid=le16_to_cpu(temp16);
    
    // read the dico of the header
    if ((res=archive_read_dico(ai, *d))!=ERR_SUCCESS)
    {   msgprintf(MSG_STACK, "imgdisk_read_dico() failed\n");
        return res;
    }
    
    return ERR_SUCCESS;
}

int archive_write_data(carchive *ai, void *data, u64 size)
{
    struct statvfs64 statvfsbuf;
    char textbuf[128];
    long lres;
    
    assert(ai);
    assert(data);
    assert(size>0);
    
    if ((lres=write(ai->archfd, (char*)data, (long)size))!=(long)size)
    {
        errprintf("write(size=%ld) returned %ld\n", (long)size, (long)lres);
        if ((lres>0) && (lres < (long)size)) // probably "no space left"
        {
            if (fstatvfs64(ai->archfd, &statvfsbuf)!=0)
            {   sysprintf("fstatvfs(fd=%d) failed\n", ai->archfd);
                return -1;
            }
            
            u64 freebytes = statvfsbuf.f_bfree * statvfsbuf.f_bsize;
            errprintf("Can't write to the archive file. Space on device is %s. \n"
                "If the archive is being written to a FAT filesystem, you may have reached \n"
                "the maximum filesize that it can handle (in general 2 GB)\n", 
                format_size(freebytes, textbuf, sizeof(textbuf), 'h'));
            return -1;
        }
        else // another error
        {
            sysprintf("write(size=%ld) failed\n", (long)size);
            return -1;
        }
    }
    
    return 0;
}

// calculates the volpath using basepath and curvol
int archive_volpath(carchive *ai)
{
    char temp[PATH_MAX];
    int pathlen;
    
    assert(ai);
    
    if ((pathlen=strlen(ai->basepath))<4) // all archives terminates with ".fsa"
    {   errprintf("archive has an invalid basepath: [%s]\n", ai->basepath);
        return -1;
    }
    
    memset(ai->volpath, 0, PATH_MAX);
    memcpy(ai->volpath, ai->basepath, pathlen-2);
    if (ai->curvol==0) // first volume
    {
        if (realpath(ai->basepath, ai->volpath)!=ai->volpath)
            snprintf(ai->volpath, PATH_MAX, "%s", ai->basepath);
    }
    else // not the first volume
    {
        if (ai->curvol<100) // 1..99
            snprintf(temp, sizeof(temp), "%.2ld", (long)ai->curvol);
        else // >=100
            snprintf(temp, sizeof(temp), "%ld", (long)ai->curvol);
        strlcat(ai->volpath, temp, PATH_MAX);
    }
    
    return 0;
}

int archive_is_path_to_curvol(carchive *ai, char *path)
{
    assert(ai);
    assert(path);
    return strncmp(ai->volpath, path, PATH_MAX)==0 ? true : false;
}

int archive_incvolume(carchive *ai, bool waitkeypress)
{
    assert(ai);
    ai->curvol++;
    return archive_volpath(ai);
}
