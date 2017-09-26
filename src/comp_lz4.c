
/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2017 Cristian Vazquez.  All rights reserved.
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

#include <lz4.h>

#include "fsarchiver.h"
#include "common.h"
#include "comp_lz4.h"
#include "error.h"



int compress_block_lz4(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
    int destsize=compbufsize;

    int res;
    
    switch (res=LZ4_compress_default((const char*)compbuf, (char*)origbuf, (int)origsize, destsize))
    {
        case 0:
            errprintf("LZ4_compress_default(): LZ4 compression failed "
                "with an out of memory error.\nYou should use a lower "
                "compression level to reduce the memory requirement.\n");
            return FSAERR_ENOMEM;
    }


    if (res > 0){
	    *compsize=(u64)destsize;
            return FSAERR_SUCCESS;
    }
    
    return FSAERR_UNKNOWN;
}

int uncompress_block_lz4(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    int destsize=origbufsize;

    int res;
    
    if((res=LZ4_decompress_safe((char*)compbuf, (char*)origbuf, (int)*origsize, destsize)) > 0)
    {
            *origsize=(u64)destsize;
            return FSAERR_SUCCESS;
    }


    errprintf("BZ2_bzBuffToBuffDecompress() failed, res=%d\n", res);
    
    return FSAERR_UNKNOWN;
}
