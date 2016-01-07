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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <gcrypt.h>

#include "fsarchiver.h"
#include "datafile.h"
#include "common.h"
#include "error.h"

struct s_datafile 
{   int  fd; // file descriptor
    bool simul; // simulation: don't write anything if true
    bool open; // true when file is open even if simulation
    bool sparse; // true if that's a sparse file
    char path[PATH_MAX]; // path to file
    gcry_md_hd_t md5ctx; // struct for md5
};

cdatafile *datafile_alloc()
{
    cdatafile *f;
    if ((f=malloc(sizeof(cdatafile)))==NULL)
        return NULL;
    f->path[0]=0;
    f->fd=-1;
    f->simul=false;
    f->open=false;
    f->sparse=false;
    return f;
}

int datafile_destroy(cdatafile *f)
{
    assert(f);
    
    if (f->open)
        datafile_close(f, NULL, 0);
    
    free(f);
    return 0;
}

int datafile_open_write(cdatafile *f, char *path, bool simul, bool sparse)
{
    assert(f);
    
    if (f->open)
    {   errprintf("File is already open\n");
        return -1;
    }
    
    if (simul==false)
    {
        errno=0;
        if ((f->fd=open64(path, O_RDWR|O_CREAT|O_TRUNC|O_LARGEFILE, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))<0)
        {   if (errno==ENOSPC)
            {   sysprintf("can't write file [%s]: no space left on device\n", path);
                return -1; // fatal error
            }
            else
            {   sysprintf("Cannot open %s for writing\n", path);
                return -1;
            }
        }
    }
    
    if (gcry_md_open(&f->md5ctx, GCRY_MD_MD5, 0) != GPG_ERR_NO_ERROR)
    {   errprintf("gcry_md_open() failed\n");
        return -1;
    }
    
    snprintf(f->path, PATH_MAX, "%s", path);
    f->simul=simul;
    f->open=true;
    f->sparse=sparse;
    return 0;
}

int datafile_is_block_zero(cdatafile *f, char *data, u64 len)
{
    bool zero=true;
    u64 pos;
    
    for (pos=0; (pos<len) && (zero==true); pos++)
        if (data[pos]!=0)
            zero=false;
    
    return zero;
}

int datafile_write(cdatafile *f, char *data, u64 len)
{
    s64 lres;
    
    assert(f);
    
    if (!f->open)
    {   errprintf("File is not open\n");
        return FSAERR_NOTOPEN;
    }
    
    if (f->simul==false)
    {
        if ((f->sparse==true) && (datafile_is_block_zero(f, data, len)))
        {
            if (lseek64(f->fd, len, SEEK_CUR)<0)
            {   sysprintf("Can't lseek64() in file [%s]\n", f->path);
                return FSAERR_SEEK;
            }
        }
        else
        {
            errno=0;
            if ((lres=write(f->fd, data, len))!=len) // error
            {
                if ((errno==ENOSPC) || ((lres>0) && (lres < len)))
                {   sysprintf("Can't write file [%s]: no space left on device\n", f->path);
                    return FSAERR_ENOSPC;
                }
                else // another error
                {   sysprintf("cannot write %s: size=%ld\n", f->path, (long)len);
                    return FSAERR_WRITE;
                }
            }
        }
    }
    
    gcry_md_write(f->md5ctx, data, len);
    
    return FSAERR_SUCCESS;
}

int datafile_close(cdatafile *f, u8 *md5bufdat, int md5bufsize)
{
    char md5store[16];
    u8 *md5tmp;
    int res=0;
    
    assert(f);
    
    if (!f->open)
    {   errprintf("File is not open\n");
        return -1;
    }
    
    if ((md5tmp=gcry_md_read(f->md5ctx, GCRY_MD_MD5))==NULL)
    {   errprintf("gcry_md_read() failed\n");
        return -1;
    }
    memcpy(md5store, md5tmp, 16);
    gcry_md_close(f->md5ctx);
    
    if (md5bufdat!=NULL)
    {
        if (md5bufsize < 16)
        {   errprintf("Buffer too small for md5 checksum\n");
            return -1;
        }
        memcpy(md5bufdat, md5store, 16);
    }
    
    if ((f->open==true) && (f->simul==false))
    {
        if ((f->sparse==true) && (ftruncate(f->fd, lseek64(f->fd, 0, SEEK_CUR))<0))
        {   sysprintf("ftruncate() failed for file [%s]\n", f->path);
            res=-1;
        }
        res=min(close(f->fd), res);
    }
    
    f->open=false;
    f->path[0]=0;
    f->fd=-1;
    
    return res;
}
