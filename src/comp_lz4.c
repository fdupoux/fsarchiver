
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

#include "fsarchiver.h"
#include "common.h"
#include "comp_lz4.h"
#include "error.h"


#ifdef OPTION_LZ4_SUPPORT
int compress_block_lz4(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
    int destsize=compbufsize;

    int res;
#define LZ4_VERSION (LZ$_VERSION_MAYOR*10 + LZ4_VERSION_MINOR)
#if LZ4_VERSION >= 17
    switch (res=LZ4_compress_default((const char*)compbuf, (char*)origbuf, (int)origsize, destsize))
    {
        if(res == 0){
            errprintf("LZ4_compress_default(): LZ4 compression failed "
                "with an out of memory error.\nYou should use a lower "
                "compression level to reduce the memory requirement.\n");
            return FSAERR_ENOMEM;
	}
    }

#else
    switch (res=LZ4_compress((const char*)origbuf, (char*)compbuf, (int)origsize))
    {
        if (res == 0){
            errprintf("LZ4_compress: LZ4 compression failed "
                "with an out of memory error.\nYou should use a lower "
                "compression level to reduce the memory requirement.\n");
            return FSAERR_ENOMEM;
	}
    }
#endif // LZ4_VERSION_MINOR
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

    if(res < 0){
        errprintf("LZ4_decompress_safe: LZ4 decompression failed"
            "destination buffer is not large enough"
            "or the source stream is detected malformed.\n");
        return FSAERR_ENOMEM;
    }

    errprintf("LZ4_decompress_safe() failed, res=%d\n", res);
    
    return FSAERR_UNKNOWN;
}
#endif //OPTION_LZ4_SUPPORT
