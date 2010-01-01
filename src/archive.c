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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <assert.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "options.h"
#include "archive.h"
#include "queue.h"
#include "writebuf.h"
#include "comp_gzip.h"
#include "comp_bzip2.h"
#include "error.h"

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

int archive_write_buffer(carchive *ai, struct s_writebuf *wb)
{
    struct statvfs64 statvfsbuf;
    char textbuf[128];
    long lres;
    
    assert(ai);
    assert(wb);
    
    if (wb->size <=0)
    {   errprintf("wb->size=%ld\n", (long)wb->size);
        return -1;
    }
    
    if ((lres=write(ai->archfd, (char*)wb->data, (long)wb->size))!=(long)wb->size)
    {
        errprintf("write(size=%ld) returned %ld\n", (long)wb->size, (long)lres);
        if ((lres>0) && (lres < (long)wb->size)) // probably "no space left"
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
            sysprintf("write(size=%ld) failed\n", (long)wb->size);
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

int archive_write_volheader(carchive *ai)
{
    struct s_writebuf *wb=NULL;
    cdico *voldico;
    
    if (ai==NULL)
    {   errprintf("ai is NULL\n");
        return -1;
    }
    
    if ((wb=writebuf_alloc())==NULL)
    {   msgprintf(MSG_STACK, "writebuf_alloc() failed\n");
        return -1;
    }
    
    if ((voldico=dico_alloc())==NULL)
    {   msgprintf(MSG_STACK, "voldico=dico_alloc() failed\n");
        return -1;
    }
    
    // prepare header
    dico_add_u32(voldico, 0, VOLUMEHEADKEY_VOLNUM, ai->curvol);
    dico_add_u32(voldico, 0, VOLUMEHEADKEY_ARCHID, ai->archid);
    dico_add_string(voldico, 0, VOLUMEHEADKEY_FILEFORMATVER, FSA_FILEFORMAT);
    dico_add_string(voldico, 0, VOLUMEHEADKEY_PROGVERCREAT, FSA_VERSION);
    
    // write header to buffer
    if (writebuf_add_header(wb, voldico, FSA_MAGIC_VOLH, ai->archid, FSA_FILESYSID_NULL)!=0)
    {   errprintf("archio_write_header() failed\n");
        return -1;
    }
    
    // write header to file
    if (archive_write_buffer(ai, wb)!=0)
    {   errprintf("archive_write_buffer() failed\n");
        return -1;
    }
    
    dico_destroy(voldico);
    writebuf_destroy(wb);
    
    return 0;
}

int archive_write_volfooter(carchive *ai, bool lastvol)
{
    struct s_writebuf *wb=NULL;
    cdico *voldico;
    
    if (ai==NULL)
    {   errprintf("ai is NULL\n");
        return -1;
    }
    
    if ((wb=writebuf_alloc())==NULL)
    {   errprintf("writebuf_alloc() failed\n");
        return -1;
    }
    
    if ((voldico=dico_alloc())==NULL)
    {   errprintf("voldico=dico_alloc() failed\n");
        return -1;
    }
    
    // prepare header
    dico_add_u32(voldico, 0, VOLUMEFOOTKEY_VOLNUM, ai->curvol);
    dico_add_u32(voldico, 0, VOLUMEFOOTKEY_ARCHID, ai->archid);
    dico_add_u32(voldico, 0, VOLUMEFOOTKEY_LASTVOL, lastvol);
    
    // write header to buffer
    if (writebuf_add_header(wb, voldico, FSA_MAGIC_VOLF, ai->archid, FSA_FILESYSID_NULL)!=0)
    {   msgprintf(MSG_STACK, "archio_write_header() failed\n");
        return -1;
    }
    
    // write header to file
    if (archive_write_buffer(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archive_write_data(size=%ld) failed\n", (long)wb->size);
        return -1;
    }
    
    dico_destroy(voldico);
    writebuf_destroy(wb);
    
    return 0;
}

int archive_split_check(carchive *ai, struct s_writebuf *wb)
{
    s64 cursize;
    
    if (((cursize=archive_get_currentpos(ai))>=0) && (g_options.splitsize>0 && cursize+wb->size > g_options.splitsize))
    {
        msgprintf(MSG_DEBUG4, "splitchk: YES --> cursize=%lld, g_options.splitsize=%lld, cursize+wb->size=%lld, wb->size=%lld\n",
            (long long)cursize, (long long)g_options.splitsize, (long long)cursize+wb->size, (long long)wb->size);
        return true;
    }
    else
    {
        msgprintf(MSG_DEBUG4, "splitchk: NO --> cursize=%lld, g_options.splitsize=%lld, cursize+wb->size=%lld, wb->size=%lld\n",
            (long long)cursize, (long long)g_options.splitsize, (long long)cursize+wb->size, (long long)wb->size);
        return false;
    }
}

int archive_split_if_necessary(carchive *ai, struct s_writebuf *wb)
{
    if (archive_split_check(ai, wb)==true)
    {
        if (archive_write_volfooter(ai, false)!=0)
        {   msgprintf(MSG_STACK, "cannot write volume footer: archio_write_volfooter() failed\n");
            return -1;
        }
        archive_close(ai);
        archive_incvolume(ai, false);
        msgprintf(MSG_VERB2, "Creating new volume: [%s]\n", ai->volpath);
        if (archive_create(ai)!=0)
        {   msgprintf(MSG_STACK, "archive_create() failed\n");
            return -1;
        }
        if (archive_write_volheader(ai)!=0)
        {   msgprintf(MSG_STACK, "cannot write volume header: archio_write_volheader() failed\n");
            return -1;
        }
    }
    return 0;
}

int archive_dowrite_block(carchive *ai, struct s_blockinfo *blkinfo)
{
    struct s_writebuf *wb=NULL;
    
    if ((wb=writebuf_alloc())==NULL)
    {   errprintf("writebuf_alloc() failed\n");
        return -1;
    }
    
    if (writebuf_add_block(wb, blkinfo, ai->archid, blkinfo->blkfsid)!=0)
    {   msgprintf(MSG_STACK, "archio_write_block() failed\n");
        return -1;
    }
    
    if (archive_split_if_necessary(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archive_split_if_necessary() failed\n");
        return -1;
    }
    
    if (archive_write_buffer(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archive_write_buffer() failed\n");
        return -1;
    }

    writebuf_destroy(wb);
    return 0;
}

int archive_dowrite_header(carchive *ai, struct s_headinfo *headinfo)
{
    struct s_writebuf *wb=NULL;
    
    if ((wb=writebuf_alloc())==NULL)
    {   errprintf("writebuf_alloc() failed\n");
        return -1;
    }
    
    if (writebuf_add_header(wb, headinfo->dico, headinfo->magic, ai->archid, headinfo->fsid)!=0)
    {   msgprintf(MSG_STACK, "archio_write_block() failed\n");
        return -1;
    }
    
    if (archive_split_if_necessary(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archive_split_if_necessary() failed\n");
        return -1;
    }
    
    if (archive_write_buffer(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archive_write_buffer() failed\n");
        return -1;
    }
    
    writebuf_destroy(wb);
    return 0;
}
