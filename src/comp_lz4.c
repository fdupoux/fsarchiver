
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

#define LZ4_VERSION (LZ4_VERSION_MAJOR*10 + LZ4_VERSION_MINOR)
#if LZ4_VERSION >= 17
    res=LZ4_compress_default((const char*)origbuf, (char*)compbuf, (int)origsize, destsize);
    if (res==0){
        errprintf("LZ4_compress_default(): failed.\n");
        return FSAERR_UNKNOWN;
    }
#else
    res=LZ4_compress((const char*)origbuf, (char*)compbuf, (int)origsize);
    if (res==0){
        errprintf("LZ4_compress(): failed.\n");
        return FSAERR_UNKNOWN;
    }
#endif // LZ4_VERSION
    if (res > 0){
        *compsize=(u64)res;
        return FSAERR_SUCCESS;
    }
    
    return FSAERR_UNKNOWN;
}

int uncompress_block_lz4(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    int destsize=origbufsize;
    int res;

    if((res=LZ4_decompress_safe((char*)compbuf, (char*)origbuf, compsize, destsize)) > 0){
        *origsize=(u64)res;
        return FSAERR_SUCCESS;
    }

    errprintf("LZ4_decompress_safe() failed, res=%d\n", res);
    
    return FSAERR_UNKNOWN;
}
#endif //OPTION_LZ4_SUPPORT
