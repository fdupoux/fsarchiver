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
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <pthread.h>

#include "fsarchiver.h"
#include "common.h"
#include "options.h"
#include "comp_gzip.h"
#include "comp_bzip2.h"
#include "comp_lzma.h"
#include "comp_lzo.h"
#include "crypto.h"
#include "syncthread.h"
#include "thread_comp.h"
#include "error.h"
#include "queue.h"

int compress_block_generic(struct s_blockinfo *blkinfo)
{
    char *bufcomp=NULL;
    int attempt=0;
    int compalgo;
    int complevel;
    u64 compsize;
    u64 bufsize;
    int res;
    
    bufsize = (blkinfo->blkrealsize) + (blkinfo->blkrealsize / 16) + 64 + 3; // alloc bigger buffer else lzo will crash
    if ((bufcomp=malloc(bufsize))==NULL)
    {   errprintf("malloc(%ld) failed: out of memory\n", (long)bufsize);
        return -1;
    }
    
    // compression level/algo to use for the first attempt
    compalgo=g_options.compressalgo;
    complevel=g_options.compresslevel;
    
    // compress the block
    do
    {
        switch (compalgo)
        {
#ifdef OPTION_LZO_SUPPORT
            case COMPRESS_LZO:
                res=compress_block_lzo(blkinfo->blkrealsize, &compsize, (u8*)blkinfo->blkdata, (void*)bufcomp, bufsize, complevel);
                blkinfo->blkcompalgo=COMPRESS_LZO;
                break;
#endif // OPTION_LZO_SUPPORT
            case COMPRESS_GZIP:
                res=compress_block_gzip(blkinfo->blkrealsize, &compsize, (u8*)blkinfo->blkdata, (void*)bufcomp, bufsize, complevel);
                blkinfo->blkcompalgo=COMPRESS_GZIP;
                break;
            case COMPRESS_BZIP2:
                res=compress_block_bzip2(blkinfo->blkrealsize, &compsize, (u8*)blkinfo->blkdata, (void*)bufcomp, bufsize, complevel);
                blkinfo->blkcompalgo=COMPRESS_BZIP2;
                break;
#ifdef OPTION_LZMA_SUPPORT
            case COMPRESS_LZMA:
                res=compress_block_lzma(blkinfo->blkrealsize, &compsize, (u8*)blkinfo->blkdata, (void*)bufcomp, bufsize, complevel);
                blkinfo->blkcompalgo=COMPRESS_LZMA;
                break;
#endif // OPTION_LZMA_SUPPORT
            default:
                free(bufcomp);
                msgprintf(2, "invalid compression level: %d\n", (int)compalgo);
                return -1;
        }
        
        // retry if high compression was used and compression failed because of FSAERR_ENOMEM
        if ((res == FSAERR_ENOMEM) && (compalgo > FSA_DEF_COMPRESS_ALGO))
        {
            errprintf("attempt to compress the current block using an alternative algorithm (\"-z%d\")\n", FSA_DEF_COMPRESS_ALGO);
            compalgo = FSA_DEF_COMPRESS_ALGO;
            complevel = FSA_DEF_COMPRESS_LEVEL;
        }
        
    } while ((res == FSAERR_ENOMEM) && (attempt++ == 0));
    
    // check compression status and efficiency
    if ((res==FSAERR_SUCCESS) && (compsize < blkinfo->blkrealsize)) // compression worked and saved space
    {   free(blkinfo->blkdata); // free old buffer (with uncompressed data)
        blkinfo->blkdata=bufcomp; // new buffer (with compressed data)
        blkinfo->blkcompsize=compsize; // size after compression and before encryption
        blkinfo->blkarsize=compsize; // in case there is no encryption to set this
        //errprintf ("COMP_DBG: block successfully compressed using %s\n", compress_algo_int_to_string(compalgo));
    }
    else // compressed version is bigger or compression failed: keep the original block
    {   memcpy(bufcomp, blkinfo->blkdata, blkinfo->blkrealsize);
        free(blkinfo->blkdata); // free old buffer
        blkinfo->blkdata=bufcomp; // new buffer
        blkinfo->blkcompsize=blkinfo->blkrealsize; // size after compression and before encryption
        blkinfo->blkarsize=blkinfo->blkrealsize;  // in case there is no encryption to set this
        blkinfo->blkcompalgo=COMPRESS_NONE;
        //errprintf ("COMP_DBG: block copied uncompressed, attempted using %s\n", compress_algo_int_to_string(compalgo));
    }
    
    u64 cryptsize;
    char *bufcrypt=NULL;
    if (g_options.encryptalgo==ENCRYPT_BLOWFISH)
    {
        if ((bufcrypt=malloc(bufsize+8))==NULL)
        {   errprintf("malloc(%ld) failed: out of memory\n", (long)bufsize+8);
            return -1;
        }
        if ((res=crypto_blowfish(blkinfo->blkcompsize, &cryptsize, (u8*)bufcomp, (u8*)bufcrypt, 
            g_options.encryptpass, strlen((char*)g_options.encryptpass), 1))!=0)
        {   errprintf("crypt_block_blowfish() failed with res=%d\n", res);
            return -1;
        }
        free(bufcomp);
        blkinfo->blkdata=bufcrypt;
        blkinfo->blkarsize=cryptsize;
        blkinfo->blkcryptalgo=ENCRYPT_BLOWFISH;
    }
    else
    {
        blkinfo->blkcryptalgo=ENCRYPT_NONE;
    }
    
    // calculates the final block checksum (block as it will be stored in the archive)
    blkinfo->blkarcsum=fletcher32((void*)blkinfo->blkdata, blkinfo->blkarsize);
    
    return 0;
}

int decompress_block_generic(struct s_blockinfo *blkinfo)
{
    u64 checkorigsize;
    char *bufcomp=NULL;
    int res;
    
    // allocate memory for uncompressed data
    if ((bufcomp=malloc(blkinfo->blkrealsize))==NULL)
    {   errprintf("malloc(%ld) failed: cannot allocate memory for compressed block\n", (long)blkinfo->blkrealsize);
        return -1;
    }
    
    // check the block checksum
    if (fletcher32((u8*)blkinfo->blkdata, blkinfo->blkarsize)!=(blkinfo->blkarcsum))
    {   errprintf("block is corrupt at blockoffset=%ld, blksize=%ld\n", (long)blkinfo->blkoffset, (long)blkinfo->blkrealsize);
        memset(bufcomp, 0, blkinfo->blkrealsize);
    }
    else // data not corrupted, decompresses the block
    {
        if ((blkinfo->blkcryptalgo!=ENCRYPT_NONE) && (g_options.encryptalgo!=ENCRYPT_BLOWFISH))
        {   msgprintf(MSG_DEBUG1, "this archive has been encrypted, you have to provide a password "
                "on the command line using option '-c'\n");
            free (bufcomp);
            return -1;
        }
        
        char *bufcrypt=NULL;
        u64 clearsize;
        if (blkinfo->blkcryptalgo==ENCRYPT_BLOWFISH)
        {
            if ((bufcrypt=malloc(blkinfo->blkrealsize+8))==NULL)
            {   errprintf("malloc(%ld) failed: out of memory\n", (long)blkinfo->blkrealsize+8);
                free(bufcomp);
                return -1;
            }
            if ((res=crypto_blowfish(blkinfo->blkarsize, &clearsize, (u8*)blkinfo->blkdata, (u8*)bufcrypt, 
                g_options.encryptpass, strlen((char*)g_options.encryptpass), 0))!=0)
            {   errprintf("crypt_block_blowfish() failed\n");
                free(bufcomp);
                return -1;
            }
            if (clearsize!=blkinfo->blkcompsize)
            {   errprintf("clearsize does not match blkcompsize: clearsize=%ld and blkcompsize=%ld\n", 
                    (long)clearsize, (long)blkinfo->blkcompsize);
                free(bufcomp);
                return -1;
            }
            free(blkinfo->blkdata);
            blkinfo->blkdata=bufcrypt;
        }
        
        switch (blkinfo->blkcompalgo)
        {
            case COMPRESS_NONE:
                memcpy(bufcomp, blkinfo->blkdata, blkinfo->blkarsize);
                res=0;
                break;
#ifdef OPTION_LZO_SUPPORT
            case COMPRESS_LZO:
                if ((res=uncompress_block_lzo(blkinfo->blkcompsize, &checkorigsize, (void*)bufcomp, blkinfo->blkrealsize, (u8*)blkinfo->blkdata))!=0)
                {   errprintf("uncompress_block_lzo()=%d failed: finalsize=%ld and checkorigsize=%ld\n", 
                        res, (long)blkinfo->blkarsize, (long)checkorigsize);
                    memset(bufcomp, 0, blkinfo->blkrealsize);
                    // TODO: inc(error_counter);
                }
                break;
#endif // OPTION_LZO_SUPPORT
            case COMPRESS_GZIP:
                if ((res=uncompress_block_gzip(blkinfo->blkcompsize, &checkorigsize, (void*)bufcomp, blkinfo->blkrealsize, (u8*)blkinfo->blkdata))!=0)
                {   errprintf("uncompress_block_gzip()=%d failed: finalsize=%ld and checkorigsize=%ld\n", 
                        res, (long)blkinfo->blkarsize, (long)checkorigsize);
                    memset(bufcomp, 0, blkinfo->blkrealsize);
                    // TODO: inc(error_counter);
                }
                break;
            case COMPRESS_BZIP2:
                if ((res=uncompress_block_bzip2(blkinfo->blkcompsize, &checkorigsize, (void*)bufcomp, blkinfo->blkrealsize, (u8*)blkinfo->blkdata))!=0)
                {   errprintf("uncompress_block_bzip2()=%d failed: finalsize=%ld and checkorigsize=%ld\n", 
                        res, (long)blkinfo->blkarsize, (long)checkorigsize);
                    memset(bufcomp, 0, blkinfo->blkrealsize);
                    // TODO: inc(error_counter);
                }
                break;
#ifdef OPTION_LZMA_SUPPORT
            case COMPRESS_LZMA:
                if ((res=uncompress_block_lzma(blkinfo->blkcompsize, &checkorigsize, (void*)bufcomp, blkinfo->blkrealsize, (u8*)blkinfo->blkdata))!=0)
                {   errprintf("uncompress_block_lzma()=%d failed: finalsize=%ld and checkorigsize=%ld\n", 
                        res, (long)blkinfo->blkarsize, (long)checkorigsize);
                    memset(bufcomp, 0, blkinfo->blkrealsize);
                    // TODO: inc(error_counter);
                }
                break;
#endif // OPTION_LZMA_SUPPORT
            default:
                errprintf("unsupported compression algorithm: %ld\n", (long)blkinfo->blkcompalgo);
                return -1;
        }
        free(blkinfo->blkdata); // free old buffer (with compressed data)
        blkinfo->blkdata=bufcomp; // pointer to new buffer with uncompressed data
    }
    return 0;
}

int compression_function(int oper)
{
    struct s_blockinfo blkinfo;
    s64 blknum;
    int res;
    
    while (queue_get_end_of_queue(&g_queue)==false)
    {
        if ((blknum=queue_get_first_block_todo(&g_queue, &blkinfo))>0) // block found
        {
            switch (oper)
            {
                case COMPTHR_COMPRESS:
                    res=compress_block_generic(&blkinfo);
                    break;
                case COMPTHR_DECOMPRESS:
                    res=decompress_block_generic(&blkinfo);
                    break;
                default:
                    errprintf("oper is invalid: %d\n", oper);
                    goto thread_comp_fct_error;
            }
            if (res!=0)
            {   msgprintf(MSG_STACK, "compress_block()=%d failed\n", res);
                goto thread_comp_fct_error;
            }
            // don't check for errors: it's normal to fail when we terminate after a problem
            queue_replace_block(&g_queue, blknum, &blkinfo, QITEM_STATUS_DONE);
        }
    }
    
    msgprintf(MSG_DEBUG1, "THREAD-COMP: exit success\n");
    return 0;
    
thread_comp_fct_error:
    get_stopfillqueue();
    msgprintf(MSG_DEBUG1, "THREAD-COMP: exit error\n");
    return 0;
}

void *thread_comp_fct(void *args)
{
    inc_secthreads();
    compression_function(COMPTHR_COMPRESS);
    dec_secthreads();
    return NULL;
}

void *thread_decomp_fct(void *args)
{
    inc_secthreads();
    compression_function(COMPTHR_DECOMPRESS);
    dec_secthreads();
    return NULL;
}
