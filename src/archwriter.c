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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <assert.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "options.h"
#include "archwriter.h"
#include "queue.h"
#include "writebuf.h"
#include "comp_gzip.h"
#include "comp_bzip2.h"
#include "error.h"

#define FSA_SMB_SUPER_MAGIC 0x517B
#define FSA_CIFS_MAGIC_NUMBER 0xFF534D42

int archwriter_init(carchwriter *ai)
{
    assert(ai);
    memset(ai, 0, sizeof(struct s_archwriter));
    strlist_init(&ai->vollist);
    ai->newarch=false;
    ai->archfd=-1;
    ai->archid=0;
    ai->curvol=0;
    return 0;
}

int archwriter_destroy(carchwriter *ai)
{
    assert(ai);
    strlist_destroy(&ai->vollist);
    return 0;
}

int archwriter_generate_id(carchwriter *ai)
{
    assert(ai);
    ai->archid=generate_random_u32_id();
    return 0;
}

int archwriter_create(carchwriter *ai)
{
    //char testpath[PATH_MAX];
    //struct statfs svfs;
    //int tempfd;
    struct stat64 st;
    long archflags=0;
    long archperm;
    int res;
    
    assert(ai);
    
    // init
    memset(&st, 0, sizeof(st));
    archflags=O_RDWR|O_CREAT|O_TRUNC|O_LARGEFILE;
    archperm=S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
    
    // if the archive already exists and is a not regular file
    res=stat64(ai->volpath, &st);
    if (res==0 && !S_ISREG(st.st_mode))
    {   errprintf("%s already exists, and is not a regular file.\n", ai->basepath);
        return -1;
    }
    else if ((g_options.overwrite==0) && (res==0) && S_ISREG(st.st_mode)) // archive exists and is a regular file
    {   errprintf("%s already exists, please remove it first.\n", ai->basepath);
        return -1;
    }
    
    // check if it's a network filesystem
    /*snprintf(testpath, sizeof(testpath), "%s.test", ai->volpath);
    if (((tempfd=open64(testpath, basicflags, archperm))<0) ||
        (fstatfs(tempfd, &svfs)!=0) ||
        (close(tempfd)!=0) ||
        (unlink(testpath)!=0))
    {   errprintf("Cannot check the filesystem type on file %s\n", testpath);
        return -1;
    }
    
    if (svfs.f_type==FSA_CIFS_MAGIC_NUMBER || svfs.f_type==FSA_SMB_SUPER_MAGIC)
    {   sysprintf ("writing an archive on a smbfs/cifs filesystem is "
            "not allowed, since it can produce corrupt archives.\n");
        return -1;
    }*/
    
    ai->archfd=open64(ai->volpath, archflags, archperm);
    if (ai->archfd < 0)
    {   sysprintf ("cannot create archive %s\n", ai->volpath);
        return -1;
    }
    ai->newarch=true;
    
    strlist_add(&ai->vollist, ai->volpath);
    
    /* lockf is causing corruption when the archive is written on a smbfs/cifs filesystem */
    /*if (lockf(ai->archfd, F_LOCK, 0)!=0)
    {   sysprintf("Cannot lock archive file: %s\n", ai->volpath);
        close(ai->archfd);
        return -1;
    }*/
    
    return 0;
}

int archwriter_close(carchwriter *ai)
{
    assert(ai);
    
    if (ai->archfd<0)
        return -1;
    
    //res=lockf(ai->archfd, F_ULOCK, 0);
    fsync(ai->archfd); // just in case the user reboots after it exits
    close(ai->archfd);
    ai->archfd=-1;
    
    return 0;
}

int archwriter_remove(carchwriter *ai)
{
    char volpath[PATH_MAX];
    int count;
    int i;
    
    assert(ai);
    
    if (ai->archfd >= 0)
    {
        archwriter_close(ai);
    }
    
    if (ai->newarch==true)
    {
        count=strlist_count(&ai->vollist);
        for (i=0; i < count; i++)
        {
            if (strlist_getitem(&ai->vollist, i, volpath, sizeof(volpath))==0)
            {
                if (unlink(volpath)==0)
                    msgprintf(MSG_FORCE, "removed %s\n", volpath);
                else
                    errprintf("cannot remove %s\n", volpath);
            }
        }
    }
    return 0;
}

s64 archwriter_get_currentpos(carchwriter *ai)
{
    assert(ai);
    return (s64)lseek64(ai->archfd, 0, SEEK_CUR);
}

int archwriter_write_buffer(carchwriter *ai, struct s_writebuf *wb)
{
    struct statvfs64 statvfsbuf;
    char textbuf[128];
    long lres;
    
    assert(ai);
    assert(wb);

    if (wb->size == 0)
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

int archwriter_volpath(carchwriter *ai)
{
    int res;
    res=get_path_to_volume(ai->volpath, PATH_MAX, ai->basepath, ai->curvol);
    return res;
}

int archwriter_is_path_to_curvol(carchwriter *ai, char *path)
{
    assert(ai);
    assert(path);
    return strncmp(ai->volpath, path, PATH_MAX)==0 ? true : false;
}

int archwriter_incvolume(carchwriter *ai, bool waitkeypress)
{
    assert(ai);
    ai->curvol++;
    return archwriter_volpath(ai);
}

int archwriter_write_volheader(carchwriter *ai)
{
    struct s_writebuf *wb=NULL;
    cdico *voldico;
    
    assert(ai);
    
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
    if (archwriter_write_buffer(ai, wb)!=0)
    {   errprintf("archwriter_write_buffer() failed\n");
        return -1;
    }
    
    dico_destroy(voldico);
    writebuf_destroy(wb);
    
    return 0;
}

int archwriter_write_volfooter(carchwriter *ai, bool lastvol)
{
    struct s_writebuf *wb=NULL;
    cdico *voldico;
    
    assert(ai);
    
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
    if (archwriter_write_buffer(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archwriter_write_data(size=%ld) failed\n", (long)wb->size);
        return -1;
    }
    
    dico_destroy(voldico);
    writebuf_destroy(wb);
    
    return 0;
}

int archwriter_split_check(carchwriter *ai, struct s_writebuf *wb)
{
    s64 cursize;
    
    assert(ai);

    if (((cursize=archwriter_get_currentpos(ai))>=0) && (g_options.splitsize>0 && cursize+wb->size > g_options.splitsize))
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

int archwriter_split_if_necessary(carchwriter *ai, struct s_writebuf *wb)
{
    assert(ai);

    if (archwriter_split_check(ai, wb)==true)
    {
        if (archwriter_write_volfooter(ai, false)!=0)
        {   msgprintf(MSG_STACK, "cannot write volume footer: archio_write_volfooter() failed\n");
            return -1;
        }
        archwriter_close(ai);
        archwriter_incvolume(ai, false);
        msgprintf(MSG_VERB2, "Creating new volume: [%s]\n", ai->volpath);
        if (archwriter_create(ai)!=0)
        {   msgprintf(MSG_STACK, "archwriter_create() failed\n");
            return -1;
        }
        if (archwriter_write_volheader(ai)!=0)
        {   msgprintf(MSG_STACK, "cannot write volume header: archio_write_volheader() failed\n");
            return -1;
        }
    }
    return 0;
}

int archwriter_dowrite_block(carchwriter *ai, struct s_blockinfo *blkinfo)
{
    struct s_writebuf *wb=NULL;
    
    assert(ai);

    if ((wb=writebuf_alloc())==NULL)
    {   errprintf("writebuf_alloc() failed\n");
        return -1;
    }
    
    if (writebuf_add_block(wb, blkinfo, ai->archid, blkinfo->blkfsid)!=0)
    {   msgprintf(MSG_STACK, "archio_write_block() failed\n");
        return -1;
    }
    
    if (archwriter_split_if_necessary(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archwriter_split_if_necessary() failed\n");
        return -1;
    }
    
    if (archwriter_write_buffer(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archwriter_write_buffer() failed\n");
        return -1;
    }

    writebuf_destroy(wb);
    return 0;
}

int archwriter_dowrite_header(carchwriter *ai, struct s_headinfo *headinfo)
{
    struct s_writebuf *wb=NULL;
    
    assert(ai);

    if ((wb=writebuf_alloc())==NULL)
    {   errprintf("writebuf_alloc() failed\n");
        return -1;
    }
    
    if (writebuf_add_header(wb, headinfo->dico, headinfo->magic, ai->archid, headinfo->fsid)!=0)
    {   msgprintf(MSG_STACK, "archio_write_block() failed\n");
        return -1;
    }
    
    if (archwriter_split_if_necessary(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archwriter_split_if_necessary() failed\n");
        return -1;
    }
    
    if (archwriter_write_buffer(ai, wb)!=0)
    {   msgprintf(MSG_STACK, "archwriter_write_buffer() failed\n");
        return -1;
    }
    
    writebuf_destroy(wb);
    return 0;
}
