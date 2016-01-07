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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fsarchiver.h"
#include "dico.h"
#include "regmulti.h"
#include "common.h"
#include "queue.h"
#include "error.h"

int regmulti_empty(cregmulti *m)
{
    int i;
    
    if (!m)
    {   errprintf("invalid param\n");
        return -1;
    }
    
    m->count=0;
    m->usedsize=0;
    
    for (i=0; i < m->maxitems; i++)
        m->objhead[i]=NULL;
    
    return 0;
}

int regmulti_init(cregmulti *m, u32 maxblksize)
{
    if (!m)
    {   errprintf("invalid param\n");
        return -1;
    }
    
    m->maxitems=FSA_MAX_SMALLFILECOUNT;
    m->maxblksize=min(maxblksize, FSA_MAX_BLKSIZE);
    return regmulti_empty(m);
}

int regmulti_count(cregmulti *m, cdico *header, char *data, u32 datsize)
{
    if (!m)
    {   errprintf("invalid param\n");
        return -1;
    }

    return m->count;
}

bool regmulti_save_enough_space_for_new_file(cregmulti *m, u32 filesize)
{
    if (!m)
    {   errprintf("invalid param\n");
        return false;
    }
    
    if (m->count >= m->maxitems)
        return false;
    if (m->usedsize + filesize > m->maxblksize)
        return false;
    return true;
}

int regmulti_save_addfile(cregmulti *m, cdico *header, char *data, u32 datsize)
{
    if (!m)
    {   errprintf("invalid param\n");
        return -1;
    }

    if (m->count >= m->maxitems)
    {   errprintf("regmulti is full: it contains %ld items\n", (long)m->count);
        return -1;
    }

    if (m->usedsize+datsize > m->maxblksize)
    {   errprintf("block is too small to store that new sub-block of data\n");
        return -1;
    }
    
    m->objhead[m->count]=header;
    memcpy(m->data+m->usedsize, data, datsize);
    m->usedsize+=datsize;
    m->count++;
    return 0;
}

// add headers and datblock at the end of the queue
int regmulti_save_enqueue(cregmulti *m, cqueue *q, int fsid)
{
    cblockinfo blkinfo;
    char *dynblock;
    u32 offset=0;
    u64 filesize;
    int i;
    
    if (!m)
    {   errprintf("invalid param\n");
        return -1;
    }
    
    // don't do anything if block is empty
    if (m->count==0)
        return 0;
    
    for (i=0; i < m->count; i++)
    {
        if (m->objhead[i]==NULL)
        {   errprintf("error: objhead[%d]==NULL\n", i);
            return -1;
        }
        
        // get file size from header
        if (dico_get_u64(m->objhead[i], DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_SIZE, &filesize)!=0)
        {   errprintf("Cannot read filesize DISKITEMKEY_SIZE from archive\n");
            return -1;
        }
        
        // the extraction function needs to know how many small-files are packed together
        if (dico_add_u32(m->objhead[i], DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_MULTIFILESCOUNT, (u32)m->count)!=0)
        {   errprintf("dico_add_u32(DISKITEMKEY_MULTIFILESCOUNT) failed\n");
            return -1;
        }
        
        // the extraction function needs to know where the data for this file are in the block
        if (dico_add_u32(m->objhead[i], DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_MULTIFILESOFFSET, (u32)offset)!=0)
        {   errprintf("dico_add_u32(DISKITEMKEY_MULTIFILESCOUNT) failed\n");
            return -1;
        }
        offset+=(u32)filesize;
        
        if (queue_add_header(q, m->objhead[i], FSA_MAGIC_OBJT, fsid)!=0)
        {   errprintf("queue_add_header() failed\n");
            return -1;
        }
    }
    
    // make a copy of the static block to dynamic memory
    if ((dynblock=malloc(m->usedsize)) == NULL)
    {   errprintf("malloc(%ld) failed: out of memory\n", (long)m->usedsize);
        return -1;
    }
    memcpy(dynblock, m->data, m->usedsize);
    
    memset(&blkinfo, 0, sizeof(blkinfo));
    blkinfo.blkrealsize=m->usedsize;
    blkinfo.blkdata=(char*)dynblock;
    blkinfo.blkoffset=0; // no meaning for multi-regfiles
    blkinfo.blkfsid=fsid;
    if (queue_add_block(q, &blkinfo, QITEM_STATUS_TODO)!=0)
    {   errprintf("queue_add_block() failed\n");
        return -1;
    }
    
     return 0;
}

int regmulti_rest_addheader(cregmulti *m, cdico *header)
{
    if (!m)
    {   errprintf("invalid param\n");
        return -1;
    }
    
    if (m->count >= m->maxitems)
    {   errprintf("regmulti is full: it contains %ld items\n", (long)m->count);
        return -1;
    }
    
    m->objhead[m->count]=header;
    m->count++;
    return 0;
}

int regmulti_rest_setdatablock(cregmulti *m, char *data, u32 datsize)
{
    if (!m)
    {   errprintf("invalid param\n");
        return -1;
    }

    if (m->usedsize+datsize > m->maxblksize)
    {   errprintf("block is too small to store that new sub-block of data\n");
        return -1;
    }
    
    memcpy(m->data, data, datsize);
    m->usedsize=datsize;

    return 0;
}

int regmulti_rest_getfile(cregmulti *m, int index, cdico **filehead, char *data, u64 *datsize, u32 bufsize)
{
    u32 offset;
    u64 filesize;
    
    if (!m || !filehead)
    {   errprintf("invalid param\n");
        return -1;
    }
    
    if (index >= m->count)
    {   errprintf("index=%d out of scope: the structure only contains %ld items\n", index, (long)m->count);
        return -1;
    }
    
    // ---- return the header to the calling function
    *filehead=m->objhead[index];
    
    // ---- return the data to the calling function
    if (dico_get_u64(m->objhead[index], DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_SIZE, &filesize)!=0)
    {   errprintf("Cannot read filesize DISKITEMKEY_SIZE from archive\n");
        return -1;
    }
    if (dico_get_u32(m->objhead[index], DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_MULTIFILESOFFSET, &offset)!=0)
    {   errprintf("Cannot read filesize DISKITEMKEY_SIZE from archive\n");
        return -1;
    }
    *datsize=filesize;
    memcpy(data, m->data+offset, filesize);
    
    return 0;
}
