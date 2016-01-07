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

#include <bzlib.h>

#include "fsarchiver.h"
#include "common.h"
#include "comp_bzip2.h"
#include "error.h"

int compress_block_bzip2(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
    unsigned int destsize=compbufsize;
    
    switch (BZ2_bzBuffToBuffCompress((char*)compbuf, &destsize, (char*)origbuf, origsize, 9, 0, 30))
    {
        case BZ_OK:
            *compsize=(u64)destsize;
            return FSAERR_SUCCESS;
        case BZ_MEM_ERROR:
            errprintf("BZ2_bzBuffToBuffCompress(): BZIP2 compression failed "
                "with an out of memory error.\nYou should use a lower "
                "compression level to reduce the memory requirement.\n");
            return FSAERR_ENOMEM;
        default:
            return FSAERR_UNKNOWN;
    }
    
    return FSAERR_UNKNOWN;
}

int uncompress_block_bzip2(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    unsigned int destsize=origbufsize;
    int res;
    
    switch ((res=BZ2_bzBuffToBuffDecompress((char*)origbuf, &destsize, (char*)compbuf, compsize, 0, 0)))
    {
        case BZ_OK:
            *origsize=(u64)destsize;
            return FSAERR_SUCCESS;
        default:
            errprintf("BZ2_bzBuffToBuffDecompress() failed, res=%d\n", res);
            return FSAERR_UNKNOWN;
    }
    
    return FSAERR_UNKNOWN;
}
