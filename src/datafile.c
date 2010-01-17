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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>

#include "fsarchiver.h"
#include "datafile.h"
#include "common.h"
#include "error.h"
#include "md5.h"

struct s_datafile 
{   int  fd; // file descriptor
    bool simul; // simulation: don't write anything if true
    bool open; // true when file is open even if simulation
    char path[PATH_MAX]; // path to file
    struct md5_ctx md5ctx; // struct for md5
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

int datafile_open_write(cdatafile *f, char *path, bool simul)
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
    
    md5_init_ctx(&f->md5ctx);
    snprintf(f->path, PATH_MAX, "%s", path);
    f->simul=simul;
    f->open=true;
    return 0;
}

int datafile_write(cdatafile *f, char *data, u64 len)
{
    s64 lres;
    
    assert(f);
    
    if (!f->open)
    {   errprintf("File is not open\n");
        return -1;
    }
    
    if (f->simul==false)
    {
        errno=0;
        if ((lres=write(f->fd, data, len))!=len) // error
        {
            if ((errno==ENOSPC) || ((lres>0) && (lres < len)))
            {   sysprintf("Can't write file [%s]: no space left on device\n", f->path);
                return -1; // fatal error
            }
            else // another error
            {   sysprintf("cannot write %s: size=%ld\n", f->path, (long)len);
                return -1;
            }
        }
    }
    
    md5_process_bytes(data, len, &f->md5ctx);
    return 0;
}

int datafile_close(cdatafile *f, u8 *md5bufdat, int md5bufsize)
{
    char md5temp[16];
    int res=0;
    
    assert(f);
    
    if (!f->open)
    {   errprintf("File is not open\n");
        return -1;
    }
    
    md5_finish_ctx(&f->md5ctx, md5temp);
    
    if (md5bufdat!=NULL)
    {
        if (md5bufsize < 16)
        {   errprintf("Buffer too small for md5 checksum\n");
            return -1;
        }
        memcpy(md5bufdat, md5temp, 16);
    }
    
    if ((f->open==true) && (f->simul==false))
        res=close(f->fd);
    
    f->open=false;
    f->path[0]=0;
    f->fd=-1;
    
    return res;
}
