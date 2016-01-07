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
#include <stdio.h>

#include "fsarchiver.h"
#include "writebuf.h"
#include "common.h"
#include "error.h"
#include "queue.h"
#include "dico.h"

cwritebuf *writebuf_alloc()
{
    cwritebuf *wb;
    if ((wb=malloc(sizeof(cwritebuf)))==NULL)
    {   errprintf("malloc(%d) failed: cannot allocate memory for writebuf\n", (int)sizeof(cwritebuf));
        return NULL;
    }
    wb->size=0;
    wb->data=NULL;
    return wb;
}

int writebuf_destroy(cwritebuf *wb)
{
    if (wb==NULL)
    {   errprintf("wb is NULL\n");
        return -1;
    }
    
    if (wb->data)
    {
        free(wb->data);
        wb->data=NULL;
    }
    wb->size=0;
    free(wb);
    return 0;
}

int writebuf_add_data(cwritebuf *wb, void *data, u64 size)
{
    u64 newsize;
    
    if (wb==NULL)
    {   errprintf("wb is NULL\n");
        return -1;
    }
    
    if (size==0)
    {   errprintf("size=0\n");
        return -1;
    }
    
    newsize=wb->size+size;
    wb->data=realloc(wb->data, newsize+4); // "+4" required else the last byte of the buffer may be alterred (see release-0.3.3)
    if (!wb->data)
    {   errprintf("realloc(oldsize=%ld, newsize=%ld) failed\n", (long)wb->size, (long)newsize+4);
        return -1;
    }
    memcpy(wb->data+wb->size, data, size);
    
    wb->size+=size;
    return 0;
}

int writebuf_add_dico(cwritebuf *wb, cdico *d, char *magic)
{
    struct s_dicoitem *item;
    int itemnum;
    u32 headerlen;
    u32 checksum;
    u8 *buffer;
    u8 *bufpos;
    u16 temp16;
    u32 temp32;
    u16 count;
    
    if (!wb || !d)
    {   errprintf("a parameter is null\n");
        return -1;
    }
    
    // 0. debugging
    msgprintf(MSG_DEBUG2, "archio_write_dico(wb=%p, dico=%p, magic=[%c%c%c%c])\n", wb, d, magic[0], magic[1], magic[2], magic[3]);
    for (item=d->head; item!=NULL; item=item->next)
        if ((item->section==DICO_OBJ_SECTION_STDATTR) && (item->key==DISKITEMKEY_PATH) && (memcmp(magic, "ObJt", 4)==0))
            msgprintf(MSG_DEBUG2, "filepath=[%s]\n", item->data);
    
    // 1. how many valid items there are
    count=dico_count_all_sections(d);
    msgprintf(MSG_DEBUG2, "dico_count_all_sections(dico=%p)=%d\n", d, (int)count);
    
    // 2. calculate len of header
    headerlen=sizeof(u16); // count
    for (item=d->head; item!=NULL; item=item->next)
    {
        headerlen+=sizeof(u8); // type
        headerlen+=sizeof(u8); // section
        headerlen+=sizeof(u16); // key
        headerlen+=sizeof(u16); // data size
        headerlen+=item->size; // data
    }
    msgprintf(MSG_DEBUG2, "calculated headerlen for that dico: headerlen=%d\n", (int)headerlen);
    
    // 3. allocate memory for header
    bufpos=buffer=malloc(headerlen);
    if (!buffer)
    {   errprintf("cannot allocate memory for buffer");
        return -1;
    }
    
    // 4. write items count in buffer
    temp16=cpu_to_le16(count);
    msgprintf(MSG_DEBUG2, "mempcpy items count to buffer: u16 count=%d\n", (int)count);
    bufpos=mempcpy(bufpos, &temp16, sizeof(temp16));
    
    // 5. write all items in buffer
    for (item=d->head, itemnum=0; item!=NULL; item=item->next, itemnum++)
    {
        msgprintf(MSG_DEBUG2, "itemnum=%d (type=%d, section=%d, key=%d, size=%d)\n", 
            (int)itemnum++, (int)item->type, (int)item->section, (int)item->key, (int)item->size);
        
        // a. write data type buffer
        bufpos=mempcpy(bufpos, &item->type, sizeof(item->type));
        
        // b. write section to buffer
        bufpos=mempcpy(bufpos, &item->section, sizeof(item->section));
        
        // c. write key to buffer
        temp16=cpu_to_le16(item->key);
        bufpos=mempcpy(bufpos, &temp16, sizeof(temp16));
        
        // d. write sizeof(data) to buffer
        temp16=cpu_to_le16(item->size);
        bufpos=mempcpy(bufpos, &temp16, sizeof(temp16));
        
        // e. write data to buffer
        if (item->size>0)
            bufpos=mempcpy(bufpos, item->data, item->size);
    }
    msgprintf(MSG_DEBUG2, "all %d items mempcopied to buffer\n", (int)itemnum);
    
    // 6. write header-len, header-data, header-checksum
    temp32=cpu_to_le32(headerlen);
    if (writebuf_add_data(wb, &temp32, sizeof(temp32))!=0)
    {   free(buffer);
        return -1;
    }
    
    if (writebuf_add_data(wb, buffer, headerlen)!=0)
    {   free(buffer);
        return -1;
    }
    
    checksum=fletcher32(buffer, headerlen);
    temp32=cpu_to_le32(checksum);
    if (writebuf_add_data(wb, &temp32, sizeof(temp32))!=0)
    {   free(buffer);
        return -1;
    }
    
    free(buffer);
    msgprintf(MSG_DEBUG2, "end of archio_write_dico(wb=%p, dico=%p, magic=[%c%c%c%c])\n", wb, d, magic[0], magic[1], magic[2], magic[3]);
    
    return 0;
}

int writebuf_add_header(cwritebuf *wb, cdico *d, char *magic, u32 archid, u16 fsid)
{
    u16 temp16;
    u32 temp32;
    
    if (!wb || !d || !magic)
    {   errprintf("a parameter is null\n");
        return -1;
    }
    
    // A. write dico magic string
    if (writebuf_add_data(wb, magic, FSA_SIZEOF_MAGIC)!=0)
    {   errprintf("writebuf_add_data() failed to write FSA_SIZEOF_MAGIC\n");
        return -2;
    }
    
    // B. write archive id
    temp32=cpu_to_le32(archid);
    if (writebuf_add_data(wb, &temp32, sizeof(temp32))!=0)
    {   errprintf("writebuf_add_data() failed to write archid\n");
        return -3;
    }
    
    // C. write filesystem id
    temp16=cpu_to_le16(fsid);
    if (writebuf_add_data(wb, &temp16, sizeof(temp16))!=0)
    {   errprintf("writebuf_add_data() failed to write fsid\n");
        return -3;
    }
    
    // D. write the dico of the header
    if (writebuf_add_dico(wb, d, magic) != 0)
    {   errprintf("archio_write_dico() failed to write the header dico\n");
        return -4;
    }
    
    return 0;
}

int writebuf_add_block(cwritebuf *wb, struct s_blockinfo *blkinfo, u32 archid, u16 fsid)
{
    cdico *blkdico; // header written in file
    int res;
    
    if (!wb || !blkinfo)
    {   errprintf("a parameter is null\n");
        return -1;
    }
    
    if ((blkdico=dico_alloc())==NULL)
    {   errprintf("dico_alloc() failed\n");
        return -1;
    }
    
    if (blkinfo->blkarsize==0)
    {   errprintf("blkinfo->blkarsize=0: block is empty\n");
        return -1;
    }

    // prepare header
    dico_add_u64(blkdico, 0, BLOCKHEADITEMKEY_BLOCKOFFSET, blkinfo->blkoffset);
    dico_add_u32(blkdico, 0, BLOCKHEADITEMKEY_REALSIZE, blkinfo->blkrealsize);
    dico_add_u32(blkdico, 0, BLOCKHEADITEMKEY_ARSIZE, blkinfo->blkarsize);
    dico_add_u32(blkdico, 0, BLOCKHEADITEMKEY_COMPSIZE, blkinfo->blkcompsize);
    dico_add_u32(blkdico, 0, BLOCKHEADITEMKEY_ARCSUM, blkinfo->blkarcsum);
    dico_add_u16(blkdico, 0, BLOCKHEADITEMKEY_COMPRESSALGO, blkinfo->blkcompalgo);
    dico_add_u16(blkdico, 0, BLOCKHEADITEMKEY_ENCRYPTALGO, blkinfo->blkcryptalgo);
    
    // write block header
    res=writebuf_add_header(wb, blkdico, FSA_MAGIC_BLKH, archid, fsid);
    dico_destroy(blkdico);
    if (res!=0)
    {   msgprintf(MSG_STACK, "cannot write FSA_MAGIC_BLKH block-header\n");
        return -1;
    }
    
    // write block data
    if (writebuf_add_data(wb, blkinfo->blkdata, blkinfo->blkarsize)!=0)
    {   msgprintf(MSG_STACK, "cannot write data block: writebuf_add_data() failed\n");
        return -1;
    }
    
    return 0;
}
