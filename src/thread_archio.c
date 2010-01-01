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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "fsarchiver.h"
#include "writebuf.h"
#include "archive.h"
#include "common.h"
#include "options.h"
#include "dico.h"
#include "error.h"
#include "syncthread.h"

int archio_write_dico(struct s_writebuf *wb, cdico *d, char *magic)
{
    struct s_dicoitem *item;
    int itemnum;
    u16 headerlen;
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
    temp16=cpu_to_le16(headerlen);
    if (writebuf_add_data(wb, &temp16, sizeof(temp16))!=0)
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

int archio_write_header(struct s_writebuf *wb, cdico *d, char *magic, u32 archid, u16 fsid)
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
    if (archio_write_dico(wb, d, magic) != 0)
    {   errprintf("archio_write_dico() failed to write the header dico\n");
        return -4;
    }
    
    return 0;
}

int archio_write_block(struct s_writebuf *wb, struct s_blockinfo *blkinfo, u32 archid, u16 fsid)
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
    res=archio_write_header(wb, blkdico, FSA_MAGIC_BLKH, archid, fsid);
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

int archio_write_volheader(carchive *ai)
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
    if (archio_write_header(wb, voldico, FSA_MAGIC_VOLH, ai->archid, FSA_FILESYSID_NULL)!=0)
    {   errprintf("archio_write_header() failed\n");
        return -1;
    }
    
    // write header to file
    if (archive_write_data(ai, wb->data, wb->size)!=0)
    {   errprintf("archive_write_data(size=%ld) failed\n", (long)wb->size);
        return -1;
    }
    
    dico_destroy(voldico);
    writebuf_destroy(wb);
    
    return 0;
}

int archio_write_volfooter(carchive *ai, bool lastvol)
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
    if (archio_write_header(wb, voldico, FSA_MAGIC_VOLF, ai->archid, FSA_FILESYSID_NULL)!=0)
    {   msgprintf(MSG_STACK, "archio_write_header() failed\n");
        return -1;
    }
    
    // write header to file
    if (archive_write_data(ai, wb->data, wb->size)!=0)
    {   msgprintf(MSG_STACK, "archive_write_data(size=%ld) failed\n", (long)wb->size);
        return -1;
    }
    
    dico_destroy(voldico);
    writebuf_destroy(wb);
    
    return 0;
}

void *thread_writer_fct(void *args)
{
    struct s_headinfo headinfo;
    struct s_blockinfo blkinfo;
    struct s_writebuf *wb=NULL;
    carchive *ai=NULL;
    s64 cursize;
    s64 blknum;
    int type;
    
    // init
    inc_secthreads();
    
    if ((ai=(carchive *)args)==NULL)
    {   errprintf("ai is NULL\n");
        goto thread_writer_fct_error;
    }
    if (archive_volpath(ai)!=0)
    {   msgprintf(MSG_STACK, "archive_volpath() failed\n");
        goto thread_writer_fct_error;
    }
    if (archive_create(ai)!=0)
    {      msgprintf(MSG_STACK, "archive_create(%s) failed\n", ai->basepath);
        goto thread_writer_fct_error;
    }
    if (archio_write_volheader(ai)!=0)
    {      msgprintf(MSG_STACK, "cannot write volume header: archio_write_volheader() failed\n");
        goto thread_writer_fct_error;
    }
    
    while (queue_get_end_of_queue(&g_queue)==false)
    {   
        if ((blknum=queue_dequeue_first(&g_queue, &type, &headinfo, &blkinfo))<0 && blknum!=QERR_ENDOFQUEUE) // error
        {   msgprintf(MSG_STACK, "queue_dequeue_first()=%ld=%s failed\n", (long)blknum, qerr(blknum));
            goto thread_writer_fct_error;
        }
        else if (blknum>0) // block or header found
        {
            // a. allocate a writebuffer
            if ((wb=writebuf_alloc())==NULL)
            {   errprintf("writebuf_alloc() failed\n");
                goto thread_writer_fct_error;
            }
            
            // b. write header/block to s_writebuf
            switch (type)
            {
                case QITEM_TYPE_BLOCK:
                    if (archio_write_block(wb, &blkinfo, ai->archid, blkinfo.blkfsid)!=0)
                    {   msgprintf(MSG_STACK, "archio_write_block() failed\n");
                        goto thread_writer_fct_error;
                    }
                    free(blkinfo.blkdata);
                    break;
                case QITEM_TYPE_HEADER:
                    if (archio_write_header(wb, headinfo.dico, headinfo.magic, ai->archid, headinfo.fsid)!=0)
                    {   msgprintf(MSG_STACK, "archive_write_header() failed\n");
                        goto thread_writer_fct_error;
                    }
                    dico_destroy(headinfo.dico);
                    break;
                default:
                    errprintf("unexpected item type from queue: type=%d\n", type);
                    break;
            }
            
            // c. check for splitting
            if (((cursize=archive_get_currentpos(ai))>=0) && (g_options.splitsize>0 && cursize+wb->size > g_options.splitsize))
            {
                msgprintf(MSG_DEBUG4, "splitchk: YES --> cursize=%lld, g_options.splitsize=%lld, cursize+wb->size=%lld, wb->size=%lld\n",
                    (long long)cursize, (long long)g_options.splitsize, (long long)cursize+wb->size, (long long)wb->size);
                if (archio_write_volfooter(ai, false)!=0)
                {   msgprintf(MSG_STACK, "cannot write volume footer: archio_write_volfooter() failed\n");
                    goto thread_writer_fct_error;
                }
                archive_close(ai);
                archive_incvolume(ai, false);
                msgprintf(MSG_VERB2, "Creating new volume: [%s]\n", ai->volpath);
                if (archive_create(ai)!=0)
                {   msgprintf(MSG_STACK, "archive_create() failed\n");
                    goto thread_writer_fct_error;
                }
                if (archio_write_volheader(ai)!=0)
                {      msgprintf(MSG_STACK, "cannot write volume header: archio_write_volheader() failed\n");
                    goto thread_writer_fct_error;
                }
            }
            else // don't split now
            {
                msgprintf(MSG_DEBUG4, "splitchk: NO --> cursize=%lld, g_options.splitsize=%lld, cursize+wb->size=%lld, wb->size=%lld\n",
                    (long long)cursize, (long long)g_options.splitsize, (long long)cursize+wb->size, (long long)wb->size);
            }
            
            // d. write s_writebuf to disk
            if (archive_write_data(ai, wb->data, wb->size)!=0)
            {   msgprintf(MSG_STACK, "archive_write_data(size=%ld) failed\n", (long)wb->size);
                goto thread_writer_fct_error;
            }
            
            // e. free memory
            writebuf_destroy(wb);
        }
    }
    
    // write last volume footer
    if (archio_write_volfooter(ai, true)!=0)
    {      msgprintf(MSG_STACK, "cannot write volume footer: archio_write_volfooter() failed\n");
        goto thread_writer_fct_error;
    }
    archive_close(ai);
    msgprintf(MSG_DEBUG1, "THREAD-WRITER: exit success\n");
    dec_secthreads();
    return NULL;
    
thread_writer_fct_error:
    msgprintf(MSG_DEBUG1, "THREAD-WRITER: exit remove\n");
    set_stopfillqueue(); // say to the create.c thread that it must stop
    while (queue_get_end_of_queue(&g_queue)==false) // wait until all the compression threads exit
        queue_destroy_first_item(&g_queue); // empty queue
    archive_close(ai);
    dec_secthreads();
    return NULL;
}

// ================================================================================================================
// ================================================================================================================
// ================================================================================================================

int read_and_copy_block(carchive *ai, cdico *blkdico, int *sumok, bool skip)
{
    struct s_blockinfo blkinfo;
    u32 arblockcsumorig;
    u32 arblockcsumcalc;
    u32 curblocksize; // data size
    u64 blockoffset; // offset of the block in the file
    u16 compalgo; // compression algo used
    u16 cryptalgo; // encryption algo used
    u32 finalsize; // compressed  block size
    u32 compsize;
    u8 *buffer;
    s64 lres;
    
    // init
    *sumok=-1;
    
    if (dico_get_u64(blkdico, 0, BLOCKHEADITEMKEY_BLOCKOFFSET, &blockoffset)!=0)
    {   msgprintf(3, "cannot get blockoffset from block-header\n");
        return -1;
    }
    
    if (dico_get_u32(blkdico, 0, BLOCKHEADITEMKEY_REALSIZE, &curblocksize)!=0 || curblocksize>FSA_MAX_BLKSIZE)
    {   msgprintf(3, "cannot get blocksize from block-header\n");
        return -1;
    }
    
    if (dico_get_u16(blkdico, 0, BLOCKHEADITEMKEY_COMPRESSALGO, &compalgo)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_COMPRESSALGO from block-header\n");
        return -1;
    }

    if (dico_get_u16(blkdico, 0, BLOCKHEADITEMKEY_ENCRYPTALGO, &cryptalgo)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_ENCRYPTALGO from block-header\n");
        return -1;
    }

    if (dico_get_u32(blkdico, 0, BLOCKHEADITEMKEY_ARSIZE, &finalsize)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_ARSIZE from block-header\n");
        return -1;
    }
    
    if (dico_get_u32(blkdico, 0, BLOCKHEADITEMKEY_COMPSIZE, &compsize)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_COMPSIZE from block-header\n");
        return -1;
    }
    
    if (dico_get_u32(blkdico, 0, BLOCKHEADITEMKEY_ARCSUM, &arblockcsumorig)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_ARCSUM from block-header\n");
        return -1;
    }
    
    if (skip==true) // the main thread does not need that block (block belongs to a filesys we want to skip)
    {
        if (lseek64(ai->archfd, (long)finalsize, SEEK_CUR)<0)
        {   sysprintf("cannot skip block (finalsize=%ld) failed\n", (long)finalsize);
            return -1;
        }
        return 0;
    }
    
    // ---- allocate memory
    if ((buffer=malloc(finalsize))==NULL)
    {   errprintf("cannot allocate block: malloc(%d) failed\n", finalsize);
        return -1;
    }
    
    if (read(ai->archfd, buffer, (long)finalsize)!=(long)finalsize)
    {   sysprintf("cannot read block (finalsize=%ld) failed\n", (long)finalsize);
        free(buffer);
        return -1;
    }
    
    // prepare blkinfo for the queue
    memset(&blkinfo, 0, sizeof(blkinfo));
    blkinfo.blkdata=(char*)buffer;
    blkinfo.blkrealsize=curblocksize;
    blkinfo.blkoffset=blockoffset;
    blkinfo.blkarcsum=arblockcsumorig;
    blkinfo.blkcompalgo=compalgo;
    blkinfo.blkcryptalgo=cryptalgo;
    blkinfo.blkarsize=finalsize;
    blkinfo.blkcompsize=compsize;
    
    // ---- checksum
    arblockcsumcalc=fletcher32(buffer, finalsize);
    if (arblockcsumcalc!=arblockcsumorig) // bad checksum
    {
        errprintf("block is corrupt at offset=%ld, blksize=%ld\n", (long)blockoffset, (long)curblocksize);
        free(blkinfo.blkdata);
        if ((blkinfo.blkdata=malloc(curblocksize))==NULL)
        {   errprintf("cannot allocate block: malloc(%d) failed\n", curblocksize);
            return -1;
        }
        memset(blkinfo.blkdata, 0, curblocksize);
        *sumok=false;
        if ((lres=queue_add_block(&g_queue, &blkinfo, QITEM_STATUS_DONE))!=QERR_SUCCESS)
        {   errprintf("queue_add_block()=%ld=%s failed\n", (long)lres, qerr(lres));
            return -1;
        }
        // go to the beginning of the corrupted contents so that the next header is searched here
        if (lseek64(ai->archfd, -(long long)finalsize, SEEK_CUR)<0)
        {   errprintf("lseek64() failed\n");
        }
    }
    else // no corruption detected
    {   *sumok=true;
        if ((lres=queue_add_block(&g_queue, &blkinfo, QITEM_STATUS_TODO))!=QERR_SUCCESS)
        {   if (lres!=QERR_CLOSED)
                errprintf("queue_add_block()=%ld=%s failed\n", (long)lres, qerr(lres));
            return -1;
        }
    }
    
    return 0;
}

int archio_read_volheader(carchive *ai)
{
    char creatver[FSA_MAX_PROGVERLEN];
    char filefmt[FSA_MAX_FILEFMTLEN];
    char magic[FSA_SIZEOF_MAGIC];
    cdico *d;
    u32 volnum;
    u32 readid;
    u16 fsid;
    int res;
    int ret=0;
    
    // init
    memset(magic, 0, sizeof(magic));

    // ---- a. read header from archive file
    if ((res=archive_read_header(ai, magic, &d, false, &fsid))!=ERR_SUCCESS)
    {   errprintf("archive_read_header() failed to read the archive header\n");
        return -1;
    }
    
    // ---- b. check the magic is what we expected
    if (strncmp(magic, FSA_MAGIC_VOLH, FSA_SIZEOF_MAGIC)!=0)
    {   errprintf("magic is not what we expected: found=[%s] and expected=[%s]\n", magic, FSA_MAGIC_VOLH);
        ret=-1; goto archio_read_volheader_error;
    }
    
    if (dico_get_u32(d, 0, VOLUMEHEADKEY_ARCHID, &readid)!=0)
    {   errprintf("cannot get VOLUMEHEADKEY_ARCHID from the volume header\n");
        ret=-1; goto archio_read_volheader_error;
    }
    
    // ---- c. check the archive id
    if (ai->archid==0) // archid not know: this is the first volume
    {
        ai->archid=readid;
    }
    else if (readid!=ai->archid) // archid known: not the first volume
    {   errprintf("wrong header id: found=%.8x and expected=%.8x\n", readid, ai->archid);
        ret=-1; goto archio_read_volheader_error;
    }
    
    // ---- d. check the volnum
    if (dico_get_u32(d, 0, VOLUMEHEADKEY_VOLNUM, &volnum)!=0)
    {   errprintf("cannot get VOLUMEHEADKEY_VOLNUM from the volume header\n");
        ret=-1; goto archio_read_volheader_error;
    }
    if (volnum!=ai->curvol) // not the right volume number
    {   errprintf("wrong volume number in [%s]: volnum is %d and we need volnum %d\n", ai->volpath, (int)volnum, (int)ai->curvol);
        ret=-1; goto archio_read_volheader_error;
    }
    
    // ---- d. check the the file format
    if (dico_get_data(d, 0, VOLUMEHEADKEY_FILEFORMATVER, filefmt, FSA_MAX_FILEFMTLEN, NULL)!=0)
    {   errprintf("cannot find VOLUMEHEADKEY_FILEFORMATVER in main-header\n");
        ret=-1; goto archio_read_volheader_error;
    }
    
    if (ai->filefmt[0]==0) // filefmt not know: this is the first volume
    {
        memcpy(ai->filefmt, filefmt, FSA_MAX_FILEFMTLEN);
    }
    else if (strncmp(filefmt, ai->filefmt, FSA_MAX_FILEFMTLEN)!=0)
    {
        errprintf("This archive is based on a different file format: [%s]. Cannot continue.\n", ai->filefmt);
        errprintf("It has been created with fsarchiver [%s], you should extrat the archive using that version.\n", ai->creatver);
        errprintf("The current version of the program is [%s], and it's based on format [%s]\n", FSA_VERSION, FSA_FILEFORMAT);
        ret=-1; goto archio_read_volheader_error;
    }
    
    if (dico_get_data(d, 0, VOLUMEHEADKEY_PROGVERCREAT, creatver, FSA_MAX_PROGVERLEN, NULL)!=0)
    {   errprintf("cannot find VOLUMEHEADKEY_PROGVERCREAT in main-header\n");
        ret=-1; goto archio_read_volheader_error;
    }
    
    if (ai->creatver[0]==0)
        memcpy(ai->creatver, creatver, FSA_MAX_PROGVERLEN);
    
archio_read_volheader_error:
    dico_destroy(d);
    
    return ret;
}

void *thread_reader_fct(void *args)
{
    char magic[FSA_SIZEOF_MAGIC];
    u32 endofarchive=false;
    carchive *ai=NULL;
    cdico *d=NULL;
    u16 fsid;
    int sumok;
    u64 errors;
    s64 lres;
    int res;
    
    // init
    errors=0;
    inc_secthreads();

    if ((ai=(carchive *)args)==NULL)
    {   errprintf("ai is NULL\n");
        goto thread_reader_fct_error;
    }
    
    // open archive file
    if (archive_volpath(ai)!=0)
    {   errprintf("archive_volpath() failed\n");
        goto thread_reader_fct_error;
    }
    
    if (archive_open(ai)!=0)
    {      errprintf("archive_open(%s) failed\n", ai->basepath);
        goto thread_reader_fct_error;
    }
    
    // read volume header
    if (archio_read_volheader(ai)!=0)
    {      errprintf("archio_read_volheader() failed\n");
        goto thread_reader_fct_error;
    }
    
    // ---- read main archive header
    if ((res=archive_read_header(ai, magic, &d, false, &fsid))!=ERR_SUCCESS)
    {   errprintf("archive_read_header() failed to read the archive header\n");
        goto thread_reader_fct_error; // this header is required to continue
    }
    
    if (dico_get_u32(d, 0, MAINHEADKEY_ARCHIVEID, &ai->archid)!=0)
    {   msgprintf(3, "cannot get archive-id from main header\n");
        goto thread_reader_fct_error;
    }
    
    if ((lres=queue_add_header(&g_queue, d, magic, fsid))!=QERR_SUCCESS)
    {   errprintf("queue_add_header()=%ld=%s failed to add the archive header\n", (long)lres, qerr(lres));
        goto thread_reader_fct_error;
    }
    
    // read all other data from file (filesys-header, normal objects headers, ...)
    while (endofarchive==false && get_stopfillqueue()==false)
    {
        if ((res=archive_read_header(ai, magic, &d, true, &fsid))!=ERR_SUCCESS)
        {   dico_destroy(d);
            msgprintf(MSG_STACK, "archive_read_header() failed to read next header: curpos=%lld\n", (long long)archive_get_currentpos(ai));
            if (res==ERR_MINOR) // header is corrupt or not what we expected
            {   errors++;
                msgprintf(MSG_DEBUG1, "ERR_MINOR\n");
                continue;
            }
            else // fatal error (eg: cannot read archive from disk)
            {
                msgprintf(MSG_DEBUG1, "!ERR_MINOR\n");
                goto thread_reader_fct_error;
            }
        }
        
        // read header and see if it's for archive management or higher level data
        if (strncmp(magic, FSA_MAGIC_VOLF, FSA_SIZEOF_MAGIC)==0) // header is "end of volume"
        {
            archive_close(ai);
            
            // check the "end of archive" flag in header
            if (dico_get_u32(d, 0, VOLUMEFOOTKEY_LASTVOL, &endofarchive)!=0)
            {   errprintf("cannot get compr from block-header\n");
                goto thread_reader_fct_error;
            }
            msgprintf(MSG_VERB2, "End of volume [%s]\n", ai->volpath);
            if (endofarchive!=true)
            {
                archive_incvolume(ai, false);
                while (regfile_exists(ai->volpath)!=true)
                {
                    // wait until the queue is empty so that the main thread does not pollute the screen
                    while (queue_count(&g_queue)>0)
                        usleep(5000);
                    fflush(stdout);
                    fflush(stderr);
                    msgprintf(MSG_FORCE, "File [%s] is not found, please type the path to volume %ld:\n", ai->volpath, (long)ai->curvol);
                    fprintf(stdout, "New path:> ");
                    res=scanf("%s", ai->volpath);
                }
                
                msgprintf(MSG_VERB2, "New volume is [%s]\n", ai->volpath);
                if (archive_open(ai)!=0)
                {   msgprintf(MSG_STACK, "archive_open() failed\n");
                    goto thread_reader_fct_error;
                }
                if (archio_read_volheader(ai)!=0)
                {      msgprintf(MSG_STACK, "archio_read_volheader() failed\n");
                    goto thread_reader_fct_error;
                }
            }
            dico_destroy(d);
        }
        else // high-level archive (not involved in volume management)
        {
            if (strncmp(magic, FSA_MAGIC_BLKH, FSA_SIZEOF_MAGIC)==0) // header starts a data block
            {
                if (read_and_copy_block(ai, d, &sumok, g_fsbitmap[fsid]==0)!=0)
                {   msgprintf(MSG_STACK, "read_and_copy_blocks() failed\n");
                    goto thread_reader_fct_error;
                }
                
                if (sumok==false) errors++;
                dico_destroy(d);
            }
            else // another higher level header
            {
                // if it's a global header or a if this local header belongs to a filesystem that the main thread needs
                if (fsid==FSA_FILESYSID_NULL || g_fsbitmap[fsid]==1)
                {
                    if ((lres=queue_add_header(&g_queue, d, magic, fsid))!=QERR_SUCCESS)
                    {   msgprintf(MSG_STACK, "queue_add_header()=%ld=%s failed\n", (long)lres, qerr(lres));
                        goto thread_reader_fct_error;
                    }
                }
                else // header not used: remove data strucutre in dynamic memory
                {
                    dico_destroy(d);
                }
            }
        }
    }
    
thread_reader_fct_error:
    msgprintf(MSG_DEBUG1, "THREAD-READER: queue_set_end_of_queue(&g_queue, true)\n");
    queue_set_end_of_queue(&g_queue, true); // don't wait for more data from this thread
        // wait until the queue is empty before that thread exits
    /*while (queue_get_end_of_queue(&g_queue)==false)
        {      //errprintf("THREADIO: queue_get_end_of_queue(): %d\n", queue_get_end_of_queue(&g_queue));
                usleep(10000);
        }*/
    dec_secthreads();
    msgprintf(MSG_DEBUG1, "THREAD-READER: exit\n");
    return NULL;
}
