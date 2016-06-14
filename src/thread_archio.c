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
#include <unistd.h>
#include <string.h>

#include "fsarchiver.h"
#include "archreader.h"
#include "archwriter.h"
#include "dico.h"
#include "common.h"
#include "error.h"
#include "syncthread.h"
#include "queue.h"

void *thread_writer_fct(void *args)
{
    struct s_headinfo headinfo;
    struct s_blockinfo blkinfo;
    carchwriter *ai=NULL;
    s64 blknum;
    int type;
    
    // init
    inc_secthreads();
    
    if ((ai=(carchwriter *)args)==NULL)
    {   errprintf("ai is NULL\n");
        goto thread_writer_fct_error;
    }
    if (archwriter_volpath(ai)!=0)
    {   msgprintf(MSG_STACK, "archwriter_volpath() failed\n");
        goto thread_writer_fct_error;
    }
    if (archwriter_create(ai)!=0)
    {   msgprintf(MSG_STACK, "archwriter_create(%s) failed\n", ai->basepath);
        goto thread_writer_fct_error;
    }
    if (archwriter_write_volheader(ai)!=0)
    {   msgprintf(MSG_STACK, "cannot write volume header: archwriter_write_volheader() failed\n");
        goto thread_writer_fct_error;
    }
    
    while (queue_get_end_of_queue(&g_queue)==false)
    {
        if ((blknum=queue_dequeue_first(&g_queue, &type, &headinfo, &blkinfo))<0 && blknum!=FSAERR_ENDOFFILE) // error
        {   msgprintf(MSG_STACK, "queue_dequeue_first()=%ld=%s failed\n", (long)blknum, error_int_to_string(blknum));
            goto thread_writer_fct_error;
        }
        else if (blknum>0) // block or header found
        {
            switch (type)
            {
                case QITEM_TYPE_BLOCK:
                    if (archwriter_dowrite_block(ai, &blkinfo)!=0)
                    {   msgprintf(MSG_STACK, "archive_dowrite_block() failed\n");
                        goto thread_writer_fct_error;
                    }
                    free(blkinfo.blkdata);
                    break;
                case QITEM_TYPE_HEADER:
                    if (archwriter_dowrite_header(ai, &headinfo)!=0)
                    {   msgprintf(MSG_STACK, "archive_write_header() failed\n");
                        goto thread_writer_fct_error;
                    }
                    dico_destroy(headinfo.dico);
                    break;
                default:
                    errprintf("unexpected item type from queue: type=%d\n", type);
                    break;
            }
        }
    }
    
    // write last volume footer
    if (archwriter_write_volfooter(ai, true)!=0)
    {   msgprintf(MSG_STACK, "cannot write volume footer: archio_write_volfooter() failed\n");
        goto thread_writer_fct_error;
    }
    archwriter_close(ai);
    msgprintf(MSG_DEBUG1, "THREAD-WRITER: exit success\n");
    dec_secthreads();
    return NULL;
    
thread_writer_fct_error:
    msgprintf(MSG_DEBUG1, "THREAD-WRITER: exit remove\n");
    set_stopfillqueue(); // say to the create.c thread that it must stop
    while (queue_get_end_of_queue(&g_queue)==false) // wait until all the compression threads exit
        queue_destroy_first_item(&g_queue); // empty queue
    archwriter_close(ai);
    dec_secthreads();
    return NULL;
}

void *thread_reader_fct(void *args)
{
    char magic[FSA_SIZEOF_MAGIC];
    struct s_blockinfo blkinfo;
    u32 endofarchive=false;
    carchreader *ai=NULL;
    cdico *dico=NULL;
    int skipblock;
    u16 fsid;
    int sumok;
    int status;
    u64 errors;
    s64 lres;
    int res;
    
    // init
    errors=0;
    inc_secthreads();

    if ((ai=(carchreader *)args)==NULL)
    {   errprintf("ai is NULL\n");
        goto thread_reader_fct_error;
    }
    
    // open archive file
    if (archreader_volpath(ai)!=0)
    {   errprintf("archreader_volpath() failed\n");
        goto thread_reader_fct_error;
    }
    
    if (archreader_open(ai)!=0)
    {   errprintf("archreader_open(%s) failed\n", ai->basepath);
        goto thread_reader_fct_error;
    }
    
    // read volume header
    if (archreader_read_volheader(ai)!=0)
    {   errprintf("archio_read_volheader() failed\n");
        goto thread_reader_fct_error;
    }
    
    // ---- read main archive header
    if ((res=archreader_read_header(ai, magic, &dico, false, &fsid))!=FSAERR_SUCCESS)
    {   errprintf("archreader_read_header() failed to read the archive header\n");
        goto thread_reader_fct_error; // this header is required to continue
    }
    
    if (dico_get_u32(dico, 0, MAINHEADKEY_ARCHIVEID, &ai->archid)!=0)
    {   msgprintf(3, "cannot get archive-id from main header\n");
        goto thread_reader_fct_error;
    }
    
    if ((lres=queue_add_header(&g_queue, dico, magic, fsid))!=FSAERR_SUCCESS)
    {   errprintf("queue_add_header()=%ld=%s failed to add the archive header\n", (long)lres, error_int_to_string(lres));
        goto thread_reader_fct_error;
    }
    
    // read all other data from file (filesys-header, normal objects headers, ...)
    while (endofarchive==false && get_stopfillqueue()==false)
    {
        if ((res=archreader_read_header(ai, magic, &dico, true, &fsid))!=FSAERR_SUCCESS)
        {   dico_destroy(dico);
            msgprintf(MSG_STACK, "archreader_read_header() failed to read next header\n");
            if (res==OLDERR_MINOR) // header is corrupt or not what we expected
            {   errors++;
                msgprintf(MSG_DEBUG1, "OLDERR_MINOR\n");
                continue;
            }
            else // fatal error (eg: cannot read archive from disk)
            {
                msgprintf(MSG_DEBUG1, "!OLDERR_MINOR\n");
                goto thread_reader_fct_error;
            }
        }
        
        // read header and see if it's for archive management or higher level data
        if (strncmp(magic, FSA_MAGIC_VOLF, FSA_SIZEOF_MAGIC)==0) // header is "end of volume"
        {
            archreader_close(ai);
            
            // check the "end of archive" flag in header
            if (dico_get_u32(dico, 0, VOLUMEFOOTKEY_LASTVOL, &endofarchive)!=0)
            {   errprintf("cannot get compr from block-header\n");
                goto thread_reader_fct_error;
            }
            msgprintf(MSG_VERB2, "End of volume [%s]\n", ai->volpath);
            if (endofarchive!=true)
            {
                archreader_incvolume(ai, false);
                while (regfile_exists(ai->volpath)!=true)
                {
                    // wait until the queue is empty so that the main thread does not pollute the screen
                    while (queue_count(&g_queue)>0)
                        usleep(5000);
                    fflush(stdout);
                    fflush(stderr);
                    msgprintf(MSG_FORCE, "File [%s] is not found, please type the path to volume %ld:\n", ai->volpath, (long)ai->curvol);
                    fprintf(stdout, "New path:> ");
                    res=scanf("%256s", ai->volpath);
                }
                
                msgprintf(MSG_VERB2, "New volume is [%s]\n", ai->volpath);
                if (archreader_open(ai)!=0)
                {   msgprintf(MSG_STACK, "archreader_open() failed\n");
                    goto thread_reader_fct_error;
                }
                if (archreader_read_volheader(ai)!=0)
                {      msgprintf(MSG_STACK, "archio_read_volheader() failed\n");
                    goto thread_reader_fct_error;
                }
            }
            dico_destroy(dico);
        }
        else // high-level archive (not involved in volume management)
        {
            if (strncmp(magic, FSA_MAGIC_BLKH, FSA_SIZEOF_MAGIC)==0) // header starts a data block
            {
                skipblock=(g_fsbitmap[fsid]==0);
                //errprintf("DEBUG: skipblock=%d g_fsbitmap[fsid=%d]=%d\n", skipblock, (int)fsid, (int)g_fsbitmap[fsid]);
                if (archreader_read_block(ai, dico, skipblock, &sumok, &blkinfo)!=0)
                {   msgprintf(MSG_STACK, "archreader_read_block() failed\n");
                    goto thread_reader_fct_error;
                }
                
                if (skipblock==false)
                {
                    status=((sumok==true)?QITEM_STATUS_TODO:QITEM_STATUS_DONE);
                    if ((lres=queue_add_block(&g_queue, &blkinfo, status))!=FSAERR_SUCCESS)
                    {   if (lres!=FSAERR_NOTOPEN)
                            errprintf("queue_add_block()=%ld=%s failed\n", (long)lres, error_int_to_string(lres));
                        goto thread_reader_fct_error;
                    }
                    if (sumok==false) errors++;
                    dico_destroy(dico);
                }
            }
            else // another higher level header
            {
                // if it's a global header or a if this local header belongs to a filesystem that the main thread needs
                if (fsid==FSA_FILESYSID_NULL || g_fsbitmap[fsid]==1)
                {
                    if ((lres=queue_add_header(&g_queue, dico, magic, fsid))!=FSAERR_SUCCESS)
                    {   msgprintf(MSG_STACK, "queue_add_header()=%ld=%s failed\n", (long)lres, error_int_to_string(lres));
                        goto thread_reader_fct_error;
                    }
                }
                else // header not used: remove data strucutre in dynamic memory
                {
                    dico_destroy(dico);
                }
            }
        }
    }
    
thread_reader_fct_error:
    msgprintf(MSG_DEBUG1, "THREAD-READER: queue_set_end_of_queue(&g_queue, true)\n");
    queue_set_end_of_queue(&g_queue, true); // don't wait for more data from this thread
    dec_secthreads();
    msgprintf(MSG_DEBUG1, "THREAD-READER: exit\n");
    return NULL;
}
