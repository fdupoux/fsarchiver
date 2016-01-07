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

#include "fsarchiver.h"
#include "common.h"
#include "comp_lzma.h"
#include "error.h"

#ifdef OPTION_LZMA_SUPPORT

#include <lzma.h>

int compress_block_lzma(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
    lzma_stream lzma = LZMA_STREAM_INIT;
    int res;
    
    // init lzma structures
    lzma.next_in = origbuf;
    lzma.avail_in = origsize;
    lzma.next_out = compbuf;
    lzma.avail_out = compbufsize;
    
    // Initialize a coder to the lzma_stream
    if ((res=lzma_easy_encoder(&lzma, level, LZMA_CHECK_CRC32))!=LZMA_OK)
    {   switch (res)
        {
            case LZMA_MEM_ERROR:
                errprintf("lzma_easy_encoder(%d): LZMA compression failed "
                    "with an out of memory error.\nYou should use a lower "
                    "compression level to reduce the memory requirement.\n", level);
                lzma_end(&lzma);
                return FSAERR_ENOMEM;
            default:
                errprintf("lzma_easy_encoder(%d) failed with res=%d\n", level, res);
                lzma_end(&lzma);
                return FSAERR_UNKNOWN;
        }
    }
    
    if ((res=lzma_code(&lzma, LZMA_RUN))!=LZMA_OK)
    {   errprintf("lzma_code(LZMA_RUN) failed with res=%d\n", res);
        lzma_end(&lzma);
        return FSAERR_UNKNOWN;
    }
    
    if ((res=lzma_code(&lzma, LZMA_FINISH))!=LZMA_STREAM_END && res!=LZMA_OK)
    {   errprintf("lzma_code(LZMA_FINISH) failed with res=%d\n", res);
        lzma_end(&lzma);
        return FSAERR_UNKNOWN;
    }
    
    *compsize=(u64)(lzma.total_out);
    lzma_end(&lzma);
    return FSAERR_SUCCESS;
}

int uncompress_block_lzma(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    lzma_stream lzma = LZMA_STREAM_INIT;
    u64 maxmemlimit=3ULL*1024ULL*1024ULL*1024ULL;
    u64 memlimit=96*1024*1024;
    int res;
    
    // init lzma structures
    lzma.next_in = compbuf;
    lzma.avail_in = compsize;
    lzma.next_out = origbuf;
    lzma.avail_out = origbufsize;
    
    // Initialize a coder to the lzma_stream
    if ((res=lzma_auto_decoder(&lzma, memlimit, 0))!=LZMA_OK)
    {   errprintf("lzma_auto_decoder() failed with res=%d\n", res);
        lzma_end(&lzma);
        return FSAERR_UNKNOWN;
    }
    
    do // retry if lzma_code() returns LZMA_MEMLIMIT_ERROR (increase the memory limit)
    {   
        if ((res=lzma_code(&lzma, LZMA_RUN)) != LZMA_STREAM_END) // if error
        {
            if (res == LZMA_MEMLIMIT_ERROR) // we have to raise the memory limit
            {   memlimit+=64*1024*1024;
                lzma_memlimit_set(&lzma, memlimit);
                msgprintf(MSG_VERB2, "lzma_memlimit_set(%lld)\n", (long long)memlimit);
            }
            else // another error
            {   errprintf("lzma_code(LZMA_RUN) failed with res=%d\n", res);
                lzma_end(&lzma);
                return FSAERR_UNKNOWN;
            }
        }
    } while ((res == LZMA_MEMLIMIT_ERROR) && (memlimit < maxmemlimit));
    
    *origsize=(u64)(lzma.total_out);
    lzma_end(&lzma);
    
    switch (res)
    {
        case LZMA_STREAM_END:
            return FSAERR_SUCCESS;
        case LZMA_MEMLIMIT_ERROR:
            return FSAERR_ENOMEM;
        default:
            return FSAERR_UNKNOWN;
    }
}

#endif // OPTION_LZMA_SUPPORT
